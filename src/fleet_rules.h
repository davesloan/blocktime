// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fleet's rules as plain logic (docs/fleet.md, milestone 15b): what a SimBrief airframe screenshot gave, which
// catalog layouts it can be, the hold maximums from SimBrief's Max Cargo, the refund when an airplane is returned and
// the price of instant delivery. No database here; leasing.h writes the results.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalog.h"
#include "dates.h"
#include "money.h"
#include "pricing.h"
#include "rates.h"

namespace blocktime {

// The values read from an airplane's SimBrief airframe page (D-242). An empty string or nullopt was not read.
struct SimbriefReading {
    std::string registration;  // Civil Registration
    std::string finNumber;     // Fin Number: the tail number; optional, "" means none
    std::string internalId;    // Internal ID: the SimBrief airframe ID
    std::string baseType;      // Base Type ("B738", "BBJ2")
    std::string weightUnits;   // "LBS"; anything else is refused when it's read
    std::optional<int> maxPassengers, passengerWeightLb, bagWeightLb, emptyWeightLb, mzfwLb, mtowLb, mlwLb, maxFuelLb,
        maxCargoLb, minRunwayFt;
};

// The SimBrief labels of the values a lease needs that weren't read, in the form's order. Fin Number is optional.
std::vector<std::string> missingFields(const SimbriefReading& reading);
// The SimBrief labels of read values that must be more than 0 but aren't (the weights other than bags, and the
// runway). Max Passengers and Baggage Weight may be 0 (a freighter, no bags).
std::vector<std::string> zeroFields(const SimbriefReading& reading);

// A layout a screenshot could be: every layout of every type that accepts its base type, with whether its seats
// equal SimBrief's Max Passengers (they must, D-242). Pointers into the catalog.
struct LayoutChoice {
    const CatalogType* type = nullptr;
    const CatalogAirframe* layout = nullptr;
    int seats = 0;
    bool seatsMatch = false;
};

std::vector<LayoutChoice> candidateLayouts(const Catalog& catalog, std::string_view baseType, int maxPassengers);
// The first choice whose seats match, in file order (D-246); nullopt if none does.
std::optional<std::size_t> preselectedLayout(const std::vector<LayoutChoice>& choices);

// The airplane's holds: SimBrief's Max Cargo split in the layout's shares (each hold's maxLb), each rounded down to
// whole pounds with the remainder to the last hold (D-246). The ratios are kept.
std::vector<Hold> holdsForMaxCargo(const std::vector<Hold>& layoutHolds, int maxCargoLb);

// The days of the current lease month after `returnedOn` (from tomorrow to the day before the next month starts).
int unusedLeaseDays(Date leasedOn, Date returnedOn);
// The refund for returning on `returnedOn`: the month's charge x unused days / the month's days, rounded to $5
// (design.md §5, D-246). Never negative.
Cents leaseRefundCents(Cents monthlyCents, Date leasedOn, Date returnedOn);

// Instant delivery's price (D-245): New Flight's delivery estimate (pilots only, maintenance, ATC, landing) with the
// fuel replaced by a Delivery fee, the distance x the type's rate, first in the costs.
Preview instantDeliveryPrice(const CatalogType& type, double distanceNm, int mlwLb, const Rates& rates);

}  // namespace blocktime
