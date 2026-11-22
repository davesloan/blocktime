// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "open_flight.h"

#include "event_log.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

namespace {

bool isFlightLine(const std::string& line) {
    return line.find(R"("type":"block_out")") != std::string::npos ||
           line.find(R"("type":"block_in")") != std::string::npos ||
           line.find(R"("type":"flight_interrupted")") != std::string::npos;
}

struct LastFlightEvent {
    std::string type;
    std::string flightId;
};

// The last parseable block_out / block_in / flight_interrupted line in the file, if any.
std::optional<LastFlightEvent> lastFlightEvent(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary);
    std::optional<LastFlightEvent> last;
    std::string line;
    while (std::getline(in, line)) {
        if (!isFlightLine(line)) continue;  // cheap filter: most lines are telemetry
        const auto j = nlohmann::json::parse(line, nullptr, false);
        if (j.is_discarded() || !j.is_object()) continue;
        const auto type = j.find("type");
        const auto data = j.find("data");
        if (type == j.end() || !type->is_string() || data == j.end() || !data->is_object()) continue;
        const auto id = data->find("flight_id");
        if (id == data->end() || !id->is_string()) continue;
        last = LastFlightEvent{type->get<std::string>(), id->get<std::string>()};
    }
    return last;
}

}  // namespace

std::optional<std::string> findOpenFlight(const std::filesystem::path& dir, std::size_t maxFiles) {
    std::vector<std::filesystem::path> files = dailyLogFiles(dir);
    std::reverse(files.begin(), files.end());  // newest first
    if (files.size() > maxFiles) files.resize(maxFiles);

    for (const auto& file : files) {
        if (auto last = lastFlightEvent(file)) {
            if (last->type == "block_out") return last->flightId;
            return std::nullopt;
        }
    }
    return std::nullopt;
}

}  // namespace blocktime
