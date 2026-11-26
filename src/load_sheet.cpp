// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "load_sheet.h"

namespace blocktime {

LoadSheet loadSheet(const PlannedFlightView& flight) {
    LoadSheet s;
    if (flight.kind != FlightKind::Cargo && flight.kind != FlightKind::Delivery) s.passengers = flight.cabins;
    if (!flight.plan) return s;
    s.hasPlan = true;
    s.blockFuelLb = flight.plan->rampFuelLb;
    s.cruiseAltitudeFt = flight.plan->cruiseAltitudeFt;
    s.transitionAltitudeFt = flight.plan->originTransitionAltitudeFt;
    if (flight.brief) {
        s.zfwLb = flight.brief->zfwLb;
        s.cargoLb = flight.brief->cargoLb;
    }
    // splitHolds asserts the cargo fits; many holds are still catalog estimates, so SimBrief's may not.
    int capacity = 0;
    for (const Hold& h : flight.holds) capacity += h.maxLb;
    if (s.cargoLb > 0 && s.cargoLb <= capacity) s.holds = splitHolds(flight.holds, s.cargoLb);
    return s;
}

}  // namespace blocktime
