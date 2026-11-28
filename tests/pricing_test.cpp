// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "pricing.h"
#include "rates.h"

using namespace blocktime;

namespace {

// 900 nm at 450 kt (2.5 h), BBJ2 MLW, no plan so no fuel: the base of every test (design.md §10-12).
PriceInput vip() {
    PriceInput in;
    in.kind = FlightKind::Vip;
    in.family = "737";
    in.role = "executive";
    in.seatsInstalled = 48;
    in.cabins = {{"economy", 46}};
    in.distanceNm = 900;
    in.cruiseSpeedKt = 450;
    in.mlwLb = 146300;
    in.fuelPriceCentsPer100Lb = 4100;
    return in;
}

// "Label amount" per line, amounts in whole dollars, so a failure shows the whole list.
std::vector<std::string> lines(const std::vector<MoneyLine>& money) {
    std::vector<std::string> out;
    for (const MoneyLine& m : money) out.push_back(m.label + " " + std::to_string(m.cents / 100));
    return out;
}

using Lines = std::vector<std::string>;

}  // namespace

TEST(Pricing, VipFlight) {
    const Preview p = pricePreview(vip(), Rates{});
    EXPECT_EQ(lines(p.income), (Lines{"VIP fares 28150"}));
    EXPECT_EQ(lines(p.costs), (Lines{"Landing fee -660", "Departure / gate -300", "Ground handling -900",
                                     "ATC / navigation -900", "Catering -460", "Pilots -1250",
                                     "Flight attendants -565", "Maintenance -750"}));
    EXPECT_EQ(p.totalCents, 2'236'500);
    EXPECT_FALSE(p.fuelIncluded);
}

TEST(Pricing, CharterFlight) {
    PriceInput in = vip();
    in.kind = FlightKind::Charter;
    const Preview p = pricePreview(in, Rates{});
    EXPECT_EQ(lines(p.income), (Lines{"Charter 18750"}));
    EXPECT_EQ(lines(p.costs), (Lines{"Landing fee -660", "GA services -1500", "ATC / navigation -900", "Catering -460",
                                     "Pilots -1250", "Flight attendants -565", "Maintenance -750"}));
    EXPECT_EQ(p.totalCents, 1'266'500);
}

TEST(Pricing, CharterTwoHourMinimum) {
    PriceInput in = vip();
    in.kind = FlightKind::Charter;
    in.planBlockS = 5400;
    EXPECT_EQ(lines(pricePreview(in, Rates{}).income), (Lines{"Charter 15000"}));
}

TEST(Pricing, AirlinerWithBellyCargo) {
    PriceInput in = vip();
    in.kind = FlightKind::Passenger;
    in.role = "airliner";
    in.seatsInstalled = 160;
    in.cabins = {{"economy", 140}, {"business", 15}};
    in.cargoLb = 3000;
    const Preview p = pricePreview(in, Rates{});
    EXPECT_EQ(lines(p.income), (Lines{"Economy fares 22050", "Business fares 7090", "Cargo 1205"}));
    EXPECT_EQ(lines(p.costs), (Lines{"Landing fee -660", "Departure / gate -300", "Ground handling -900",
                                     "ATC / navigation -900", "Catering -775", "Cargo handling -120", "Pilots -1000",
                                     "Flight attendants -550", "Maintenance -750"}));
    EXPECT_EQ(p.totalCents, 2'439'000);
}

