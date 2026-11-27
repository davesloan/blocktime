// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The predictions (D-192, D-212) on Dave's KSAN → KSJC plan: enroute 1:04:36, TOC at 0:19:12 with 9,401 lb, TOD at
// 0:39:38 with 7,827 lb, 7,040 lb at KSJC.

#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "predictions.h"
#include "route_progress.h"
#include "simbrief.h"

using namespace blocktime;
using namespace std::chrono_literals;

namespace {

constexpr int kEnrouteS = 3600 + 4 * 60 + 36;
constexpr int kTocS = 19 * 60 + 12, kTodS = 39 * 60 + 38, kTesea = 31 * 60 + 42;
constexpr TaxiTimes kTaxi{540, 120};

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

struct Ksjc {
    std::vector<PlanFix> fixes = parsePlanFixes(fixture("ofp-ksan-ksjc-navlog.json"));
    RouteProgress route{routeOf(fixes)};
    PlanTimeline plan{fixes, route, 13439 - 500};

    static std::vector<RoutePoint> routeOf(const std::vector<PlanFix>& fixes) {
        std::vector<RoutePoint> r{{32.7336, -117.19}};
        for (const PlanFix& f : fixes) r.push_back(f.at);
        r.push_back({37.3626, -121.929});
        return r;
    }
    double tocNm() const { return *plan.fixNm("TOC"); }
    double todNm() const { return *plan.fixNm("TOD"); }
};

const TimePoint kNow = TimePoint(std::chrono::sys_days(std::chrono::year(2026) / 11 / 25)) + 19h;

// Seconds from b to a, for comparing within a second.
double after(std::optional<TimePoint> a, TimePoint b) {
    EXPECT_TRUE(a.has_value());
    return a ? std::chrono::duration<double>(*a - b).count() : -1e9;
}

}  // namespace

TEST(RouteProgress, PointNm) {
    const Ksjc k;
    EXPECT_DOUBLE_EQ(k.route.pointNm(0), 0);
    EXPECT_DOUBLE_EQ(k.route.pointNm(18), k.route.totalNm());
    for (std::size_t i = 1; i < 19; ++i) EXPECT_GE(k.route.pointNm(i), k.route.pointNm(i - 1));
}

TEST(Predictions, TimelineInterpolates) {
    const Ksjc k;
    ASSERT_FALSE(k.plan.empty());
    const double mid = (k.route.pointNm(6) + k.route.pointNm(7)) / 2;  // halfway from TOC to TESEA
    EXPECT_NEAR(k.plan.elapsedS(mid), (kTocS + kTesea) / 2.0, 0.5);
    EXPECT_NEAR(k.plan.elapsedS(-5), 0, 1e-9);
    EXPECT_NEAR(k.plan.elapsedS(k.route.totalNm() + 10), kEnrouteS, 1e-9);
    EXPECT_NEAR(k.plan.fuelLb(0), 12939, 1e-9);
    EXPECT_EQ(k.plan.enrouteS(), kEnrouteS);
    EXPECT_EQ(k.plan.landingFuelLb(), 7040);
    EXPECT_FALSE(k.plan.fixNm("NOPE").has_value());
}

TEST(Predictions, TimelineEmptyWithoutTimes) {
    const auto fixes = parsePlanFixes(R"({"navlog":[{"pos_lat":"33","pos_long":"-118"},{"pos_lat":"34","pos_long":"-119"}]})");
    const RouteProgress route({{32, -117}, fixes[0].at, fixes[1].at, {35, -120}});
    const PlanTimeline plan(fixes, route, 10000);
    EXPECT_TRUE(plan.empty());
    const Predictions p = predict(plan, kTaxi, FlightNow{kNow});
    EXPECT_FALSE(p.arrival.has_value());  // Review Focus 1: the header falls back
    EXPECT_FALSE(p.todNm.has_value());
    EXPECT_FALSE(p.planFuelLb.has_value());
}

TEST(Predictions, BeforeBlockOut) {
    const Ksjc k;
    const Predictions p = predict(k.plan, kTaxi, FlightNow{kNow});
    EXPECT_NEAR(after(p.arrival, kNow), 540 + kEnrouteS + 120, 1);
    EXPECT_FALSE(p.reference.has_value());
    EXPECT_NEAR(after(p.tocAt, kNow), 540 + kTocS, 1);
    EXPECT_NEAR(*p.tocNm, k.tocNm(), 1e-9);
}

TEST(Predictions, TaxiingOut) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 300s;
    f.phase = "taxi_out";
    const Predictions p = predict(k.plan, kTaxi, f);
    EXPECT_NEAR(after(p.arrival, kNow), 240 + kEnrouteS + 120, 1);
    EXPECT_NEAR(after(p.reference, *f.blockOut), 540 + kEnrouteS + 120, 1);
    EXPECT_NEAR(after(p.planLanding, *f.blockOut), 540 + kEnrouteS, 1);
}

