// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "pricing.h"
#include "rates.h"

using namespace blocktime;

namespace {

// 900 nm, 2.5 h, BBJ2 MLW, no fuel: the base of every test (design.md §10-12).
PriceInput vip() {
    PriceInput in;
    in.kind = FlightKind::Vip;
    in.family = "737";
    in.role = "executive";
    in.seatsInstalled = 48;
    in.cabins = {{"economy", 46}};
    in.distanceNm = 900;
    in.blockHours = 2.5;
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
    in.blockHours = 1.5;
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

TEST(Pricing, FuelLineWhenBurnKnown) {
    PriceInput in = vip();
    in.fuelBurnLb = 6000;
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
        in.blockHours = hours;
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
    in.blockHours = 2.0;
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
    in.blockHours = 7.5;
    in.actualBlockHours = 8.5;  // three pilots, at the VIP rate
    for (const MoneyLine& m : pricePreview(in, Rates{}).costs) {
        if (m.label == "Pilots") EXPECT_EQ(m.cents, -3 * 25000 * 85 / 10);
    }
}

TEST(Pricing, LinesCarryTheirCategory) {
    PriceInput in = vip();
    in.fuelBurnLb = 6000;
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
    in.fuelBurnLb = 6000;
    Rates rates;
    rates.fuelPer100Lb = 8200;
    in.fuelPriceCentsPer100Lb = rates.fuelPer100Lb;  // the planned flight's locked price is what's charged
    EXPECT_EQ(lines(pricePreview(in, rates).costs)[0], "Fuel -4920");
    rates.narrowbody.gate = 600;
    EXPECT_EQ(lines(pricePreview(in, rates).costs)[2], "Departure / gate -600");
}

TEST(Pricing, EstimatedBlockHours) { EXPECT_DOUBLE_EQ(estimatedBlockHours(900, 450), 2.5); }

// SOULS on the in-flight panel (D-203): the crew BlockTime pays for (design.md section 10).
TEST(Pricing, CrewFor) {
    EXPECT_EQ(crewFor("executive", false, 48, 1.1, 3), 5);   // the BBJ2: 2 pilots, 1 × 3 flight attendants
    EXPECT_EQ(crewFor("airliner", false, 162, 1.1, 3), 6);   // 2 pilots, 4 flight attendants
    EXPECT_EQ(crewFor("freighter", false, 0, 1.1, 3), 2);    // pilots only
    EXPECT_EQ(crewFor("airliner", false, 162, 9.0, 3), 7);   // 3 pilots over 8 hours
    EXPECT_EQ(crewFor("airliner", true, 162, 1.1, 3), 2);    // a delivery flies the pilots only
}