TEST(Pricing, DeliveryIsALoss) {
    PriceInput in = vip();
    in.kind = FlightKind::Delivery;
    in.cabins = {};
    const Preview p = pricePreview(in, Rates{});
    EXPECT_TRUE(p.income.empty());
    EXPECT_EQ(lines(p.costs), (Lines{"Landing fee -660", "ATC / navigation -900", "Pilots -1250", "Maintenance -750"}));
    EXPECT_EQ(p.totalCents, -356'000);
}

TEST(Pricing, FuelLineWhenFuelAddedKnown) {
    PriceInput in = vip();
    in.fuelAddedLb = 6000;
    const Preview p = pricePreview(in, Rates{});
    ASSERT_FALSE(p.costs.empty());
    EXPECT_EQ(lines(p.costs)[0], "Fuel -2460");
    EXPECT_TRUE(p.fuelIncluded);
    EXPECT_EQ(p.totalCents, 2'236'500 - 246'000);
}

TEST(Pricing, Crew) {
    EXPECT_EQ(flightAttendantsFor(19), 0);
    EXPECT_EQ(flightAttendantsFor(20), 1);
    EXPECT_EQ(flightAttendantsFor(50), 1);
    EXPECT_EQ(flightAttendantsFor(51), 2);
    EXPECT_EQ(flightAttendantsFor(100), 2);
    EXPECT_EQ(flightAttendantsFor(101), 3);
    EXPECT_EQ(flightAttendantsFor(150), 3);
    EXPECT_EQ(flightAttendantsFor(151), 4);
    EXPECT_EQ(pilotsFor(8.0), 2);
    EXPECT_EQ(pilotsFor(8.1), 3);
    EXPECT_EQ(pilotsFor(12.0), 3);
    EXPECT_EQ(pilotsFor(12.1), 4);
}

TEST(Pricing, CateringBands) {
    PriceInput in = vip();
    in.role = "airliner";
    in.kind = FlightKind::Passenger;
    const auto catering = [&](double hours) {
        in.planBlockS = static_cast<int>(hours * 3600);
        for (const MoneyLine& m : pricePreview(in, Rates{}).costs) {
            if (m.label == "Catering") return m.cents / 100;
        }
        return Cents{0};
    };
    EXPECT_EQ(catering(2.9), -230);
    EXPECT_EQ(catering(3.0), -690);
    EXPECT_EQ(catering(8.0), -1610);
}

TEST(Pricing, WidebodyRates) {
    PriceInput in = vip();
    in.family = "777";
    in.role = "airliner";
    in.kind = FlightKind::Passenger;
    in.seatsInstalled = 48;
    in.mlwLb = 554000;
    const Preview p = pricePreview(in, Rates{});
    EXPECT_EQ(lines(p.costs), (Lines{"Landing fee -2495", "Departure / gate -900", "Ground handling -2800",
                                     "ATC / navigation -1800", "Catering -230", "Pilots -1500",
                                     "Flight attendants -140", "Maintenance -1750"}));
}

TEST(Pricing, ActualBlockHoursOnlyChangeCrewAndMaintenance) {
    PriceInput in = vip();
    in.kind = FlightKind::Passenger;
    in.role = "airliner";
    in.seatsInstalled = 160;
    in.cabins = {{"economy", 140}};
    in.planBlockS = 7200;
    const Preview planned = pricePreview(in, Rates{});
    in.actualBlockHours = 3.5;
    const Preview actual = pricePreview(in, Rates{});
    EXPECT_EQ(lines(actual.income), lines(planned.income));
    EXPECT_EQ(lines(actual.costs), (Lines{"Landing fee -660", "Departure / gate -300", "Ground handling -900",
                                          "ATC / navigation -900", "Catering -700", "Pilots -1400",
                                          "Flight attendants -770", "Maintenance -1050"}));
}

TEST(Pricing, ActualBlockHoursChangeThePilotCount) {
    PriceInput in = vip();
    in.planBlockS = 27000;
    in.actualBlockHours = 8.5;  // three pilots, at the VIP rate
    for (const MoneyLine& m : pricePreview(in, Rates{}).costs) {
        if (m.label == "Pilots") EXPECT_EQ(m.cents, -3 * 25000 * 85 / 10);
    }
}

TEST(Pricing, LinesCarryTheirCategory) {
    PriceInput in = vip();
    in.fuelAddedLb = 6000;
    const Preview p = pricePreview(in, Rates{});
    std::vector<std::string> codes;
    for (const MoneyLine& m : p.income) codes.push_back(m.category);
    for (const MoneyLine& m : p.costs) codes.push_back(m.category);
    EXPECT_EQ(codes, (Lines{"vip_fares", "fuel", "landing_fee", "gate", "handling", "atc", "catering", "pilots",
                            "flight_attendants", "maintenance"}));
}

TEST(Pricing, EveryKindsCategories) {
    PriceInput in = vip();
    in.kind = FlightKind::Charter;
    std::vector<std::string> codes;
    for (const MoneyLine& m : pricePreview(in, Rates{}).income) codes.push_back(m.category);
    for (const MoneyLine& m : pricePreview(in, Rates{}).costs) codes.push_back(m.category);
    EXPECT_EQ(codes, (Lines{"charter", "landing_fee", "ga_services", "atc", "catering", "pilots",
                            "flight_attendants", "maintenance"}));
    in.kind = FlightKind::Passenger;
    in.role = "airliner";
    in.cabins = {{"economy", 10}, {"business", 2}, {"first", 1}};
    in.cargoLb = 1000;
    codes.clear();
    for (const MoneyLine& m : pricePreview(in, Rates{}).income) codes.push_back(m.category);
    EXPECT_EQ(codes, (Lines{"economy_fares", "business_fares", "first_fares", "cargo"}));
}

TEST(Pricing, RatesAreUsed) {
    PriceInput in = vip();
    in.fuelAddedLb = 6000;
    Rates rates;
    rates.fuelPer100Lb = 8200;
    in.fuelPriceCentsPer100Lb = rates.fuelPer100Lb;  // the planned flight's locked price is what's charged
    EXPECT_EQ(lines(pricePreview(in, rates).costs)[0], "Fuel -4920");
    rates.narrowbody.gate = 600;
    EXPECT_EQ(lines(pricePreview(in, rates).costs)[2], "Departure / gate -600");
}

TEST(Pricing, EstimatedBlockHours) { EXPECT_DOUBLE_EQ(estimatedBlockHours(900, 450), 2.5); }

// D-223: once a plan is in, the fuel line is the plan's ramp fuel less the fuel on record.
TEST(Pricing, FuelIsTheFuelToAdd) {
    PriceInput in = vip();
    in.planBlockS = 9000;
    in.planRampFuelLb = 9200;
    in.fuelOnRecordLb = 3300;
    const Preview p = pricePreview(in, Rates{});
    ASSERT_FALSE(p.costs.empty());
    EXPECT_EQ(lines(p.costs)[0], "Fuel -2420");  // 5,900 lb × $0.41 = $2,419
    EXPECT_TRUE(p.fuelIncluded);
}

// Review Focus 1: the airplane already holds the plan's fuel. Nothing to buy, but the fuel is known.
// Review Focus 3 (milestone 16): nothing to add is a $0 fuel line, "0 lb added", so the report can show it.
TEST(Pricing, NoFuelToAddIsAZeroLine) {
    PriceInput in = vip();
    in.planBlockS = 9000;
    in.planRampFuelLb = 3000;
    in.fuelOnRecordLb = 3300;
    const Preview p = pricePreview(in, Rates{});
    const auto fuel = std::find_if(p.costs.begin(), p.costs.end(), [](const MoneyLine& m) { return m.category == "fuel"; });
    ASSERT_NE(fuel, p.costs.end());
    EXPECT_EQ(fuel->cents, 0);
    EXPECT_EQ(fuel->quantities.at(0).value, 0);
    EXPECT_TRUE(p.fuelIncluded);
}

// Settlement charges the fuel really added (D-109), whatever the plan said.
TEST(Pricing, FuelAddedReplacesThePlan) {
    PriceInput in = vip();
    in.planBlockS = 9000;
    in.planRampFuelLb = 9200;
    in.fuelOnRecordLb = 3300;
    in.fuelAddedLb = 6000;
    EXPECT_EQ(lines(pricePreview(in, Rates{}).costs)[0], "Fuel -2460");
}

TEST(Pricing, EstimateWithoutAPlan) {
    const PriceInput in = vip();
    EXPECT_DOUBLE_EQ(blockHoursFor(in), 2.5);
    const Preview p = pricePreview(in, Rates{});
    for (const MoneyLine& m : p.costs) EXPECT_NE(m.category, "fuel");
    EXPECT_FALSE(p.fuelIncluded);
}

TEST(Pricing, PlanBlockTimeReplacesTheEstimate) {
    PriceInput in = vip();
    in.planBlockS = 9060;
    EXPECT_DOUBLE_EQ(blockHoursFor(in), 9060 / 3600.0);
}

// SOULS on the in-flight panel (D-203): the crew BlockTime pays for (design.md section 10).
TEST(Pricing, CrewFor) {
    EXPECT_EQ(crewFor("executive", false, 48, 1.1, 3), 5);   // the BBJ2: 2 pilots, 1 × 3 flight attendants
    EXPECT_EQ(crewFor("airliner", false, 162, 1.1, 3), 6);   // 2 pilots, 4 flight attendants
    EXPECT_EQ(crewFor("freighter", false, 0, 1.1, 3), 2);    // pilots only
    EXPECT_EQ(crewFor("airliner", false, 162, 9.0, 3), 7);   // 3 pilots over 8 hours
    EXPECT_EQ(crewFor("airliner", true, 162, 1.1, 3), 2);    // a delivery flies the pilots only
}

namespace {

// The line's amount from its working: the quantities multiplied, × the rate, ÷ the basis, rounded to $5.
Cents fromWorking(const MoneyLine& m) {
    double dollars = m.rateDollars / m.rateBasis;
    for (const Quantity& q : m.quantities) dollars *= q.value;
    return roundToFiveDollars(static_cast<Cents>(std::llround(dollars * 100)));
}

void expectEveryLineMultipliesOut(const PriceInput& in, const char* what) {
    const Preview p = pricePreview(in, Rates{});
    for (const MoneyLine& m : p.income) EXPECT_EQ(fromWorking(m), m.cents) << what << ": " << m.label;
    for (const MoneyLine& m : p.costs) EXPECT_EQ(fromWorking(m), -m.cents) << what << ": " << m.label;
}

}  // namespace

// Review 4.4: every line carries how it was worked out, and the working gives the amount.
TEST(Pricing, EveryLineMultipliesOut) {
    PriceInput in = vip();
    in.planBlockS = 9060;
    in.planRampFuelLb = 9200;
    in.fuelOnRecordLb = 3300;
    expectEveryLineMultipliesOut(in, "vip");
    in.kind = FlightKind::Charter;
    expectEveryLineMultipliesOut(in, "charter");
    in.planBlockS = 4000;  // under the charter minimum
    expectEveryLineMultipliesOut(in, "short charter");
    in.planBlockS = 9060;
    in.kind = FlightKind::Passenger;
    in.role = "airliner";
    in.seatsInstalled = 160;
    in.cabins = {{"economy", 140}, {"business", 15}, {"first", 4}};
    in.cargoLb = 3170;
    expectEveryLineMultipliesOut(in, "passenger");
    in.actualBlockHours = 2.71;
    in.fuelAddedLb = 6123;
    expectEveryLineMultipliesOut(in, "settled passenger");
    in.kind = FlightKind::Cargo;
    in.role = "freighter";
    in.family = "777";
    in.cabins = {};
    in.cargoLb = 180000;
    expectEveryLineMultipliesOut(in, "cargo");
    in.kind = FlightKind::Delivery;
    in.cargoLb = 0;
    expectEveryLineMultipliesOut(in, "delivery");
}

TEST(Pricing, WorkingText) {
    PriceInput in = vip();
    in.planBlockS = 9000;
    in.planRampFuelLb = 9200;
    in.fuelOnRecordLb = 3300;
    const Preview p = pricePreview(in, Rates{});
    const auto working = [&](const char* category) {
        for (const MoneyLine& m : p.costs) {
            if (m.category == category) return workingText(m);
        }
        return std::string("none");
    };
    EXPECT_EQ(working("pilots"), "2.5 h × 2 pilots × $250");
    EXPECT_EQ(working("gate"), "$300 / departure");
    EXPECT_EQ(working("fuel"), "5,900 lb × $41 / 100 lb");
    EXPECT_EQ(working("landing_fee"), "146,300 lb MLW × $4.50 / 1,000 lb");
    EXPECT_EQ(working("atc"), "900 nm × $1");
    EXPECT_EQ(workingText(MoneyLine{"lease", "Lease", -500}), "");  // a ledger line read back: no working
}

TEST(Pricing, WorkingTextDecimals) {
    MoneyLine m{"fuel", "Fuel", -500};
    m.quantities = {{2.5167, "h"}, {1043.27, "nm"}};
    m.rateDollars = 0.4125;
    EXPECT_EQ(workingText(m), "2.52 h × 1,043.3 nm × $0.4125");  // hours to two decimals (D-238's open point)
    m.rateDollars = 157.5;
    EXPECT_EQ(workingText(m), "2.52 h × 1,043.3 nm × $157.50");
    m.rateDollars = 1250;
    m.rateUnit = "h";
    EXPECT_EQ(workingText(m), "2.52 h × 1,043.3 nm × $1,250 / h");
}
