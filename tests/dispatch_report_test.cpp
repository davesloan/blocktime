// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "dispatch_report.h"
#include "pricing.h"
#include "rates.h"

using namespace blocktime;

namespace {

// 900 nm, BBJ2, a plan of 2:30 block and 9,200 lb ramp fuel with 3,300 lb on board.
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
    in.planBlockS = 9000;
    in.planRampFuelLb = 9200;
    in.fuelOnRecordLb = 3300;
    return in;
}

PriceInput passengers() {
    PriceInput in = vip();
    in.kind = FlightKind::Passenger;
    in.role = "airliner";
    in.seatsInstalled = 160;
    in.cabins = {{"business", 16}, {"economy", 124}};
    in.cargoLb = 2000;
    in.planBlockS = 6240;  // 1:44
    return in;
}

const MoneyLine* find(const Preview& p, const std::string& category) {
    for (const auto* lines : {&p.income, &p.costs}) {
        for (const MoneyLine& m : *lines) {
            if (m.category == category) return &m;
        }
    }
    return nullptr;
}

double sweeps(const Ring& r) {
    double sum = 0;
    for (const RingSlice& s : r.slices) sum += s.sweep;
    return sum;
}

}  // namespace

TEST(SettleAccepted, OnlyCrewMaintenanceAndFuelMove) {
    const Preview accepted = pricePreview(passengers(), Rates{});
    const Preview settled = settleAccepted(accepted, 2.1, 6400, 4100);
    for (const char* fixed : {"economy_fares", "business_fares", "cargo", "landing_fee", "gate", "handling", "atc",
                              "catering", "cargo_handling"}) {
        ASSERT_NE(find(accepted, fixed), nullptr) << fixed;
        ASSERT_NE(find(settled, fixed), nullptr) << fixed;
        EXPECT_EQ(find(settled, fixed)->cents, find(accepted, fixed)->cents) << fixed;
    }
    const MoneyLine& pilots = *find(settled, "pilots");
    EXPECT_EQ(pilots.cents, -roundToFiveDollars(std::llround(2.1 * 2 * find(accepted, "pilots")->rateDollars * 100)));
    EXPECT_DOUBLE_EQ(pilots.quantities.at(0).value, 2.1);
    EXPECT_EQ(find(settled, "fuel")->cents, -roundToFiveDollars(std::llround(6400 * 41.0 / 100 * 100)));
    Cents total = 0;
    for (const auto* lines : {&settled.income, &settled.costs}) {
        for (const MoneyLine& m : *lines) total += m.cents;
    }
    EXPECT_EQ(settled.totalCents, total);
}

TEST(SettleAccepted, ThirdPilotOverEightHours) {
    PriceInput in = passengers();
    in.planBlockS = static_cast<int>(7.9 * 3600);
    const Preview settled = settleAccepted(pricePreview(in, Rates{}), 8.2, 6400, 4100);
    EXPECT_DOUBLE_EQ(find(settled, "pilots")->quantities.at(1).value, 3);
}

TEST(SettleAccepted, AddsAMissingFuelLine) {
    Preview accepted = pricePreview(passengers(), Rates{});
    std::erase_if(accepted.costs, [](const MoneyLine& m) { return m.category == "fuel"; });
    const Preview settled = settleAccepted(accepted, 1.8, 800, 4100);
    ASSERT_NE(find(settled, "fuel"), nullptr);
    EXPECT_EQ(find(settled, "fuel")->cents, -33000);  // 800 lb × $41 / 100 lb = $328, to $5
}

TEST(Working, HoursTwoDecimals) {
    MoneyLine pilots{"pilots", "Pilots", -148000, {{2.96, "h"}, {2, "pilots"}}, 250, 1, ""};
    EXPECT_EQ(workingText(pilots), "2.96 h × 2 pilots × $250");
    pilots.quantities[0].value = 2.5;
    EXPECT_EQ(workingText(pilots), "2.5 h × 2 pilots × $250");
}

