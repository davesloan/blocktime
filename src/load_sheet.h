// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Pre-Flight panel's load sheet (D-153, D-187): block fuel, ZFW, passengers by cabin, SimBrief's cargo split into
// the holds, and the FMC line's cruise level and transition altitude. Pounds; no centre of gravity.

#pragma once

#include <vector>

#include "loads.h"
#include "planned_flight.h"

namespace blocktime {

struct LoadSheet {
    bool hasPlan = false;
    int blockFuelLb = 0, zfwLb = 0;     // block fuel is the plan's ramp fuel: the total on board, not what to add
    std::vector<CabinLoad> passengers;  // the planned cabins; empty for a cargo or delivery flight
    int cargoLb = 0;                    // SimBrief's cargo (bags and freight)
    std::vector<HoldLoad> holds;        // the cargo at the holds' ratios; empty when it doesn't fit them
    int cruiseAltitudeFt = 0, transitionAltitudeFt = 0;  // the FMC line (D-187)
};

LoadSheet loadSheet(const PlannedFlightView& flight);

}  // namespace blocktime
