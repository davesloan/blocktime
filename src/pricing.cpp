// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pricing.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace blocktime {

namespace {

constexpr double kTaxiAndClimbHours = 0.5;

// How a line is worked out: the quantities, the rate and what it's per.
struct Working {
    std::vector<Quantity> quantities;
    double rateDollars = 0;
    int rateBasis = 1;
    const char* rateUnit = "";
};

Working per(std::vector<Quantity> quantities, double rateDollars, int rateBasis = 1, const char* rateUnit = "") {
    return {std::move(quantities), rateDollars, rateBasis, rateUnit};
}

// One line, worked out from its working so the two can't disagree: exact cents, then rounded to $5 (D-080). A cost
// is negative. Lines worth nothing are left out.
void add(std::vector<MoneyLine>& lines, const char* category, const char* label, Working w, bool isCost = false) {
    double dollars = w.rateDollars;
    for (const Quantity& q : w.quantities) dollars *= q.value;
    dollars /= w.rateBasis;
    const Cents cents = roundToFiveDollars(static_cast<Cents>(std::llround(dollars * 100)));
    if (cents == 0) return;
    lines.push_back(
        {category, label, isCost ? -cents : cents, std::move(w.quantities), w.rateDollars, w.rateBasis, w.rateUnit});
}

void cost(std::vector<MoneyLine>& lines, const char* category, const char* label, Working w) {
    add(lines, category, label, std::move(w), true);
}

Quantity passengers(int n) { return {static_cast<double>(n), "passengers"}; }
Quantity pounds(int lb, const char* unit = "lb") { return {static_cast<double>(lb), unit}; }

// One decimal, dropped when it's 0: "2.5", "1,043.3", "900".
std::string quantityText(double value) {
    const long long tenths = std::llround(value * 10);
    if (tenths % 10 == 0) return formatNumber(tenths / 10);
    return formatNumber(tenths / 10) + "." + std::to_string(std::abs(tenths % 10));
}

// Whole dollars bare, else two decimals, more (up to 4) only when needed: "$250", "$157.50", "$0.4125".
std::string rateText(double dollars) {
    const long long tenThousandths = std::llround(dollars * 10000);
    const std::string whole = "$" + formatNumber(tenThousandths / 10000);
    const long long fraction = tenThousandths % 10000;
    if (fraction == 0) return whole;
    std::string digits = std::to_string(10000 + fraction).substr(1);  // always four digits
    while (digits.size() > 2 && digits.back() == '0') digits.pop_back();
    return whole + "." + digits;
}

int passengersIn(const std::vector<CabinLoad>& cabins, const char* cabinClass) {
    for (const CabinLoad& c : cabins) {
        if (c.cabinClass == cabinClass) return c.passengers;
    }
    return 0;
}

}  // namespace

std::string workingText(const MoneyLine& line) {
    if (line.rateDollars == 0) return "";
    std::string out;
    for (const Quantity& q : line.quantities) out += quantityText(q.value) + " " + q.unit + " × ";
    out += rateText(line.rateDollars);
    if (!line.rateUnit.empty()) {
        out += " / ";
        if (line.rateBasis > 1) out += formatNumber(line.rateBasis) + " ";
        out += line.rateUnit;
    }
    return out;
}

int pilotsFor(double blockHours) { return blockHours > 12 ? 4 : blockHours > 8 ? 3 : 2; }

int flightAttendantsFor(int seatsInstalled) { return seatsInstalled < 20 ? 0 : (seatsInstalled + 49) / 50; }

double estimatedBlockHours(double distanceNm, int cruiseSpeedKt) {
    return distanceNm / cruiseSpeedKt + kTaxiAndClimbHours;
}

double blockHoursFor(const PriceInput& in) {
    return in.planBlockS ? *in.planBlockS / 3600.0 : estimatedBlockHours(in.distanceNm, in.cruiseSpeedKt);
}

