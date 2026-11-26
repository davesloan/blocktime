// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The plan's extras read from the stored OFP, and the Pre-Flight panel's load sheet (D-153, D-187, D-188).

#include <fstream>
#include <numeric>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "load_sheet.h"
#include "plan_brief.h"

using namespace blocktime;

namespace {

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// N814SX's KSAN → KSJC leg as planned on 2026-11-25.
PlannedFlightView bbj2() {
    PlannedFlightView v;
    v.kind = FlightKind::Vip;
    v.cabins = {{"economy", 44}};
    v.holds = {{"forward", 7842, 4}, {"aft", 9803, 5}};
    SimbriefPlan p;
    p.rampFuelLb = 13439;
    p.cruiseAltitudeFt = 40000;
    p.originTransitionAltitudeFt = 18000;
    p.originRunway = "27";
    v.plan = p;
    v.brief = parsePlanBrief(fixture("ofp-ksan-ksjc-brief.json"));
    return v;
}

}  // namespace

TEST(PlanBrief, KsanKsjc) {
    const PlanBrief b = parsePlanBrief(fixture("ofp-ksan-ksjc-brief.json"));
    EXPECT_EQ(b.sid, "PADRZ2");
    EXPECT_EQ(b.sidTransition, "IKAYE");
    EXPECT_EQ(b.zfwLb, 112967);
    EXPECT_EQ(b.cargoLb, 2420);
    ASSERT_EQ(b.runways.size(), 2u);
    EXPECT_EQ(b.runways[0].ident, "09");
    EXPECT_EQ(b.runways[0].magneticCourseDeg, 95);
    EXPECT_EQ(b.runways[1].ident, "27");
    EXPECT_EQ(b.runways[1].magneticCourseDeg, 275);
    EXPECT_EQ(b.surface, "dry");
}

TEST(PlanBrief, OldFixtureHasNone) {
    const PlanBrief b = parsePlanBrief(fixture("ofp_v2_min.json"));
    EXPECT_EQ(b.sid, "");
    EXPECT_EQ(b.sidTransition, "");
    EXPECT_TRUE(b.runways.empty());
    EXPECT_EQ(b.surface, "");
    EXPECT_EQ(b.zfwLb, 102187);
    EXPECT_EQ(parsePlanBrief("not json").zfwLb, 0);
}

TEST(LoadSheet, Bbj2) {
    const LoadSheet s = loadSheet(bbj2());
    EXPECT_TRUE(s.hasPlan);
    EXPECT_EQ(s.blockFuelLb, 13439);
    EXPECT_EQ(s.zfwLb, 112967);
    ASSERT_EQ(s.passengers.size(), 1u);
    EXPECT_EQ(s.passengers[0].cabinClass, "economy");
    EXPECT_EQ(s.passengers[0].passengers, 44);
    EXPECT_EQ(s.cargoLb, 2420);
    ASSERT_EQ(s.holds.size(), 2u);
    EXPECT_EQ(s.holds[0].hold, "forward");
    EXPECT_EQ(s.holds[0].weightLb, 1075);
    EXPECT_EQ(s.holds[1].hold, "aft");
    EXPECT_EQ(s.holds[1].weightLb, 1345);
    EXPECT_EQ(s.cruiseAltitudeFt, 40000);
    EXPECT_EQ(s.transitionAltitudeFt, 18000);
}

TEST(LoadSheet, Freighter) {
    PlannedFlightView v = bbj2();
    v.kind = FlightKind::Cargo;
    v.cabins = {};
    v.holds = {{"main_deck", 34960, 34960}, {"forward", 4892, 4892}, {"aft", 6108, 6108}};
    v.brief->cargoLb = 30000;
    const LoadSheet s = loadSheet(v);
    EXPECT_TRUE(s.passengers.empty());
    ASSERT_EQ(s.holds.size(), 3u);
    const int sum = std::accumulate(s.holds.begin(), s.holds.end(), 0,
                                    [](int total, const HoldLoad& h) { return total + h.weightLb; });
    EXPECT_EQ(sum, 30000);
}

TEST(LoadSheet, NoPlan) {
    PlannedFlightView v = bbj2();
    v.plan.reset();
    v.brief.reset();
    const LoadSheet s = loadSheet(v);
    EXPECT_FALSE(s.hasPlan);
    EXPECT_EQ(s.blockFuelLb, 0);
    ASSERT_EQ(s.passengers.size(), 1u);
    EXPECT_EQ(s.passengers[0].passengers, 44);
    EXPECT_TRUE(s.holds.empty());
}

TEST(LoadSheet, CargoOverCapacity) {
    PlannedFlightView v = bbj2();
    v.brief->cargoLb = 20000;  // the holds take 17,645
    const LoadSheet s = loadSheet(v);
    EXPECT_EQ(s.cargoLb, 20000);
    EXPECT_TRUE(s.holds.empty());
}

// The arrival readouts' plan values (D-194) and FUEL REMAINING's endurance (D-203), from Dave's stored OFP.
TEST(PlanBrief, StarAndLandingRunway) {
    const PlanBrief b = parsePlanBrief(fixture("ofp-ksan-ksjc-brief.json"));
    EXPECT_EQ(b.star, "SILCN6");
    EXPECT_EQ(b.starTransition, "TROXX");
    EXPECT_EQ(b.landingRunway, "30L");
    EXPECT_EQ(b.reserveLb, 3139);
    EXPECT_EQ(b.rampLb, 13439);
    EXPECT_EQ(b.taxiLb, 500);
    EXPECT_EQ(b.enduranceS, 2 * 3600 + 41 * 60);
}

TEST(PlanBrief, EnduranceInSeconds) {
    EXPECT_EQ(parsePlanBrief(R"({"times": {"endurance": "9660"}})").enduranceS, 9660);
}

// Fuel on board at the plan's own average burn: 13,420 lb × 2:41 / 12,939 lb ≈ 2:47.
TEST(PlanBrief, FuelRemaining) {
    const PlanBrief b = parsePlanBrief(fixture("ofp-ksan-ksjc-brief.json"));
    EXPECT_EQ(fuelRemainingMinutes(13420, b), std::optional<int>(167));
    EXPECT_EQ(fuelRemainingMinutes(7190, b), std::optional<int>(89));
    PlanBrief noEndurance = b;
    noEndurance.enduranceS = 0;
    EXPECT_FALSE(fuelRemainingMinutes(13420, noEndurance).has_value());
    EXPECT_FALSE(fuelRemainingMinutes(13420, PlanBrief{}).has_value());
}