// Review Focus 5: a taxi longer than expected never puts the arrival before the rest of the flight.
TEST(Predictions, TaxiOutLongerThanExpected) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 900s;
    f.phase = "taxi_out";
    EXPECT_NEAR(after(predict(k.plan, kTaxi, f).arrival, kNow), kEnrouteS + 120, 1);
}

TEST(Predictions, Airborne) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 1800s;
    f.liftoff = kNow - 1200s;
    f.flownNm = k.tocNm();
    f.phase = "climb";
    const Predictions p = predict(k.plan, kTaxi, f);
    EXPECT_NEAR(after(p.arrival, kNow), kEnrouteS - kTocS + 120, 1);
    EXPECT_NEAR(after(p.landing, kNow), kEnrouteS - kTocS, 1);
    EXPECT_FALSE(p.tocNm.has_value());  // passed
    EXPECT_FALSE(p.tocAt.has_value());
    EXPECT_NEAR(*p.todNm, k.todNm() - k.tocNm(), 1e-9);
    EXPECT_NEAR(after(p.todAt, kNow), kTodS - kTocS, 1);
}

// Review Focus 2: in a hold the distance flown stands still, and the arrival slips with the clock.
TEST(Predictions, HoldSlips) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 3000s;
    f.liftoff = kNow - 2400s;
    f.flownNm = k.todNm() + 20;
    f.phase = "descent";
    const auto first = predict(k.plan, kTaxi, f).arrival;
    f.now = kNow + 600s;
    EXPECT_NEAR(after(predict(k.plan, kTaxi, f).arrival, *first), 600, 1e-6);
}

TEST(Predictions, TaxiInLongerThanExpected) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 5000s;
    f.liftoff = kNow - 4400s;
    f.touchdown = kNow - 300s;
    f.flownNm = k.route.totalNm();
    f.phase = "taxi_in";
    const Predictions p = predict(k.plan, kTaxi, f);
    EXPECT_NEAR(after(p.arrival, kNow), 0, 1e-6);
    EXPECT_FALSE(p.landing.has_value());
}

TEST(Predictions, Fuel) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 3000s;
    f.liftoff = kNow - 2400s;
    f.flownNm = k.todNm();
    f.fuelLb = 7500;
    f.phase = "cruise";
    Predictions p = predict(k.plan, kTaxi, f);
    EXPECT_EQ(p.planFuelLb, std::optional<int>(7827));
    EXPECT_EQ(p.planLandingFuelLb, std::optional<int>(7040));
    EXPECT_EQ(p.fuelAtLandingLb, std::optional<int>(7500 - (7827 - 7040)));
    f.touchdown = kNow;
    f.phase = "landing";
    p = predict(k.plan, kTaxi, f);
    EXPECT_FALSE(p.planFuelLb.has_value());
    EXPECT_FALSE(p.fuelAtLandingLb.has_value());
}

TEST(Predictions, PlanFuelBeforeLiftoffIsTheTakeoffFuel) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 60s;
    f.fuelLb = 13400;
    f.phase = "pushback";
    EXPECT_EQ(predict(k.plan, kTaxi, f).planFuelLb, std::optional<int>(12939));
}

TEST(Predictions, CruiseSpeedOnlyInCruise) {
    const Ksjc k;
    FlightNow f{kNow};
    f.blockOut = kNow - 1800s;
    f.liftoff = kNow - 1200s;
    f.flownNm = (k.route.pointNm(6) + k.route.pointNm(7)) / 2;  // between TOC and TESEA
    f.phase = "cruise";
    EXPECT_EQ(predict(k.plan, kTaxi, f).cruiseSpeedKt, std::optional<int>(240));
    f.phase = "climb";
    EXPECT_FALSE(predict(k.plan, kTaxi, f).cruiseSpeedKt.has_value());
}

TEST(Predictions, PunctualityBands) {
    const TimePoint ref = kNow;
    EXPECT_EQ(punctuality(ref + 120s, ref).kind, Punctuality::OnTime);
    EXPECT_EQ(punctuality(ref - 120s, ref).kind, Punctuality::OnTime);
    const PunctualityView late = punctuality(ref + 180s, ref);
    EXPECT_EQ(late.kind, Punctuality::Delayed);
    EXPECT_EQ(late.minutes, 3);
    const PunctualityView early = punctuality(ref - 240s, ref);
    EXPECT_EQ(early.kind, Punctuality::Early);
    EXPECT_EQ(early.minutes, 4);
    EXPECT_EQ(punctualityText(punctuality(ref + 60s, ref), false), "On time");
    EXPECT_EQ(punctualityText(late, false), "Delayed ~3m");
    EXPECT_EQ(punctualityText(early, true), "Early 4m");
}
