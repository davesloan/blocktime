// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The arrival gate (D-074, D-141, D-147): preferred gates for gate flights first, then the spot of the right type that
// fits, closest to SayIntentions' landing runway. Ramp flights fall back to any ramp, then a gate.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "parking.h"

namespace blocktime {

enum class SpotUse { Gate, Ramp, CargoRamp };
// passenger, vip → Gate; charter, delivery → Ramp; cargo → CargoRamp.
SpotUse spotUseFor(std::string_view flightKind);

enum class PickReason { Preferred, Closest, AnyRamp, GateFallback };
// "preferred", "closest", "any_ramp", "gate_fallback"
std::string_view pickReasonCode(PickReason r);

struct GatePick {
    ParkingSpot spot;
    PickReason reason;
};

// What the airline knows about the flight: the type's wingspan, how it parks, and the airline's preferred gates at
// the arrival airport (short names, airline.yaml).
struct GateContext {
    double wingspanM = 0;
    SpotUse use = SpotUse::Gate;
    std::vector<std::string> preferred;
};

bool spotFits(const ParkingSpot& s, double wingspanM);  // 2 × radius ≥ wingspan

// `landingRunway` is "26L", or "" (then the airport's reference point stands in for the runway). nullopt when nothing
// fits (a gate flight with no gate big enough).
std::optional<GatePick> pickGate(const AirportFacilities& airport, std::string_view landingRunway,
                                 const GateContext& context);

}  // namespace blocktime
