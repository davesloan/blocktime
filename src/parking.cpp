// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "parking.h"

#include <cmath>
#include <numbers>

#include "geo.h"

namespace blocktime {

namespace {

constexpr double kMetresPerDegree = 111320.0;
constexpr const char* kCompass[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};

}  // namespace

std::string spotName(const ParkingSpot& s) {
    const std::string number = std::to_string(s.number);
    if (s.name >= 12 && s.name <= 37) return "Gate " + spotShortName(s);
    if (s.name == 10) return "Gate " + number;
    if (s.name == 11) return "Dock " + number;
    if (s.name == 1) return "Parking " + number;
    if (s.name >= 2 && s.name <= 9) return std::string(kCompass[s.name - 2]) + " Parking " + number;
    return "Spot " + number;
}

std::string spotShortName(const ParkingSpot& s) {
    const std::string number = std::to_string(s.number);
    if (s.name >= 12 && s.name <= 37) return std::string(1, static_cast<char>('A' + (s.name - 12))) + number;
    return number;
}

void placeSpot(ParkingSpot& s, double airportLat, double airportLon, double biasXM, double biasZM) {
    s.lat = airportLat + biasZM / kMetresPerDegree;
    s.lon = airportLon + biasXM / (kMetresPerDegree * std::cos(airportLat * std::numbers::pi / 180.0));
}

std::optional<ParkingSpot> nearestSpot(const std::vector<ParkingSpot>& spots, double lat, double lon, double maxM) {
    std::optional<ParkingSpot> best;
    double bestM = maxM;
    for (const ParkingSpot& s : spots) {
        const double d = distanceMeters(lat, lon, s.lat, s.lon);
        if (d <= bestM) {
            bestM = d;
            best = s;
        }
    }
    return best;
}

}  // namespace blocktime
