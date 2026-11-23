// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_match.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "block_detector.h"
#include "geo.h"

namespace blocktime {

namespace {

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

bool equalsIgnoringCase(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return lower(x) == lower(y); });
}

}  // namespace

std::string_view problemCode(MatchProblem p) {
    switch (p) {
        case MatchProblem::None: return "";
        case MatchProblem::NoPlannedFlight: return "no_planned_flight";
        case MatchProblem::WrongAircraftType: return "wrong_aircraft_type";
        case MatchProblem::WrongAirport: return "wrong_airport";
        case MatchProblem::FuelBelowRecord: return "fuel_below_record";
    }
    return "";
}

std::string aircraftFolder(std::string_view path) {
    const auto file = path.find_last_of("\\/");
    if (file == std::string_view::npos) return {};
    const std::string_view dir = path.substr(0, file);
    const auto start = dir.find_last_of("\\/");
    return std::string(start == std::string_view::npos ? dir : dir.substr(start + 1));
}

bool folderMatches(std::string_view folder, std::string_view simFolder) {
    if (folder.empty() || simFolder.empty() || folder.size() < simFolder.size()) return false;
    if (!equalsIgnoringCase(folder.substr(0, simFolder.size()), simFolder)) return false;
    return folder.size() == simFolder.size() || folder[simFolder.size()] == ' ';
}

MatchResult checkMatch(const std::optional<FlightTarget>& target, const BlockSample& s) {
    MatchResult r;
    r.aircraftFolder = s.aircraftFolder;
    r.fuelLb = static_cast<int>(std::lround(s.fuelLb));
    r.lat = s.latDeg;
    r.lon = s.lonDeg;
    if (!target) return r;  // NoPlannedFlight
    r.plannedFlightId = target->plannedFlightId;
    r.distanceM = distanceMeters(target->originLat, target->originLon, s.latDeg, s.lonDeg);
    r.minFuelLb = std::max(0, target->fuelOnRecordLb - kFuelSlackLb);
    if (!folderMatches(s.aircraftFolder, target->simFolder)) {
        r.problem = MatchProblem::WrongAircraftType;
    } else if (r.distanceM > kOriginRadiusM) {
        r.problem = MatchProblem::WrongAirport;
    } else if (s.fuelLb < r.minFuelLb) {
        r.problem = MatchProblem::FuelBelowRecord;
    } else {
        r.problem = MatchProblem::None;
    }
    return r;
}

void FlightTargetSlot::set(std::optional<FlightTarget> target) {
    const std::lock_guard lock(mutex_);
    target_ = std::move(target);
}

std::optional<FlightTarget> FlightTargetSlot::get() const {
    const std::lock_guard lock(mutex_);
    return target_;
}

}  // namespace blocktime
