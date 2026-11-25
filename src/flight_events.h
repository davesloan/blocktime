// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One flight's lines read back from the event log (D-060), for the debrief (docs/debrief.md): what the measurements
// and Claude's timeline are built from. Read-only.

#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "open_flight.h"

namespace blocktime {

struct LogLine {
    std::chrono::system_clock::time_point ts;  // when the line was written (telemetry has no data.time)
    std::string type;
    nlohmann::json data;
};

// How far before block out the lines start: the beacon and engine state before the push (debrief check 1).
inline constexpr std::chrono::minutes kFlightLeadIn{30};

// Every line from kFlightLeadIn before the flight's block_out line to its block_in line, in log order, across the last
// `maxFiles` daily logs. A line with another flight's flight_id is dropped; lines that don't parse are skipped. Empty
// when the flight has no block_in (interrupted, or not in these files).
std::vector<LogLine> loadFlightEvents(const std::filesystem::path& logDir, std::string_view logFlightId,
                                      std::size_t maxFiles = kOpenFlightLookbackFiles);

}  // namespace blocktime
