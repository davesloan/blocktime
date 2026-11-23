// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pricing.h"

#include <algorithm>
#include <cmath>

namespace blocktime {

namespace {

// design.md §11-12, in dollars. Narrowbody (737) first, widebody (777) second where they differ.
constexpr double kLandingPer1000LbMlw = 4.50;
constexpr double kGate[2] = {300, 900};
constexpr double kHandling[2] = {900, 2800};
constexpr double kAtcPerNm[2] = {1.00, 2.00};
constexpr double kCargoHandlingPer1000Lb = 40;
constexpr double kPilotPerHour[2] = {200, 300};
constexpr double kVipPilotPerHour = 250;
constexpr double kFlightAttendantPerHour = 55;
constexpr double kVipFlightAttendantPerHour = 75;
constexpr double kMaintenancePerHour[2] = {300, 700};
constexpr double kGaServices = 1500;
constexpr double kCatering[3] = {5, 15, 35};  // per passenger: under 3 h, 3-8 h, 8 h and over; doubled for VIP

constexpr double kEconomyBase = 90, kEconomyPerNm = 0.075;  // business 3x, first 5x
constexpr double kVipBase = 180, kVipPerNm = 0.48;          // per seat sold
constexpr double kCharterPerHour = 7500, kCharterMinHours = 2;
constexpr double kCargoBasePer100Lb = 15, kCargoPer100LbPerNm = 0.028;

constexpr double kTaxiAndClimbHours = 0.5;

// One line: exact cents, then rounded to $5 (D-080). Lines worth nothing are left out.
void add(std::vector<MoneyLine>& lines, const char* label, double dollars) {
    const Cents cents = roundToFiveDollars(static_cast<Cents>(std::llround(dollars * 100)));
    if (cents != 0) lines.push_back({label, cents});
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

Preview pricePreview(const PriceInput& in) {
    Preview p;
    const int w = in.family == "777" ? 1 : 0;
    const bool vip = in.role == "executive";
    const bool delivery = in.kind == FlightKind::Delivery;
    const double h = in.blockHours;
    int people = 0;
    for (const CabinLoad& c : in.cabins) people += c.passengers;

    // Income.
    const double economyFare = kEconomyBase + kEconomyPerNm * in.distanceNm;
    if (in.kind == FlightKind::Passenger) {
        add(p.income, "Economy fares", economyFare * passengersIn(in.cabins, "economy"));
        add(p.income, "Business fares", 3 * economyFare * passengersIn(in.cabins, "business"));
        add(p.income, "First fares", 5 * economyFare * passengersIn(in.cabins, "first"));
    } else if (in.kind == FlightKind::Vip) {
        add(p.income, "VIP fares", (kVipBase + kVipPerNm * in.distanceNm) * people);
    } else if (in.kind == FlightKind::Charter) {
        add(p.income, "Charter", kCharterPerHour * std::max(h, kCharterMinHours));
    }
    if (in.cargoLb > 0) {
        add(p.income, "Cargo", (kCargoBasePer100Lb + kCargoPer100LbPerNm * in.distanceNm) * in.cargoLb / 100.0);
    }

    // Costs.
    if (in.fuelBurnLb) {
        add(p.costs, "Fuel", -static_cast<double>(*in.fuelBurnLb) / 100.0 * in.fuelPriceCentsPer100Lb / 100.0);
        p.fuelIncluded = true;
    }
    add(p.costs, "Landing fee", -kLandingPer1000LbMlw * in.mlwLb / 1000.0);
    if (!delivery && in.kind != FlightKind::Charter) {
        add(p.costs, "Departure / gate", -kGate[w]);
        add(p.costs, "Ground handling", -kHandling[w]);
    }
    if (in.kind == FlightKind::Charter) add(p.costs, "GA services", -kGaServices);
    add(p.costs, "ATC / navigation", -kAtcPerNm[w] * in.distanceNm);
    if (!delivery) {
        const double perPassenger = kCatering[h < 3 ? 0 : h < 8 ? 1 : 2] * (vip ? 2 : 1);
        add(p.costs, "Catering", -perPassenger * people);
        add(p.costs, "Cargo handling", -kCargoHandlingPer1000Lb * in.cargoLb / 1000.0);
    }
    add(p.costs, "Pilots", -pilotsFor(h) * (vip ? kVipPilotPerHour : kPilotPerHour[w]) * h);
    if (!delivery && in.role != "freighter") {
        const int attendants = flightAttendantsFor(in.seatsInstalled) * (vip ? 3 : 1);
        add(p.costs, "Flight attendants", -attendants * (vip ? kVipFlightAttendantPerHour : kFlightAttendantPerHour) * h);
    }
    add(p.costs, "Maintenance", -kMaintenancePerHour[w] * h);

    for (const MoneyLine& m : p.income) p.totalCents += m.cents;
    for (const MoneyLine& m : p.costs) p.totalCents += m.cents;
    return p;
}

}  // namespace blocktime
