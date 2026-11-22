// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads flights and their tracks back from the event log (D-060), for the app's Flights screen. Read-only: the log
// stays the one record of every flight (D-008).

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace blocktime {

struct TrackPoint {
    double lat = 0;
    double lon = 0;
    double altFt = 0;  // alt_ft (PLANE ALTITUDE); 0 for the block out and block in points
};

enum class FlightEnd { Open, BlockIn, Interrupted };

struct FlightRecord {
    std::string flightId;
    std::string atcId;
    std::string title;
    std::string outTime;  // block_out data.time, as logged (ISO 8601 UTC)
    double outFuelLb = 0;
    FlightEnd end = FlightEnd::Open;
    std::string endTime;    // block_in data.time, or the flight_interrupted line's ts; "" while Open
    std::string endReason;  // flight_interrupted reason ("unclosed" if a later block_out found it open), else ""
    std::optional<double> inFuelLb;  // block_in only
    std::optional<int> blockTimeS;   // block_in only
    double distanceNm = 0;           // over all samples, before thinning
    double maxAltFt = 0;             // highest alt_ft over all samples
    std::vector<TrackPoint> track;   // block out position, telemetry, block in position; thinned
};

inline constexpr std::size_t kFlightHistoryFiles = 30;  // daily log files read
inline constexpr std::size_t kMaxTrackPoints = 2000;
inline constexpr double kTrackMinSpacingM = 25.0;  // a sample closer than this to the last kept point is dropped

// Newest flight first. Reads the last `maxFiles` daily logs in `dir`, oldest to newest, as one stream, so a flight
// that crosses UTC midnight is one flight. A block_out opens a flight; the telemetry after it is its track; a
// block_in or flight_interrupted with the same flight_id closes it. A block_out while a flight is open closes the
// open one as Interrupted with reason "unclosed". Lines that don't parse are skipped (D-036).
std::vector<FlightRecord> loadFlightHistory(const std::filesystem::path& dir,
                                            std::size_t maxFiles = kFlightHistoryFiles);

}  // namespace blocktime
