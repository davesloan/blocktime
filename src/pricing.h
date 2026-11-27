// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A flight's money (design.md §10-12): crew, every cost line and every income line, each rounded to $5 (D-080).
// The rates come from rates.yaml (D-107). The same function prices the preview and the settled flight (D-109).

#pragma once

#include <optional>
#include <string>
#include <string_view>
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
    // The block time is the plan's once one is in, else the estimate from the distance and cruise speed (D-104).
    int cruiseSpeedKt = 0;
    std::optional<int> planBlockS;
    // A settled flight's real block time (D-109): replaces the block time for pilots (and their count), flight
    // attendants and maintenance only. Fares, catering and the rest stay as previewed.
    std::optional<double> actualBlockHours;
    int mlwLb = 0;
    // The fuel charged. Before a plan it's unknown and the preview says "+ fuel". With a plan it's the fuel to add:
    // the plan's ramp fuel less the fuel on record, never below 0 (D-223). A settled flight charges the fuel really
    // added (D-109), which replaces the plan's.
    std::optional<int> planRampFuelLb;
    int fuelOnRecordLb = 0;
    std::optional<int> fuelAddedLb;
    Cents fuelPriceCentsPer100Lb = 0;
};

struct Quantity {
    double value = 0;
    std::string unit;  // "passengers", "h", "pilots", "lb", "nm"
};

struct MoneyLine {
    std::string category;  // the ledger_category code: "economy_fares", "fuel", "pilots", ...
    std::string label;
    Cents cents = 0;  // income positive, a cost negative; a multiple of 500
    // How it was worked out (review 4.4): the quantities multiplied, × rateDollars per rateBasis rateUnit, is the
    // amount before rounding to $5. A flat charge has no quantities. Lines read back from the ledger have no rate.
    std::vector<Quantity> quantities;
    double rateDollars = 0;
    int rateBasis = 1;     // 100 for "/ 100 lb", 1,000 for "/ 1,000 lb"
    std::string rateUnit;  // "" when the quantities say it ("2.5 h × 2 pilots × $250")
};

// "140 passengers × $157.50", "2.5 h × 2 pilots × $250", "5,900 lb × $41 / 100 lb", "$300 / departure". "" for a
// line without a rate.
std::string workingText(const MoneyLine& line);

struct Preview {
    std::vector<MoneyLine> income, costs;
    Cents totalCents = 0;  // the sum of the rounded lines
    bool fuelIncluded = false;
};

Preview pricePreview(const PriceInput& in, const Rates& rates);

int pilotsFor(double blockHours);          // 2; 3 over 8 hours; 4 over 12
int flightAttendantsFor(int seatsInstalled);  // FAA minimum: 1 per 50 seats or part, none under 20
// The crew BlockTime pays for (design.md section 10), for SOULS on the in-flight panel (D-203): the pilots for the
// block time, plus the flight attendants for the seats (× vipAttendantsTimes on an executive airplane). A freighter
// or a delivery flies the pilots only.
int crewFor(std::string_view role, bool delivery, int seatsInstalled, double blockHours, int vipAttendantsTimes);
double estimatedBlockHours(double distanceNm, int cruiseSpeedKt);  // before a plan: distance / speed + 30 min
double blockHoursFor(const PriceInput& in);  // the plan's block time, else the estimate

}  // namespace blocktime
