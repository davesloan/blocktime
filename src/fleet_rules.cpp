// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fleet_rules.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <utility>

namespace blocktime {

std::vector<std::string> missingFields(const SimbriefReading& r) {
    std::vector<std::string> out;
    const auto text = [&](const std::string& value, const char* label) {
        if (value.empty()) out.push_back(label);
    };
    const auto number = [&](const std::optional<int>& value, const char* label) {
        if (!value) out.push_back(label);
    };
    text(r.registration, "Civil Registration");
    text(r.internalId, "Internal ID");
    text(r.baseType, "Base Type");
    number(r.maxPassengers, "Max Passengers");
    number(r.passengerWeightLb, "Passenger Weight");
    number(r.bagWeightLb, "Baggage Weight");
    number(r.emptyWeightLb, "Empty Weight");
    number(r.mzfwLb, "Max Zero Fuel Weight");
    number(r.mtowLb, "Max Takeoff Weight");
    number(r.mlwLb, "Max Landing Weight");
    number(r.maxFuelLb, "Max Fuel Capacity");
    number(r.maxCargoLb, "Max Cargo Weight");
    number(r.minRunwayFt, "Min Runway");
    return out;
}

std::vector<LayoutChoice> candidateLayouts(const Catalog& catalog, std::string_view baseType, int maxPassengers) {
    std::vector<LayoutChoice> out;
    for (const CatalogType& type : catalog.types) {
        const auto& bases = type.simbriefBaseTypes;
        if (std::find(bases.begin(), bases.end(), baseType) == bases.end()) continue;
        for (const CatalogAirframe& layout : catalog.airframes) {
            if (layout.typeCode != type.code) continue;
            const int seats = std::accumulate(layout.cabins.begin(), layout.cabins.end(), 0,
                                              [](int sum, const Cabin& c) { return sum + c.seats; });
            out.push_back({&type, &layout, seats, seats == maxPassengers});
        }
    }
    return out;
}

std::optional<std::size_t> preselectedLayout(const std::vector<LayoutChoice>& choices) {
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (choices[i].seatsMatch) return i;
    }
    return std::nullopt;
}

std::vector<Hold> holdsForMaxCargo(const std::vector<Hold>& layoutHolds, int maxCargoLb) {
    std::int64_t shares = 0;
    for (const Hold& h : layoutHolds) shares += h.maxLb;
    std::vector<Hold> out = layoutHolds;
    if (out.empty() || shares <= 0) return out;
    int given = 0;
    for (std::size_t i = 0; i + 1 < out.size(); ++i) {
        out[i].maxLb = static_cast<int>(static_cast<std::int64_t>(maxCargoLb) * layoutHolds[i].maxLb / shares);
        given += out[i].maxLb;
    }
    out.back().maxLb = maxCargoLb - given;
    return out;
}

namespace {

// The lease month holding `day`: its start and the next month's start.
std::pair<Date, Date> leaseMonthOf(Date leasedOn, Date day) {
    int k = 0;
    while (leasePeriodStart(leasedOn, k + 1) <= day) ++k;
    return {leasePeriodStart(leasedOn, k), leasePeriodStart(leasedOn, k + 1)};
}

int daysBetween(Date from, Date to) {
    return static_cast<int>((std::chrono::sys_days{to} - std::chrono::sys_days{from}).count());
}

}  // namespace

int unusedLeaseDays(Date leasedOn, Date returnedOn) {
    const auto [start, next] = leaseMonthOf(leasedOn, returnedOn);
    (void)start;
    return std::max(0, daysBetween(returnedOn, next) - 1);
}

Cents leaseRefundCents(Cents monthlyCents, Date leasedOn, Date returnedOn) {
    const auto [start, next] = leaseMonthOf(leasedOn, returnedOn);
    const int monthDays = daysBetween(start, next);
    if (monthDays <= 0) return 0;
    const double cents = static_cast<double>(monthlyCents) * unusedLeaseDays(leasedOn, returnedOn) / monthDays;
    return std::max<Cents>(0, roundToFiveDollars(static_cast<Cents>(std::llround(cents))));
}

Preview instantDeliveryPrice(const CatalogType& type, double distanceNm, int mlwLb, const Rates& rates) {
    PriceInput in;
    in.kind = FlightKind::Delivery;
    in.family = type.family;
    in.role = type.role;
    in.distanceNm = distanceNm;
    in.cruiseSpeedKt = type.cruiseSpeedKt;
    in.mlwLb = mlwLb;
    Preview p = pricePreview(in, rates);  // no plan: no fuel line
    MoneyLine fee = costLine("delivery_fee", "Delivery fee", {{distanceNm, "nm"}}, type.deliveryFeePerNmDollars);
    p.totalCents += fee.cents;
    p.costs.insert(p.costs.begin(), std::move(fee));
    return p;
}

}  // namespace blocktime
