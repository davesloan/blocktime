// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "open_flight.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

namespace {

// events-YYYY-MM-DD.jsonl, as EventLog names its daily files (D-020).
bool isDailyLog(const std::string& name) {
    constexpr std::string_view prefix = "events-";
    constexpr std::string_view suffix = ".jsonl";
    if (name.size() != prefix.size() + 10 + suffix.size()) return false;
    if (!name.starts_with(prefix) || !name.ends_with(suffix)) return false;
    const std::string_view date = std::string_view(name).substr(prefix.size(), 10);
    for (std::size_t i = 0; i < date.size(); ++i) {
        const bool dash = i == 4 || i == 7;
        if (dash ? date[i] != '-' : !std::isdigit(static_cast<unsigned char>(date[i]))) return false;
    }
    return true;
}

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
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && isDailyLog(it->path().filename().string())) files.push_back(it->path());
    }
    // The date in the name sorts as text, so newest first is a reverse sort by name.
    std::sort(files.begin(), files.end(), std::greater<>());
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
