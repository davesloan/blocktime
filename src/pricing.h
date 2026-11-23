// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A flight's money (design.md §10-12): crew, every cost line and every income line, each rounded to $5 (D-080).
// The rates are constants in pricing.cpp until the settings screen (D-104).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "loads.h"
#include "money.h"

namespace blocktime {

struct PriceInput {
    FlightKind kind = FlightKind::Passenger;
    std::string family;  // "737" (narrowbody rates) or "777" (widebody)
    std::string role;    // "airliner", "executive" (VIP crew and catering rates) or "freighter" (no cabin crew)
    int seatsInstalled = 0;
    std::vector<CabinLoad> cabins;
    int cargoLb = 0;
    double distanceNm = 0;  // great-circle (design.md §15)
    double blockHours = 0;
    int mlwLb = 0;
    std::optional<int> fuelBurnLb;  // unknown before a plan: the preview says "+ fuel"
    Cents fuelPriceCentsPer100Lb = 0;
};

struct MoneyLine {
    std::string label;
    Cents cents = 0;  // income positive, a cost negative; a multiple of 500
};

struct Preview {
    std::vector<MoneyLine> income, costs;
    Cents totalCents = 0;  // the sum of the rounded lines
    bool fuelIncluded = false;
};

Preview pricePreview(const PriceInput& in);

int pilotsFor(double blockHours);          // 2; 3 over 8 hours; 4 over 12
int flightAttendantsFor(int seatsInstalled);  // FAA minimum: 1 per 50 seats or part, none under 20
double estimatedBlockHours(double distanceNm, int cruiseSpeedKt);  // before a plan: distance / speed + 30 min

}  // namespace blocktime