TEST(Pricing, ZeroFuelToAddKeepsTheFuelLine) {
    PriceInput in = vip();
    in.planRampFuelLb = 3000;  // less than the 3,300 on board
    const Preview p = pricePreview(in, Rates{});
    ASSERT_NE(find(p, "fuel"), nullptr);
    EXPECT_EQ(find(p, "fuel")->cents, 0);
    EXPECT_TRUE(p.fuelIncluded);
}

TEST(Board, RowsAndText) {
    const std::vector<BoardRow> rows = boardRows(pricePreview(vip(), Rates{}));
    ASSERT_GE(rows.size(), 3u);
    EXPECT_EQ(rows.front().id, "vip_fares");
    EXPECT_EQ(rows.front().label, "VIP fares");
    EXPECT_EQ(rows.front().tone, BoardRow::Tone::Income);
    EXPECT_EQ(rows.front().value.front(), '+');
    EXPECT_EQ(rows.back().id, "profit");
    EXPECT_EQ(rows.back().label, "Profit");
    EXPECT_EQ(rows.back().tone, BoardRow::Tone::Profit);
    EXPECT_EQ(boardText({"fuel", "Fuel", "-$2,420", BoardRow::Tone::Cost}), "FUEL         -$2,420");
    EXPECT_EQ(boardText({"x", "Flight attendants", "-$12,345", BoardRow::Tone::Cost}), "FLIGHT ATTE -$12,345");
    EXPECT_EQ(boardText({"x", "Flight attendants", "-$12,345", BoardRow::Tone::Cost}).size(), 20u);
    EXPECT_EQ(boardLabel("flight_attendants", "Flight attendants"), "Cabin crew");
    EXPECT_EQ(boardLabel("cargo_handling", "Cargo handling"), "Cargo hdlg");
    EXPECT_EQ(boardLabel("atc", "ATC / navigation"), "ATC / nav");
    EXPECT_EQ(boardLabel("landing_fee", "Landing fee"), "Landing");
    EXPECT_EQ(boardLabel("gate", "Departure / gate"), "Gate");
    EXPECT_EQ(boardLabel("handling", "Ground handling"), "Handling");
    EXPECT_EQ(boardLabel("pilots", "Pilots"), "Pilots");
    EXPECT_EQ(boardValue(-124000, BoardRow::Tone::Loss), "-$1,240");
    EXPECT_EQ(boardValue(876000, BoardRow::Tone::Profit), "$8,760");
}

TEST(Board, DeliveryHasNoFaresAndALoss) {
    PriceInput in = vip();
    in.kind = FlightKind::Delivery;
    in.cabins = {};
    const std::vector<BoardRow> rows = boardRows(pricePreview(in, Rates{}));
    for (const BoardRow& r : rows) EXPECT_NE(r.tone, BoardRow::Tone::Income) << r.id;
    EXPECT_EQ(rows.back().id, "profit");
    EXPECT_EQ(rows.back().tone, BoardRow::Tone::Loss);
    EXPECT_EQ(rows.back().value.front(), '-');
}

TEST(Ring, ProfitCircleIsTheFares) {
    const Preview p = pricePreview(vip(), Rates{});
    ASSERT_GT(p.totalCents, 0);
    const Ring r = ring(p);
    EXPECT_FALSE(r.loss);
    EXPECT_NEAR(sweeps(r), 1.0, 1e-9);
    EXPECT_EQ(r.slices.front().id, "fuel");
    EXPECT_EQ(r.slices.front().family, "fuel");
    EXPECT_EQ(r.slices.back().id, "profit");
    EXPECT_EQ(r.slices.back().family, "profit");
    EXPECT_NEAR(r.slices.back().sweep, static_cast<double>(p.totalCents) / p.income.at(0).cents, 1e-9);
    EXPECT_EQ(ringFamily("landing_fee"), "airport");
    EXPECT_EQ(ringFamily("pilots"), "crew");
    EXPECT_EQ(ringFamily("catering"), "service");
}

