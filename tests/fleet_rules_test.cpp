// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "catalog.h"
#include "fleet_rules.h"
#include "leasing.h"
#include "pricing.h"
#include "rates.h"

using namespace blocktime;
using namespace std::chrono;

namespace {

constexpr Date d(int y, unsigned m, unsigned day) { return Date{year{y}, month{m}, std::chrono::day{day}}; }

const Catalog& shipped() {
    static const Catalog catalog = loadCatalogDir(BLOCKTIME_SOURCE_DIR "/data/aircraft", nullptr);
    return catalog;
}

const LayoutChoice* find(const std::vector<LayoutChoice>& choices, const std::string& name) {
    const auto it = std::find_if(choices.begin(), choices.end(),
                                 [&](const LayoutChoice& c) { return c.layout->name == name; });
    return it == choices.end() ? nullptr : &*it;
}

std::vector<int> maxima(const std::vector<Hold>& holds) {
    std::vector<int> out;
    for (const Hold& h : holds) out.push_back(h.maxLb);
    return out;
}

}  // namespace

TEST(Slots, TwoAtStartOnePerTenHoursCappedAtTwenty) {
    EXPECT_EQ(slotsEarned(0), 2);
    EXPECT_EQ(slotsEarned(9.9), 2);
    EXPECT_EQ(slotsEarned(10), 3);
    EXPECT_EQ(slotsEarned(23.4), 4);
    EXPECT_EQ(slotsEarned(180), 20);
    EXPECT_EQ(slotsEarned(500), 20);
    EXPECT_DOUBLE_EQ(hoursForSlot(2), 0);
    EXPECT_DOUBLE_EQ(hoursForSlot(3), 10);
    EXPECT_DOUBLE_EQ(hoursForSlot(5), 30);
    EXPECT_DOUBLE_EQ(hoursForSlot(20), 180);
    EXPECT_EQ(kMaxSlots, 20);
}

TEST(Layouts, SeatsPickTheLayout) {
    const auto choices = candidateLayouts(shipped(), "B738", 160);
    const LayoutChoice* two = find(choices, "737-800 Two-class");
    ASSERT_NE(two, nullptr);
    EXPECT_TRUE(two->seatsMatch);
    EXPECT_EQ(two->seats, 160);
    const LayoutChoice* economy = find(choices, "737-800 All-economy");
    ASSERT_NE(economy, nullptr);
    EXPECT_FALSE(economy->seatsMatch);
    EXPECT_EQ(economy->seats, 184);
    const auto pick = preselectedLayout(choices);
    ASSERT_TRUE(pick.has_value());
    EXPECT_EQ(choices[*pick].layout->name, "737-800 Two-class");
}

TEST(Layouts, BbjAndBcfBySeats) {
    const auto bbj = candidateLayouts(shipped(), "BBJ2", 48);
    ASSERT_TRUE(preselectedLayout(bbj).has_value());
    EXPECT_EQ(bbj[*preselectedLayout(bbj)].layout->name, "737-800 BBJ2 VIP");
    const auto bcf = candidateLayouts(shipped(), "B738", 0);
    ASSERT_TRUE(preselectedLayout(bcf).has_value());
    EXPECT_EQ(bcf[*preselectedLayout(bcf)].layout->name, "737-800 BCF Freighter");
}

