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
        "aircraft_loaded",  "block_out_refused", "airport_runways",  "flight_interrupted"};
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
    for (LogLine& l : lines) {
        if (l.type == "paused") {
            if (!pausedSince) pausedSince = l.ts;
            continue;
        }
        if (l.type == "unpaused") {
            if (pausedSince) {
                pauses_.push_back({ms(*pausedSince - start_ - pausedTotal), l.ts - *pausedSince});
                pausedTotal += l.ts - *pausedSince;
            }
            pausedSince.reset();
            continue;
        }
        const ReplayRoute route = replayRoute(l.type);
        if (route == ReplayRoute::Skip) continue;
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
    return ReplayTimeline(loadFlightEvents(replayLogFiles(dir, flightId), flightId));
}

std::int64_t ReplayClock::advance(std::chrono::milliseconds wall) {
    if (!paused_) offset_ += std::llround(static_cast<double>(wall.count()) * speed_);
    return offset_;
}

}  // namespace blocktime
