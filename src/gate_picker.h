// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The arrival gate (D-074, D-141, D-147, D-155): SayIntentions' own gate unless the airline prefers another spot there
// or SayIntentions' is too small for the type. MSFS's parking data lists spots the scenery doesn't have, so BlockTime
// never picks from the whole airport.

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

enum class PickReason { Preferred, Larger };
// "preferred", "larger"
std::string_view pickReasonCode(PickReason r);

struct GatePick {
    ParkingSpot spot;
    PickReason reason;
};

// What the airline knows about the flight: the type's wingspan, how it parks, and the airline's preferred spots at
// the arrival airport (short names, airline.yaml): gates for gate flights, ramps for ramp flights.
struct GateContext {
    double wingspanM = 0;
    SpotUse use = SpotUse::Gate;
    std::vector<std::string> preferred;
};

bool spotFits(const ParkingSpot& s, double wingspanM);  // 2 × radius ≥ wingspan

// `siSpot` is SayIntentions' gate matched to an MSFS spot (nullopt if none is near it). The first preferred spot that
// fits, unless SayIntentions' already is one; else, if SayIntentions' is too small, the nearest larger spot of the same
// kind (gate or ramp). nullopt: keep SayIntentions' gate.
std::optional<GatePick> pickGate(const AirportFacilities& airport, const std::optional<ParkingSpot>& siSpot,
                                 const GateContext& context);

}  // namespace blocktime