TEST(Layouts, TieGoesToFileOrder) {
    Catalog c;
    c.types = {CatalogType{"A", "737", "airliner", 35, 450, 100'000, "PMDG A", 35.8, {"B738"}, "KBFI", 8},
               CatalogType{"B", "737", "airliner", 35, 450, 100'000, "PMDG B", 35.8, {"B738"}, "KBFI", 8}};
    c.airframes = {CatalogAirframe{"A one", "A", {{"economy", 150}}, {{"forward", 1, 1}}},
                   CatalogAirframe{"B one", "B", {{"economy", 150}}, {{"forward", 1, 1}}}};
    const auto choices = candidateLayouts(c, "B738", 150);
    ASSERT_EQ(choices.size(), 2u);
    EXPECT_TRUE(choices[0].seatsMatch);
    EXPECT_TRUE(choices[1].seatsMatch);
    EXPECT_EQ(preselectedLayout(choices), std::optional<std::size_t>(0));
    EXPECT_EQ(choices[0].type->code, "A");
}

TEST(Layouts, UnknownBaseTypeGivesNone) {
    EXPECT_TRUE(candidateLayouts(shipped(), "A20N", 150).empty());
    EXPECT_FALSE(preselectedLayout({}).has_value());
}

TEST(Holds, SplitByShareRemainderLast) {
    const std::vector<Hold> layout{{"forward", 7500, 4}, {"aft", 9500, 5}};
    const auto holds = holdsForMaxCargo(layout, 18000);
    EXPECT_EQ(maxima(holds), (std::vector<int>{7941, 10059}));
    EXPECT_EQ(holds[0].hold, "forward");
    EXPECT_EQ(holds[0].ratio, 4);
    EXPECT_EQ(holds[1].ratio, 5);
}

TEST(Holds, ThreeHolds) {
    const std::vector<Hold> layout{{"forward", 10000, 10}, {"aft", 12000, 8}, {"bulk", 3000, 2}};
    EXPECT_EQ(maxima(holdsForMaxCargo(layout, 50000)), (std::vector<int>{20000, 24000, 6000}));
    EXPECT_EQ(maxima(holdsForMaxCargo(layout, 50001)), (std::vector<int>{20000, 24000, 6001}));
}

TEST(Refund, UnusedDaysFromTomorrow) {
    // Leased 22 Nov: the month is 22 Nov - 21 Dec (30 days). Returned 27 Nov: 28 Nov - 21 Dec unused, 24 days.
    EXPECT_EQ(leaseRefundCents(15'000'000, d(2026, 11, 22), d(2026, 11, 27)), 12'000'000);
    EXPECT_EQ(unusedLeaseDays(d(2026, 11, 22), d(2026, 11, 27)), 24);
}

TEST(Refund, FirstAndLastDay) {
    EXPECT_EQ(leaseRefundCents(15'000'000, d(2026, 11, 22), d(2026, 11, 22)), 14'500'000);  // 29 of 30 days
    EXPECT_EQ(leaseRefundCents(15'000'000, d(2026, 11, 22), d(2026, 12, 21)), 0);
    // A later month: 22 Dec - 21 Jan is 31 days; returned 22 Dec refunds 30 of them.
    EXPECT_EQ(leaseRefundCents(15'500'000, d(2026, 11, 22), d(2026, 12, 22)), 15'000'000);
}

TEST(Refund, RoundsToFiveDollars) {
    // Leased 1 Dec: the month is 1-31 Dec (31 days). Returned 21 Dec: 22-31 Dec unused, 10 days.
    // $235,000 x 10 / 31 = $75,806.45, rounded to $75,805.
    const Cents refund = leaseRefundCents(23'500'000, d(2026, 12, 1), d(2026, 12, 21));
    EXPECT_EQ(refund, 7'580'500);
    EXPECT_EQ(refund % 500, 0);
}

TEST(Delivery, FeeReplacesFuel) {
    const CatalogType* b738 = shipped().type("737-800");
    ASSERT_NE(b738, nullptr);
    const Rates rates;
    const Preview p = instantDeliveryPrice(*b738, 943, 146300, rates);
    ASSERT_FALSE(p.costs.empty());
    EXPECT_EQ(p.costs[0].category, "delivery_fee");
    EXPECT_EQ(p.costs[0].label, "Delivery fee");
    EXPECT_EQ(workingText(p.costs[0]), "943 nm × $8");
    EXPECT_EQ(p.costs[0].cents, -754'500);
    Cents sum = 0;
    bool pilots = false;
    for (const MoneyLine& m : p.costs) {
        EXPECT_NE(m.category, "fuel");
        EXPECT_NE(m.category, "flight_attendants");
        pilots = pilots || m.category == "pilots";
        sum += m.cents;
    }
    EXPECT_TRUE(pilots);
    EXPECT_TRUE(p.income.empty());
    EXPECT_EQ(p.totalCents, sum);
}

TEST(Reading, MissingFieldsInFormOrder) {
    SimbriefReading r;
    r.registration = "N738SX";
    r.internalId = "1_2";
    r.baseType = "B738";
    r.maxPassengers = 160;
    r.passengerWeightLb = 190;
    r.bagWeightLb = 55;
    r.emptyWeightLb = 91300;
    r.mzfwLb = 138300;
    r.mtowLb = 174200;
    r.maxFuelLb = 46063;
    r.maxCargoLb = 18000;
    r.minRunwayFt = 7000;
    // Fin Number is optional (no tail number); MLW wasn't read.
    EXPECT_EQ(missingFields(r), std::vector<std::string>{"Max Landing Weight"});
    r.registration.clear();
    EXPECT_EQ(missingFields(r), (std::vector<std::string>{"Civil Registration", "Max Landing Weight"}));
}
