// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_events.h"

#include <deque>
#include <fstream>
#include <optional>
#include <string_view>

#include "event_log.h"

namespace blocktime {

namespace {

using Time = std::chrono::system_clock::time_point;

// A line's ts without parsing its JSON: EventLog::write puts it first, as {"ts":"…",…}. nullopt in any other form.
std::optional<Time> leadingTs(std::string_view text) {
    constexpr std::string_view prefix = R"({"ts":")";
    if (!text.starts_with(prefix)) return std::nullopt;
    const auto end = text.find('"', prefix.size());
    if (end == std::string_view::npos) return std::nullopt;
    return parseUtc(text.substr(prefix.size(), end - prefix.size()));
}

std::optional<Time> firstTs(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::string text;
    return std::getline(in, text) ? leadingTs(text) : std::nullopt;
}

struct BlockOut {
    std::size_t file;  // index into the files
    Time at;
};

// The flight's block_out line, found by text in the writer's compact form, newest file first (a flight is nearly always
// in the last file or two). Only matching lines are parsed. nullopt when no line in that form is found. A flight has
// one block_out; if a failed write was retried across midnight, this opens at the later copy (the old reader, the first).
std::optional<BlockOut> findBlockOut(const std::vector<std::filesystem::path>& files, std::string_view logFlightId) {
    const std::string idText = R"("flight_id":")" + std::string(logFlightId) + '"';
    for (std::size_t i = files.size(); i-- > 0;) {
        std::ifstream in(files[i], std::ios::binary);
        std::string text;
        while (std::getline(in, text)) {
            if (text.find(R"("type":"block_out")") == std::string::npos || text.find(idText) == std::string::npos) {
                continue;
            }
            const auto j = nlohmann::json::parse(text, nullptr, false);
            if (!j.is_object() || j.value("type", nlohmann::json()) != "block_out") continue;
            const auto data = j.value("data", nlohmann::json::object());
            const auto id = data.is_object() ? data.find("flight_id") : data.end();
            if (id == data.end() || !id->is_string() || id->get_ref<const std::string&>() != logFlightId) continue;
            const auto ts = j.value("ts", nlohmann::json());
            if (const auto at = ts.is_string() ? parseUtc(ts.get<std::string>()) : std::nullopt) return BlockOut{i, *at};
        }
    }
    return std::nullopt;
}

}  // namespace

std::vector<LogLine> loadFlightEvents(const std::filesystem::path& logDir, std::string_view logFlightId,
                                      std::size_t maxFiles) {
    std::vector<std::filesystem::path> files = dailyLogFiles(logDir);
    if (files.size() > maxFiles) files.erase(files.begin(), files.end() - static_cast<std::ptrdiff_t>(maxFiles));
    return loadFlightEvents(files, logFlightId);
}

std::vector<LogLine> loadFlightEvents(const std::vector<std::filesystem::path>& files, std::string_view logFlightId,
                                      bool startAfterOtherFlights, bool withBlockInInstant) {
    std::deque<LogLine> leadIn;  // the last kFlightLeadIn of lines before block out
    std::vector<LogLine> flight;
    bool open = false;
    std::optional<Time> blockedIn;  // withBlockInInstant: block in's instant
    // Review 5.1: settling reads this on the GUI thread, so only the flight's part of the log is parsed. Reading starts
    // at the file with its block_out (or earlier, while a file starts inside the lead-in), and a line before the
    // lead-in is skipped by its ts alone; the lead-in would have dropped it anyway. Without a block_out in the
    // writer's form, every line of every file is parsed, as before.
    std::size_t first = 0;
    std::optional<Time> leadStart;
    if (const auto out = findBlockOut(files, logFlightId)) {
        leadStart = out->at - kFlightLeadIn;
        first = out->file;
        while (first > 0) {
            const auto ts = firstTs(files[first]);
            if (ts && *ts < *leadStart) break;
            --first;
        }
    }
    for (std::size_t f = first; f < files.size(); ++f) {
        std::ifstream in(files[f], std::ios::binary);
        std::string text;
        while (std::getline(in, text)) {
            if (!open && leadStart) {
                if (const auto ts = leadingTs(text); ts && *ts < *leadStart) continue;
            }
            const auto j = nlohmann::json::parse(text, nullptr, false);
            if (!j.is_object() || !j.contains("ts") || !j["ts"].is_string() || !j.contains("type")) continue;
            const auto ts = parseUtc(j["ts"].get<std::string>());
            if (!ts) continue;
            LogLine line{*ts, j.value("type", ""), j.value("data", nlohmann::json::object())};
            const std::string id = line.data.is_object() ? line.data.value("flight_id", "") : "";
            if (blockedIn) {
                if (line.ts != *blockedIn) return flight;
                if (id.empty() || id == logFlightId) flight.push_back(std::move(line));
                continue;
            }
            if (!id.empty() && id != logFlightId) {
                if (startAfterOtherFlights && !open && (line.type == "block_in" || line.type == "flight_interrupted")) {
                    leadIn.clear();
                }
                continue;
            }
            if (!open) {
                while (!leadIn.empty() && leadIn.front().ts < line.ts - kFlightLeadIn) leadIn.pop_front();
                if (line.type == "block_out" && id == logFlightId) {
                    open = true;
                    flight.assign(leadIn.begin(), leadIn.end());
                    flight.push_back(std::move(line));
                    continue;
                }
                leadIn.push_back(std::move(line));
                continue;
            }
            const bool end = line.type == "block_in" && id == logFlightId;
            const auto at = line.ts;
            flight.push_back(std::move(line));
            if (end && !withBlockInInstant) return flight;
            if (end) blockedIn = at;
        }
    }
    return blockedIn ? flight : std::vector<LogLine>{};
}

}  // namespace blocktime