TEST(Ring, LossCircleIsTheCosts) {
    PriceInput in = vip();
    in.kind = FlightKind::Delivery;
    in.cabins = {};
    const Ring r = ring(pricePreview(in, Rates{}));
    EXPECT_TRUE(r.loss);
    for (const RingSlice& s : r.slices) EXPECT_NE(s.id, "profit");
    EXPECT_NEAR(sweeps(r), 1.0, 1e-9);
}

TEST(Ring, ZeroLineHasNoSlice) {
    PriceInput in = vip();
    in.planRampFuelLb = 3000;
    const Ring r = ring(pricePreview(in, Rates{}));
    double next = 0;
    for (const RingSlice& s : r.slices) {
        EXPECT_NE(s.id, "fuel");
        EXPECT_NEAR(s.start, next, 1e-12) << s.id;
        next = s.start + s.sweep;
    }
    EXPECT_NEAR(next, 1.0, 1e-9);
}

TEST(Working, FuelDerivation) {
    const Preview p = pricePreview(vip(), Rates{});
    const MoneyLine& fuel = *find(p, "fuel");
    EXPECT_EQ(workingLines(fuel, {9200, 3300, 0, false}),
              (std::vector<std::string>{"9,200 lb block fuel", "− 3,300 lb on board",
                                        "= 5,900 lb added × $41 / 100 lb", "Price locked when you dispatched"}));
    EXPECT_EQ(workingLines(fuel, {3000, 3300, 0, false}).at(2), "= 0 lb added × $41 / 100 lb");
    const MoneyLine& pilots = *find(p, "pilots");
    EXPECT_EQ(workingLines(pilots, {9200, 3300, 0, false}),
              (std::vector<std::string>{workingText(pilots), "Charged on the actual block time"}));
    EXPECT_EQ(workingLines(pilots, {9200, 3300, 0, true}), (std::vector<std::string>{workingText(pilots)}));
    const MoneyLine& fares = p.income.at(0);
    EXPECT_EQ(workingLines(fares, {9200, 3300, 0, false}), (std::vector<std::string>{workingText(fares)}));
}

TEST(Changes, FuelAndProfitAfterReimport) {
    PriceInput in = vip();
    in.planRampFuelLb = 3300 + 4900;
    const Preview before = pricePreview(in, Rates{});
    in.planRampFuelLb = 3300 + 5900;
    const Preview after = pricePreview(in, Rates{});
    const auto changes = changesSince(before, {9000, 4900, 0}, after, {9000, 5900, 1000});
    ASSERT_EQ(changes.size(), 2u);
    EXPECT_EQ(changes[0].id, "fuel");
    EXPECT_EQ(changes[0].was, find(before, "fuel")->cents);
    EXPECT_EQ(changes[0].why, "Was " + formatMoneyFull(find(before, "fuel")->cents) +
                                  " before the re-import: 1,000 lb more fuel to add (extra fuel +1,000 lb)");
    EXPECT_EQ(changes[1].id, "profit");
    EXPECT_EQ(changes[1].why, formatMoneyFull(after.totalCents - before.totalCents) + " since the last import");
}

TEST(Changes, CrewAfterALongerPlan) {
    PriceInput in = passengers();
    const Preview before = pricePreview(in, Rates{});
    in.planBlockS = 6240 + 4 * 60 * 15;  // an hour longer
    const Preview after = pricePreview(in, Rates{});
    const auto changes = changesSince(before, {6240, 5900, 0}, after, {9840, 5900, 0});
    bool sawPilots = false;
    for (const LineChange& c : changes) {
        if (c.id != "pilots") continue;
        sawPilots = true;
        EXPECT_EQ(c.why, "Was " + formatMoneyFull(c.was) + " before the re-import: planned block 2:44 (was 1:44)");
    }
    EXPECT_TRUE(sawPilots);
}
