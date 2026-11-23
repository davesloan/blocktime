// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A flight's money (design.md §10-12): crew, every cost line and every income line, each rounded to $5 (D-080).
// The rates come from rates.yaml (D-107). The same function prices the preview and the settled flight (D-109).

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "loads.h"
#include "money.h"
#include "rates.h"

namespace blocktime {

struct PriceInput {
    FlightKind kind = FlightKind::Passenger;
    std::string family;  // "737" (narrowbody rates) or "777" (widebody)
    std::string role;    // "airliner", "executive" (VIP crew and catering rates) or "freighter" (no cabin crew)
    int seatsInstalled = 0;
    std::vector<CabinLoad> cabins;
    int cargoLb = 0;
    double distanceNm = 0;  // great-circle (design.md §15)
    double blockHours = 0;  // the estimate, or the plan's block time
    // A settled flight's real block time (D-109): replaces blockHours for pilots (and their count), flight
    // attendants and maintenance only. Fares, catering and the rest stay as previewed.
    std::optional<double> actualBlockHours;
    int mlwLb = 0;
    std::optional<int> fuelBurnLb;  // unknown before a plan: the preview says "+ fuel"
    Cents fuelPriceCentsPer100Lb = 0;
};

struct MoneyLine {
    std::string category;  // the ledger_category code: "economy_fares", "fuel", "pilots", ...
    std::string label;
    Cents cents = 0;  // income positive, a cost negative; a multiple of 500
};

struct Preview {
    std::vector<MoneyLine> income, costs;
    Cents totalCents = 0;  // the sum of the rounded lines
    bool fuelIncluded = false;
};

Preview pricePreview(const PriceInput& in, const Rates& rates);

int pilotsFor(double blockHours);          // 2; 3 over 8 hours; 4 over 12
int flightAttendantsFor(int seatsInstalled);  // FAA minimum: 1 per 50 seats or part, none under 20
double estimatedBlockHours(double distanceNm, int cruiseSpeedKt);  // before a plan: distance / speed + 30 min

}  // namespace blocktime
