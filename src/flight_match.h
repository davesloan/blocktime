// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The matching check (design.md §13.4, D-088): a brake release only opens a flight when the sim's aircraft is the
// planned airplane's type (by its aircraft.cfg folder, D-112), it's at the planned origin, and it has at least the
// fuel the airplane has on record, less some slack (D-110). The GUI thread hands the planned flight to the recorder
// thread through a FlightTargetSlot.

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace blocktime {

struct BlockSample;  // block_detector.h, which includes this header for MatchResult

inline constexpr double kOriginRadiusM = 3000.0;  // from the airport reference point (D-088)
inline constexpr int kFuelSlackLb = 300;          // D-110

// What the recorder needs to know about the planned flight.
struct FlightTarget {
    std::int64_t plannedFlightId = 0;
    std::string plannedAt;  // UTC instant; settling ignores an older log pair with the same id (D-113)
    std::string simFolder, typeCode, registration, originIcao;
    std::string destinationIcao;  // only for --probe's facility data (D-116)
    double originLat = 0, originLon = 0;
    int fuelOnRecordLb = 0;
    // For the phases (D-124); 0 = unknown (no plan imported yet).
    double destinationLat = 0, destinationLon = 0;
    int cruiseAltitudeFt = 0, transitionAltitudeFt = 0, transitionLevelFt = 0;
    // Accepted on Your Flight (D-222). Block out accepts a flight that wasn't, with a marker first (D-250).
    bool accepted = false;
};

// Checked in this order; the first that applies is reported.
enum class MatchProblem { None, NoPlannedFlight, WrongAircraftType, WrongAirport, FuelBelowRecord };

struct MatchResult {
    MatchProblem problem = MatchProblem::NoPlannedFlight;
    std::int64_t plannedFlightId = 0;  // 0 without a target
    std::string aircraftFolder;        // as seen ("pmdg 737-800bbj2"); "" if not known yet
    double distanceM = 0;              // from the origin's reference point; 0 without a target
    double lat = 0, lon = 0;           // where the aircraft is, for "Not recording" to name the airport
    int fuelLb = 0, minFuelLb = 0;     // minFuelLb = max(0, fuelOnRecordLb - kFuelSlackLb)

    bool operator==(const MatchResult&) const = default;
};

// "", "no_planned_flight", "wrong_aircraft_type", "wrong_airport" or "fuel_below_record": the log's reason.
std::string_view problemCode(MatchProblem p);

// The folder that holds aircraft.cfg, from either form of the AircraftLoaded path; "" for "".
std::string aircraftFolder(std::string_view aircraftCfgPath);

// Ignoring case: the folder is simFolder, or starts with simFolder plus a space ("PMDG 777-200ER GE"). So
// "PMDG 737-800BBJ2" isn't a "PMDG 737-800". An empty folder or simFolder matches nothing.
bool folderMatches(std::string_view folder, std::string_view simFolder);

MatchResult checkMatch(const std::optional<FlightTarget>& target, const BlockSample& s);

// Set on the GUI thread, read on the recorder thread.
class FlightTargetSlot {
public:
    void set(std::optional<FlightTarget> target);
    std::optional<FlightTarget> get() const;

private:
    mutable std::mutex mutex_;
    std::optional<FlightTarget> target_;
};

}  // namespace blocktime
