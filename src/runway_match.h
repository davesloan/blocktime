// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Names the runway a takeoff or landing roll used, from the sim's runways (facility data). It only names: the offsets
// are information for the log and never count toward any score (D-122, D-126). The method was checked on the probe
// flights' rolls (docs/notes/probe-flight.md, results item 4).

#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

// One runway as the sim's facility data gives it.
struct Runway {
    double lat = 0, lon = 0;  // its centre
    double headingDeg = 0;    // true, the primary end's
    double lengthM = 0, widthM = 0;
    int primaryNumber = 0, primaryDesignator = 0, secondaryNumber = 0, secondaryDesignator = 0;
};

struct AirportRunways {
    std::string icao;
    std::vector<Runway> runways;
};

struct RollPoint {
    double lat = 0, lon = 0;
};

struct RunwayName {
    std::string icao, runway;  // runway "14R"; "" = none fits
    double offCentrelineM = 0, offHeadingDeg = 0;
};

inline constexpr double kRunwayMaxOffHeadingDeg = 20.0;  // D-126
inline constexpr double kRunwayMaxOffCentrelineM = 75.0;
inline constexpr double kRunwayMinRollM = 300.0;

// The designator is 0 none, 1 L, 2 R, 3 C; the number is zero-padded to two digits ("03", "10R").
std::string runwayEndName(int number, int designator);

// The runway end whose heading is within 20° of the roll's track and whose centreline the roll's points are nearest,
// at most 75 m away on average. Fewer than 2 points, or a roll under 300 m, names nothing.
RunwayName nameRunway(const std::vector<AirportRunways>& airports, const std::vector<RollPoint>& roll);

// The airport_runways event's data: {"icao", "runways": [{lat, lon, heading, length_m, width_m, primary_number,
// primary_designator, secondary_number, secondary_designator}]}.
nlohmann::json toJson(const AirportRunways& airport);
AirportRunways airportRunwaysFromJson(const nlohmann::json& j);

}  // namespace blocktime
