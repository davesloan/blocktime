// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_replay.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <unordered_set>

#include "event_log.h"
#include "telemetry_json.h"

namespace blocktime {

namespace {

constexpr std::int64_t kLeadInMs = 15'000;  // how long before the first event the replay starts (D-178)

std::string dayOf(std::string_view flightId) { return std::string(flightId.substr(0, 10)); }

std::int64_t ms(std::chrono::system_clock::duration d) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

// "H:MM" -> minutes; nullopt otherwise.
std::optional<int> hoursMinutes(std::string_view at) {
    const auto colon = at.find(':');
    if (colon == std::string_view::npos || at.size() - colon != 3) return std::nullopt;
    int h = 0, m = 0;
    const auto hp = std::from_chars(at.data(), at.data() + colon, h);
    const auto mp = std::from_chars(at.data() + colon + 1, at.data() + at.size(), m);
    if (hp.ec != std::errc{} || hp.ptr != at.data() + colon || mp.ec != std::errc{} || mp.ptr != at.data() + at.size() ||
        m >= 60 || h < 0) {
        return std::nullopt;
    }
    return h * 60 + m;
}

}  // namespace

std::vector<std::filesystem::path> replayLogFiles(const std::filesystem::path& dir, std::string_view flightId) {
    const auto out = parseUtc(flightId);
    if (!out) return {};
    const std::string from = formatUtc(*out - std::chrono::hours(24)).substr(0, 10);
    std::vector<std::filesystem::path> files = dailyLogFiles(dir);
    std::erase_if(files, [&](const std::filesystem::path& f) {
        return f.filename().string().substr(7, 10) < from;  // events-YYYY-MM-DD.jsonl
    });
    return files;
}

bool replayLogAvailable(const std::filesystem::path& dir, std::string_view flightId) {
    std::error_code ec;
    return std::filesystem::exists(dir / ("events-" + dayOf(flightId) + ".jsonl"), ec);
}

ReplayRoute replayRoute(std::string_view type) {
    static const std::unordered_set<std::string_view> kLogOnly = {
        "cockpit",          "telemetry_invalid", "paused",           "unpaused",
        "pmdg_data",        "recorder_started",  "recorder_stopped", "sim_disconnected",
        "sim_responsive",   "sim_unresponsive",  "simconnect_exception", "warning",
        "aircraft_loaded",  "block_out_refused", "airport_runways",  "flight_interrupted", "sim_connected"};
    if (type == "telemetry") return ReplayRoute::Sample;
    if (type == "block_out" || type == "block_in") return ReplayRoute::Block;
    if (type.starts_with("probe_") || kLogOnly.contains(type)) return ReplayRoute::Skip;
    return ReplayRoute::Flight;
}

ReplayTimeline::ReplayTimeline(std::vector<LogLine> lines) {
    if (lines.empty()) return;
    start_ = lines.front().ts;
    std::optional<TimePoint> pausedSince;
    std::chrono::system_clock::duration pausedTotal{};
    bool blockedOut = false;
    for (LogLine& l : lines) {
        if (l.type == "paused") {
            if (!pausedSince) pausedSince = l.ts;
            continue;
        }
        // A session boundary also ends a pause: after a reconnect the recorder logs no `unpaused` for one from before.
        const bool boundary = l.type == "sim_connected" || l.type == "sim_disconnected" || l.type == "recorder_started";
        if (l.type == "unpaused" || boundary) {
            if (pausedSince) {
                pauses_.push_back({ms(*pausedSince - start_ - pausedTotal), l.ts - *pausedSince});
                pausedTotal += l.ts - *pausedSince;
            }
            pausedSince.reset();
            continue;
        }
        const ReplayRoute route = replayRoute(l.type);
        if (route == ReplayRoute::Skip) continue;
        if (l.type == "block_out") blockedOut = true;
        // Deboarding before block out is the previous flight's (D-137): Live showed it on that flight's Arrival.
        if (!blockedOut && l.type == "gsx_service" && l.data.is_object() && l.data.value("service", "") == "deboarding") {
            continue;
        }
        ReplayItem item;
        item.offsetMs = ms((pausedSince ? *pausedSince : l.ts) - start_ - pausedTotal);
        item.ts = l.ts;
        item.route = route;
        item.type = std::move(l.type);
        if (route == ReplayRoute::Sample) {
            item.sample = telemetryFromJson(l.data);
        } else {
            item.data = std::move(l.data);
        }
        items_.push_back(std::move(item));
    }
    if (items_.empty()) return;
    // Start 15 s before the first event, not on minutes of samples at the gate (D-178); the last sample before that
    // opens the replay, so Live shows the aircraft where it was. Without a sample there, or an event at all, start at
    // the first line that plays, not at a skipped one before it.
    std::int64_t shift = items_.front().offsetMs;
    const auto event = std::find_if(items_.begin(), items_.end(),
                                    [](const ReplayItem& i) { return i.route != ReplayRoute::Sample; });
    if (event != items_.end()) {
        const std::int64_t cut = event->offsetMs - kLeadInMs;
        const auto after = std::find_if(items_.begin(), event, [&](const ReplayItem& i) { return i.offsetMs > cut; });
        if (after != items_.begin()) {
            const auto opening = after - 1;  // the last sample at or before the cut
            opening->offsetMs = cut;
            items_.erase(items_.begin(), opening);
            shift = cut;
        }
    }
    if (shift == 0) return;
    start_ += std::chrono::milliseconds(shift);
    for (ReplayItem& i : items_) i.offsetMs -= shift;
    for (Pause& p : pauses_) p.offsetMs -= shift;
}

TimePoint ReplayTimeline::timeAt(std::int64_t offsetMs) const {
    TimePoint t = start_ + std::chrono::milliseconds(offsetMs);
    for (const Pause& p : pauses_) {
        if (p.offsetMs < offsetMs) t += p.length;
    }
    return t;
}

std::optional<std::int64_t> ReplayTimeline::offsetOf(std::string_view at) const {
    const auto find = [&](auto pred) -> std::optional<std::int64_t> {
        const auto it = std::find_if(items_.begin(), items_.end(), pred);
        if (it == items_.end()) return std::nullopt;
        return it->offsetMs;
    };
    if (at == "block_out" || at == "block_in") {
        return find([&](const ReplayItem& i) { return i.type == at; });
    }
    if (const auto minutes = hoursMinutes(at)) {
        const auto out = offsetOf("block_out");
        if (!out) return std::nullopt;
        return *out + std::int64_t{*minutes} * 60'000;
    }
    return find([&](const ReplayItem& i) {
        return i.type == "phase_changed" && i.data.is_object() && i.data.value("phase", "") == at;
    });
}

ReplayTimeline loadReplay(const std::filesystem::path& dir, std::string_view flightId) {
    // Not from the previous leg's arrival, when it ended inside the 30 minutes before block out.
    return ReplayTimeline(loadFlightEvents(replayLogFiles(dir, flightId), flightId, true));
}

std::int64_t ReplayClock::advance(std::chrono::milliseconds wall) {
    if (!paused_) offset_ += std::llround(static_cast<double>(wall.count()) * speed_);
    return offset_;
}

}  // namespace blocktime