Preview pricePreview(const PriceInput& in, const Rates& r) {
    Preview p;
    const FamilyRates& f = in.family == "777" ? r.widebody : r.narrowbody;
    const bool vip = in.role == "executive";
    const bool delivery = in.kind == FlightKind::Delivery;
    const double h = blockHoursFor(in);
    const double crewH = in.actualBlockHours.value_or(h);  // D-109
    int people = 0;
    for (const CabinLoad& c : in.cabins) people += c.passengers;

    // Income.
    const double economyFare = r.economyBase + r.economyPerNm * in.distanceNm;
    if (in.kind == FlightKind::Passenger) {
        add(p.income, "economy_fares", "Economy fares",
            per({passengers(passengersIn(in.cabins, "economy"))}, economyFare));
        add(p.income, "business_fares", "Business fares",
            per({passengers(passengersIn(in.cabins, "business"))}, r.businessTimesEconomy * economyFare));
        add(p.income, "first_fares", "First fares",
            per({passengers(passengersIn(in.cabins, "first"))}, r.firstTimesEconomy * economyFare));
    } else if (in.kind == FlightKind::Vip) {
        add(p.income, "vip_fares", "VIP fares",
            per({passengers(people)}, r.vipFareBase + r.vipFarePerNm * in.distanceNm));
    } else if (in.kind == FlightKind::Charter) {
        add(p.income, "charter", "Charter", per({{std::max(h, r.charterMinHours), "h"}}, r.charterPerHour));
    }
    if (in.cargoLb > 0) {
        add(p.income, "cargo", "Cargo",
            per({pounds(in.cargoLb)}, r.cargoBasePer100Lb + r.cargoPer100LbPerNm * in.distanceNm, 100, "lb"));
    }

    // Costs.
    std::optional<int> fuelLb = in.fuelAddedLb;
    if (!fuelLb && in.planRampFuelLb) fuelLb = std::max(0, *in.planRampFuelLb - in.fuelOnRecordLb);  // D-223
    if (fuelLb) {
        cost(p.costs, "fuel", "Fuel",
             per({pounds(*fuelLb)}, static_cast<double>(in.fuelPriceCentsPer100Lb) / 100.0, 100, "lb"));
        p.fuelIncluded = true;
    }
    cost(p.costs, "landing_fee", "Landing fee", per({pounds(in.mlwLb, "lb MLW")}, r.landingPer1000LbMlw, 1000, "lb"));
    if (!delivery && in.kind != FlightKind::Charter) {
        cost(p.costs, "gate", "Departure / gate", per({}, f.gate, 1, "departure"));
        cost(p.costs, "handling", "Ground handling", per({}, f.handling, 1, "departure"));
    }
    if (in.kind == FlightKind::Charter) {
        cost(p.costs, "ga_services", "GA services", per({}, r.gaServices, 1, "departure"));
    }
    cost(p.costs, "atc", "ATC / navigation", per({{in.distanceNm, "nm"}}, f.atcPerNm));
    if (!delivery) {
        const double perPassenger = r.catering[h < 3 ? 0 : h < 8 ? 1 : 2] * (vip ? r.vipCateringTimes : 1);
        cost(p.costs, "catering", "Catering", per({passengers(people)}, perPassenger));
        cost(p.costs, "cargo_handling", "Cargo handling",
             per({pounds(in.cargoLb)}, r.cargoHandlingPer1000Lb, 1000, "lb"));
    }
    const Quantity crewHours{crewH, "h"};
    cost(p.costs, "pilots", "Pilots",
         per({crewHours, {static_cast<double>(pilotsFor(crewH)), "pilots"}}, vip ? r.vipPilotPerHour : f.pilotPerHour));
    if (!delivery && in.role != "freighter") {
        const double attendants = flightAttendantsFor(in.seatsInstalled) * (vip ? r.vipFlightAttendantsTimes : 1);
        cost(p.costs, "flight_attendants", "Flight attendants",
             per({crewHours, {attendants, "attendants"}},
                 vip ? r.vipFlightAttendantPerHour : r.flightAttendantPerHour));
    }
    cost(p.costs, "maintenance", "Maintenance", per({crewHours}, f.maintenancePerHour));

    for (const MoneyLine& m : p.income) p.totalCents += m.cents;
    for (const MoneyLine& m : p.costs) p.totalCents += m.cents;
    return p;
}

int crewFor(std::string_view role, bool delivery, int seatsInstalled, double blockHours, int vipAttendantsTimes) {
    int crew = pilotsFor(blockHours);
    if (!delivery && role != "freighter") {
        crew += flightAttendantsFor(seatsInstalled) * (role == "executive" ? vipAttendantsTimes : 1);
    }
    return crew;
}

}  // namespace blocktime
