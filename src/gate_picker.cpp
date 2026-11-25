// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gate_picker.h"

#include <algorithm>

#include "geo.h"

namespace blocktime {

namespace {

// Spot types (D-147). Military (6-7) is never picked.
bool isGate(int type) { return type >= 8 && type <= 10; }
bool isRamp(int type) { return type >= 2 && type <= 5; }

bool sameKind(int a, int b) { return (isGate(a) && isGate(b)) || (isRamp(a) && isRamp(b)); }

bool usable(const ParkingSpot& s, SpotUse use) { return use == SpotUse::Gate ? isGate(s.type) : isRamp(s.type); }

}  // namespace

SpotUse spotUseFor(std::string_view flightKind) {
    if (flightKind == "charter" || flightKind == "delivery") return SpotUse::Ramp;
    if (flightKind == "cargo") return SpotUse::CargoRamp;
    return SpotUse::Gate;
}

std::string_view pickReasonCode(PickReason r) {
    switch (r) {
        case PickReason::Preferred: return "preferred";
        case PickReason::Larger: return "larger";
    }
    return "";
}

bool spotFits(const ParkingSpot& s, double wingspanM) { return 2.0 * s.radiusM >= wingspanM; }

std::optional<GatePick> pickGate(const AirportFacilities& airport, const std::optional<ParkingSpot>& siSpot,
                                 const GateContext& context) {
    const double span = context.wingspanM;
    const auto preferred = [&](const ParkingSpot& s) {
        return usable(s, context.use) && spotFits(s, span) &&
               std::ranges::find(context.preferred, spotShortName(s)) != context.preferred.end();
    };
    if (siSpot && preferred(*siSpot)) return std::nullopt;
    for (const std::string& name : context.preferred) {
        for (const ParkingSpot& s : airport.spots) {
            if (spotShortName(s) == name && preferred(s)) return GatePick{s, PickReason::Preferred};
        }
    }
    if (!siSpot || spotFits(*siSpot, span)) return std::nullopt;
    std::optional<ParkingSpot> best;
    double bestM = 0;
    for (const ParkingSpot& s : airport.spots) {
        if (!sameKind(s.type, siSpot->type) || !spotFits(s, span)) continue;
        const double d = distanceMeters(s.lat, s.lon, siSpot->lat, siSpot->lon);
        if (!best || d < bestM) {
            best = s;
            bestM = d;
        }
    }
    if (best) return GatePick{*best, PickReason::Larger};
    return std::nullopt;
}

}  // namespace blocktime
