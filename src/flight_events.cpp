// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_events.h"

#include <deque>
#include <fstream>

#include "event_log.h"

namespace blocktime {

std::vector<LogLine> loadFlightEvents(const std::filesystem::path& logDir, std::string_view logFlightId,
                                      std::size_t maxFiles) {
    std::vector<std::filesystem::path> files = dailyLogFiles(logDir);
    if (files.size() > maxFiles) files.erase(files.begin(), files.end() - static_cast<std::ptrdiff_t>(maxFiles));
    return loadFlightEvents(files, logFlightId);
}

std::vector<LogLine> loadFlightEvents(const std::vector<std::filesystem::path>& files, std::string_view logFlightId,
                                      bool startAfterOtherFlights) {
    std::deque<LogLine> leadIn;  // the last kFlightLeadIn of lines before block out
    std::vector<LogLine> flight;
    bool open = false;
    for (const auto& file : files) {
        std::ifstream in(file, std::ios::binary);
        std::string text;
        while (std::getline(in, text)) {
            const auto j = nlohmann::json::parse(text, nullptr, false);
            if (!j.is_object() || !j.contains("ts") || !j["ts"].is_string() || !j.contains("type")) continue;
            const auto ts = parseUtc(j["ts"].get<std::string>());
            if (!ts) continue;
            LogLine line{*ts, j.value("type", ""), j.value("data", nlohmann::json::object())};
            const std::string id = line.data.is_object() ? line.data.value("flight_id", "") : "";
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
            flight.push_back(std::move(line));
            if (end) return flight;
        }
    }
    return {};
}

}  // namespace blocktime
