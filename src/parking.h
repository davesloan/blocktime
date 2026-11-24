// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// An airport's parking spots, as the sim's facility data gives them, with their names and the nearest one to a
// position (milestone 9, D-122, D-147).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "runway_match.h"

namespace blocktime {

// One TAXI_PARKING record. `name` is MSFS's code: 0 none, 1 Parking, 2-9 N/NE/E/SE/S/SW/W/NW Parking, 10 Gate,
// 11 Dock, 12-37 Gate A-Z. `type`: 2-4 GA ramps (small, medium, large), 5 cargo ramp, 6-7 military, 8-10 gates
// (small, medium, heavy). `suffix` is ignored (D-147).
struct ParkingSpot {
    int name = 0, number = 0, suffix = 0, type = 0;
    double lat = 0, lon = 0, headingDeg = 0, radiusM = 0;
};

// What the recorder reads about an airport from the sim (D-126, D-147).
struct AirportFacilities {
    std::string icao;
    double lat = 0, lon = 0;  // reference point
    std::vector<Runway> runways;
    std::vector<ParkingSpot> spots;
};

inline constexpr double kParkedSpotMaxM = 40.0;  // D-122

// "Gate E7", "Gate 46", "Parking 70", "N Parking 3", "Dock 2"; "Spot 5" for name 0.
std::string spotName(const ParkingSpot& s);
// SayIntentions' form, letters and digits only: "E7", "46", "70".
std::string spotShortName(const ParkingSpot& s);

// Sets the spot's position from the facility record's offsets: biasXM metres east and biasZM metres north of the
// airport's reference point (a flat offset; it put SayIntentions' KLAS E7A 16.6 m from MSFS's Gate E7).
void placeSpot(ParkingSpot& s, double airportLat, double airportLon, double biasXM, double biasZM);

// The nearest spot within maxM, or nullopt.
std::optional<ParkingSpot> nearestSpot(const std::vector<ParkingSpot>& spots, double lat, double lon,
                                       double maxM = kParkedSpotMaxM);

}  // namespace blocktime
