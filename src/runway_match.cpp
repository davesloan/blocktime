// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "runway_match.h"

#include <cmath>
#include <numbers>

#include "geo.h"

namespace blocktime {

namespace {

constexpr double kDegToRad = std::numbers::pi / 180.0;
constexpr double kMetersPerDeg = 6371000.0 * kDegToRad;  // the same Earth as distanceMeters()

struct EastNorth {
    double e = 0, n = 0;
};

// A flat projection around (lat0, lon0): plenty for the few kilometres of an airport.
EastNorth project(double lat, double lon, double lat0, double lon0) {
    return {(lon - lon0) * std::cos(lat0 * kDegToRad) * kMetersPerDeg, (lat - lat0) * kMetersPerDeg};
}

double angleBetween(double a, double b) { return std::abs(std::fmod(a - b + 540.0, 360.0) - 180.0); }

}  // namespace

std::string runwayEndName(int number, int designator) {
    std::string s = (number < 10 ? "0" : "") + std::to_string(number);
    switch (designator) {
        case 1: return s + "L";
        case 2: return s + "R";
        case 3: return s + "C";
        default: return s;
    }
}

RunwayName nameRunway(const std::vector<AirportRunways>& airports, const std::vector<RollPoint>& roll) {
    RunwayName best;
    if (roll.size() < 2) return best;
    const RollPoint& a = roll.front();
    const RollPoint& b = roll.back();
    if (distanceMeters(a.lat, a.lon, b.lat, b.lon) < kRunwayMinRollM) return best;
    const EastNorth d = project(b.lat, b.lon, a.lat, a.lon);
    const double track = std::fmod(std::atan2(d.e, d.n) / kDegToRad + 360.0, 360.0);

    double bestOffset = kRunwayMaxOffCentrelineM;
    for (const AirportRunways& airport : airports) {
        for (const Runway& r : airport.runways) {
            struct End {
                double heading;
                int number, designator;
            };
            for (const End& end : {End{r.headingDeg, r.primaryNumber, r.primaryDesignator},
                                   End{std::fmod(r.headingDeg + 180.0, 360.0), r.secondaryNumber,
                                       r.secondaryDesignator}}) {
                const double offHeading = angleBetween(track, end.heading);
                if (offHeading > kRunwayMaxOffHeadingDeg) continue;
                const double h = end.heading * kDegToRad;
                double sum = 0;
                for (const RollPoint& p : roll) {
                    const EastNorth q = project(p.lat, p.lon, r.lat, r.lon);
                    sum += std::abs(q.e * std::cos(h) - q.n * std::sin(h));
                }
                const double offset = sum / static_cast<double>(roll.size());
                if (offset > bestOffset) continue;
                bestOffset = offset;
                best = {airport.icao, runwayEndName(end.number, end.designator), offset, offHeading};
            }
        }
    }
    return best;
}

nlohmann::json toJson(const AirportRunways& airport) {
    nlohmann::json runways = nlohmann::json::array();
    for (const Runway& r : airport.runways) {
        runways.push_back({{"lat", r.lat},
                           {"lon", r.lon},
                           {"heading", r.headingDeg},
                           {"length_m", r.lengthM},
                           {"width_m", r.widthM},
                           {"primary_number", r.primaryNumber},
                           {"primary_designator", r.primaryDesignator},
                           {"secondary_number", r.secondaryNumber},
                           {"secondary_designator", r.secondaryDesignator}});
    }
    return {{"icao", airport.icao}, {"runways", runways}};
}

AirportRunways airportRunwaysFromJson(const nlohmann::json& j) {
    AirportRunways a;
    a.icao = j.value("icao", "");
    for (const auto& r : j.value("runways", nlohmann::json::array())) {
        Runway w;
        w.lat = r.value("lat", 0.0);
        w.lon = r.value("lon", 0.0);
        w.headingDeg = r.value("heading", 0.0);
        w.lengthM = r.value("length_m", 0.0);
        w.widthM = r.value("width_m", 0.0);
        w.primaryNumber = r.value("primary_number", 0);
        w.primaryDesignator = r.value("primary_designator", 0);
        w.secondaryNumber = r.value("secondary_number", 0);
        w.secondaryDesignator = r.value("secondary_designator", 0);
        a.runways.push_back(w);
    }
    return a;
}

}  // namespace blocktime
