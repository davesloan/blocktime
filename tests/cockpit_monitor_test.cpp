// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cockpit_monitor.h"

#include <chrono>

#include <gtest/gtest.h>

using namespace blocktime;
using namespace std::chrono_literals;

namespace {

const TimePoint t0 = std::chrono::sys_days{std::chrono::year{2026} / 11 / 24} + 23h;

PlainCockpit plain(bool light = false, bool belts = false, double gear = 1.0, bool paused = false) {
    return PlainCockpit{light, belts, gear, paused};
}

}  // namespace

TEST(CockpitMonitor, PlainOnlyHasNoPmdgFields) {
    CockpitMonitor m;
    m.onPlain(plain(false, true), t0);
    const auto e = m.due(t0);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->size(), 4u);
    EXPECT_EQ((*e)["landing_lights"], false);
    EXPECT_EQ((*e)["seat_belts"], 2);
    EXPECT_EQ((*e)["gear_down"], true);
    EXPECT_EQ((*e)["pmdg"], false);
    EXPECT_FALSE(e->contains("vref_kt"));
}

TEST(CockpitMonitor, PmdgWinsForLightsAndBelts) {
    CockpitMonitor m;
    m.onPlain(plain(false, false), t0);
    PmdgCockpit p;
    p.seatBelts = 1;
    p.landingLights = true;
    p.vrefKt = 125;
    p.landingFlaps = 40;
    p.autopilot = true;
    m.onPmdg(p, t0);
    const auto e = m.due(t0);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ((*e)["landing_lights"], true);
    EXPECT_EQ((*e)["seat_belts"], 1);
    EXPECT_EQ((*e)["pmdg"], true);
    EXPECT_EQ((*e)["vref_kt"], 125);
    EXPECT_EQ((*e)["landing_flaps"], 40);
    EXPECT_EQ((*e)["autopilot"], true);
}

TEST(CockpitMonitor, PausedPlainSampleIgnored) {
    CockpitMonitor m;
    m.onPlain(plain(), t0);
    ASSERT_TRUE(m.due(t0).has_value());
    m.onPlain(plain(false, false, 0, true), t0 + 2s);  // the sim reads gear 0 while paused
    EXPECT_FALSE(m.due(t0 + 3s).has_value());
}

TEST(CockpitMonitor, AtMostOncePerSecond) {
    CockpitMonitor m;
    m.onPlain(plain(), t0);
    ASSERT_TRUE(m.due(t0).has_value());
    m.onPlain(plain(true), t0 + 200ms);
    EXPECT_FALSE(m.due(t0 + 200ms).has_value());
    m.onPlain(plain(true, true), t0 + 500ms);
    EXPECT_FALSE(m.due(t0 + 500ms).has_value());
    const auto e = m.due(t0 + 1s);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ((*e)["landing_lights"], true);
    EXPECT_EQ((*e)["seat_belts"], 2);
}

TEST(CockpitMonitor, NoEventWithoutChange) {
    CockpitMonitor m;
    m.onPlain(plain(), t0);
    ASSERT_TRUE(m.due(t0).has_value());
    m.onPlain(plain(), t0 + 5s);
    EXPECT_FALSE(m.due(t0 + 6s).has_value());
}

TEST(CockpitMonitor, NothingBeforeTheFirstSample) {
    CockpitMonitor m;
    EXPECT_FALSE(m.due(t0).has_value());
}

TEST(CockpitMonitor, SnapshotWritesTheCurrentStateEvenUnchanged) {
    // At block out: the flight's log gets the cockpit state, however long ago it last changed.
    CockpitMonitor m;
    EXPECT_FALSE(m.snapshot(t0).has_value());
    m.onPlain(plain(true, true), t0);
    ASSERT_TRUE(m.due(t0).has_value());
    const auto s = m.snapshot(t0 + 200ms);  // unchanged, and inside the once-a-second limit
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ((*s)["landing_lights"], true);
    EXPECT_EQ((*s)["seat_belts"], 2);
    EXPECT_FALSE(m.due(t0 + 2s).has_value());  // nothing new after it
}

TEST(CockpitMonitor, GearIsAFractionOfOne) {
    // GEAR TOTAL PCT EXTENDED in "percent" units reads 0 to 1 (probe flights and the milestone 10 sim test): 1.0 is
    // down and locked, values between are the gear in transit.
    CockpitMonitor m;
    m.onPlain(plain(false, false, 1.0), t0);
    EXPECT_EQ((*m.due(t0))["gear_down"], true);
    m.onPlain(plain(false, false, 0.9035), t0 + 2s);
    EXPECT_EQ((*m.due(t0 + 2s))["gear_down"], false);
    m.onPlain(plain(false, false, 0.0), t0 + 4s);
    EXPECT_FALSE(m.due(t0 + 4s).has_value());  // still not down: no change
}
