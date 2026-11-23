// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pricing.h"

#include <algorithm>
#include <cmath>

namespace blocktime {

namespace {

constexpr double kTaxiAndClimbHours = 0.5;

// One line: exact cents, then rounded to $5 (D-080). Lines worth nothing are left out.
void add(std::vector<MoneyLine>& lines, const char* category, const char* label, double dollars) {
    const Cents cents = roundToFiveDollars(static_cast<Cents>(std::llround(dollars * 100)));
    if (cents != 0) lines.push_back({category, label, cents});
}

int passengersIn(const std::vector<CabinLoad>& cabins, const char* cabinClass) {
    for (const CabinLoad& c : cabins) {
        if (c.cabinClass == cabinClass) return c.passengers;
    }
    return 0;
}

}  // namespace

int pilotsFor(double blockHours) { return blockHours > 12 ? 4 : blockHours > 8 ? 3 : 2; }

int flightAttendantsFor(int seatsInstalled) { return seatsInstalled < 20 ? 0 : (seatsInstalled + 49) / 50; }

double estimatedBlockHours(double distanceNm, int cruiseSpeedKt) {
    return distanceNm / cruiseSpeedKt + kTaxiAndClimbHours;
}

Preview pricePreview(const PriceInput& in, const Rates& r) {
    Preview p;
    const FamilyRates& f = in.family == "777" ? r.widebody : r.narrowbody;
    const bool vip = in.role == "executive";
    const bool delivery = in.kind == FlightKind::Delivery;
    const double h = in.blockHours;
    const double crewH = in.actualBlockHours.value_or(h);  // D-109
    int people = 0;
    for (const CabinLoad& c : in.cabins) people += c.passengers;

    // Income.
    const double economyFare = r.economyBase + r.economyPerNm * in.distanceNm;
    if (in.kind == FlightKind::Passenger) {
        add(p.income, "economy_fares", "Economy fares", economyFare * passengersIn(in.cabins, "economy"));
        add(p.income, "business_fares", "Business fares",
            r.businessTimesEconomy * economyFare * passengersIn(in.cabins, "business"));
        add(p.income, "first_fares", "First fares",
            r.firstTimesEconomy * economyFare * passengersIn(in.cabins, "first"));
    } else if (in.kind == FlightKind::Vip) {
        add(p.income, "vip_fares", "VIP fares", (r.vipFareBase + r.vipFarePerNm * in.distanceNm) * people);
    } else if (in.kind == FlightKind::Charter) {
        add(p.income, "charter", "Charter", r.charterPerHour * std::max(h, r.charterMinHours));
    }
    if (in.cargoLb > 0) {
        add(p.income, "cargo", "Cargo",
            (r.cargoBasePer100Lb + r.cargoPer100LbPerNm * in.distanceNm) * in.cargoLb / 100.0);
    }

    // Costs.
    if (in.fuelBurnLb) {
        add(p.costs, "fuel", "Fuel",
            -static_cast<double>(*in.fuelBurnLb) / 100.0 * static_cast<double>(in.fuelPriceCentsPer100Lb) / 100.0);
        p.fuelIncluded = true;
    }
    add(p.costs, "landing_fee", "Landing fee", -r.landingPer1000LbMlw * in.mlwLb / 1000.0);
    if (!delivery && in.kind != FlightKind::Charter) {
        add(p.costs, "gate", "Departure / gate", -f.gate);
        add(p.costs, "handling", "Ground handling", -f.handling);
    }
    if (in.kind == FlightKind::Charter) add(p.costs, "ga_services", "GA services", -r.gaServices);
    add(p.costs, "atc", "ATC / navigation", -f.atcPerNm * in.distanceNm);
    if (!delivery) {
        const double perPassenger = r.catering[h < 3 ? 0 : h < 8 ? 1 : 2] * (vip ? r.vipCateringTimes : 1);
        add(p.costs, "catering", "Catering", -perPassenger * people);
        add(p.costs, "cargo_handling", "Cargo handling", -r.cargoHandlingPer1000Lb * in.cargoLb / 1000.0);
    }
    add(p.costs, "pilots", "Pilots", -pilotsFor(crewH) * (vip ? r.vipPilotPerHour : f.pilotPerHour) * crewH);
    if (!delivery && in.role != "freighter") {
        const double attendants = flightAttendantsFor(in.seatsInstalled) * (vip ? r.vipFlightAttendantsTimes : 1);
        add(p.costs, "flight_attendants", "Flight attendants",
            -attendants * (vip ? r.vipFlightAttendantPerHour : r.flightAttendantPerHour) * crewH);
    }
    add(p.costs, "maintenance", "Maintenance", -f.maintenancePerHour * crewH);

    for (const MoneyLine& m : p.income) p.totalCents += m.cents;
    for (const MoneyLine& m : p.costs) p.totalCents += m.cents;
    return p;
}

}  // namespace blocktime
