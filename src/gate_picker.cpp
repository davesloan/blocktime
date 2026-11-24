// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gate_picker.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace blocktime {

namespace {

constexpr double kMetresPerDegree = 111320.0;
constexpr double kDeg = std::numbers::pi / 180.0;

// Spot types (D-147). Military (6-7) is never picked.
bool isGate(int type) { return type >= 8 && type <= 10; }
bool isGaRamp(int type) { return type >= 2 && type <= 4; }
bool isCargoRamp(int type) { return type == 5; }
bool isRamp(int type) { return type >= 2 && type <= 5; }

struct Xy {
    double x = 0, y = 0;  // metres east and north of the reference point
};

struct Target {
    Xy a, b;  // a segment; a == b for a point
};

Xy toXy(const AirportFacilities& ap, double lat, double lon) {
    return {(lon - ap.lon) * kMetresPerDegree * std::cos(ap.lat * kDeg), (lat - ap.lat) * kMetresPerDegree};
}

// The landing runway as a segment between its thresholds; the reference point if it isn't found.
Target targetFor(const AirportFacilities& ap, std::string_view runway) {
    for (const Runway& r : ap.runways) {
        if (runway.empty()) break;
        if (runwayEndName(r.primaryNumber, r.primaryDesignator) != runway &&
            runwayEndName(r.secondaryNumber, r.secondaryDesignator) != runway) {
            continue;
        }
        const Xy c = toXy(ap, r.lat, r.lon);
        const double h = r.headingDeg * kDeg, half = r.lengthM / 2.0;
        return {{c.x - half * std::sin(h), c.y - half * std::cos(h)}, {c.x + half * std::sin(h), c.y + half * std::cos(h)}};
    }
    return {};
}

double distanceTo(const Target& t, const Xy& p) {
    const double dx = t.b.x - t.a.x, dy = t.b.y - t.a.y;
    const double len2 = dx * dx + dy * dy;
    const double k = len2 == 0 ? 0 : std::clamp(((p.x - t.a.x) * dx + (p.y - t.a.y) * dy) / len2, 0.0, 1.0);
    return std::hypot(p.x - (t.a.x + k * dx), p.y - (t.a.y + k * dy));
}

template <class Pred>
std::optional<ParkingSpot> closest(const AirportFacilities& ap, const Target& t, double wingspanM, Pred typeOk) {
    std::optional<ParkingSpot> best;
    double bestM = 0;
    for (const ParkingSpot& s : ap.spots) {
        if (!typeOk(s.type) || !spotFits(s, wingspanM)) continue;
        const double d = distanceTo(t, toXy(ap, s.lat, s.lon));
        if (!best || d < bestM) {
            best = s;
            bestM = d;
        }
    }
    return best;
}

}  // namespace

SpotUse spotUseFor(std::string_view flightKind) {
    if (flightKind == "charter" || flightKind == "delivery") return SpotUse::Ramp;
    if (flightKind == "cargo") return SpotUse::CargoRamp;
    return SpotUse::Gate;
}

std::string_view pickReasonCode(PickReason r) {
    switch (r) {
        case PickReason::Preferred: return "preferred";
        case PickReason::Closest: return "closest";
        case PickReason::AnyRamp: return "any_ramp";
        case PickReason::GateFallback: return "gate_fallback";
    }
    return "";
}

bool spotFits(const ParkingSpot& s, double wingspanM) { return 2.0 * s.radiusM >= wingspanM; }

std::optional<GatePick> pickGate(const AirportFacilities& airport, std::string_view landingRunway,
                                 const GateContext& context) {
    const Target target = targetFor(airport, landingRunway);
    const double span = context.wingspanM;
    if (context.use == SpotUse::Gate) {
        for (const std::string& name : context.preferred) {
            for (const ParkingSpot& s : airport.spots) {
                if (isGate(s.type) && spotShortName(s) == name && spotFits(s, span)) {
                    return GatePick{s, PickReason::Preferred};
                }
            }
        }
        if (auto s = closest(airport, target, span, isGate)) return GatePick{*s, PickReason::Closest};
        return std::nullopt;
    }
    const auto own = context.use == SpotUse::CargoRamp ? isCargoRamp : isGaRamp;
    if (auto s = closest(airport, target, span, own)) return GatePick{*s, PickReason::Closest};
    if (auto s = closest(airport, target, span, isRamp)) return GatePick{*s, PickReason::AnyRamp};
    if (auto s = closest(airport, target, span, isGate)) return GatePick{*s, PickReason::GateFallback};
    return std::nullopt;
}

}  // namespace blocktime
