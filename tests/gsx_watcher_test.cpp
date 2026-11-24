// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GsxWatcher: GSX's L:vars as gsx_service log events (milestone 8, D-138).

#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "gsx_watcher.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const system_clock::time_point kT0 = sys_days{2026y / 11 / 24} + 5h;

GsxValues all(double state) { return GsxValues{state, state, state, state, state, 0, 0}; }

}  // namespace

TEST(GsxWatcher, IgnoresZeros) {
    GsxWatcher w;
    EXPECT_TRUE(w.onValues(all(0), kT0).empty());
    EXPECT_TRUE(w.onValues(all(0), kT0 + 1s).empty());
}

TEST(GsxWatcher, FirstAvailableIsNotLogged) {
    GsxWatcher w;
    EXPECT_TRUE(w.onValues(all(1), kT0).empty());
}

TEST(GsxWatcher, LogsEachStateChange) {
    GsxWatcher w;
    w.onValues(all(1), kT0);
    std::vector<LogEvent> events;
    int t = 1;
    for (double s : {4.0, 5.0, 6.0, 1.0}) {
        GsxValues v = all(1);
        v.refuelling = s;
        for (auto& e : w.onValues(v, kT0 + seconds(t++))) events.push_back(e);
    }
    ASSERT_EQ(events.size(), 4u);
    const char* states[] = {"requested", "performing", "completed", "available"};
    for (std::size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(events[i].type, "gsx_service");
        EXPECT_EQ(events[i].data["service"], "refuelling");
        EXPECT_EQ(events[i].data["state"], states[i]);
        EXPECT_FALSE(events[i].data.contains("passengers"));
    }
    EXPECT_EQ(events[0].data["time"], "2026-11-24T05:00:01.000Z");
}

TEST(GsxWatcher, BoardingCarriesPassengers) {
    GsxWatcher w;
    w.onValues(all(1), kT0);
    GsxValues v = all(1);
    v.boarding = 5;
    v.boardedPax = 12;
    const auto events = w.onValues(v, kT0 + 1s);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data["service"], "boarding");
    EXPECT_EQ(events[0].data["state"], "performing");
    EXPECT_EQ(events[0].data["passengers"], 12);
}

TEST(GsxWatcher, PassengerChangeAloneIsNotLogged) {
    GsxWatcher w;
    GsxValues v = all(1);
    v.boarding = 5;
    v.boardedPax = 12;
    w.onValues(v, kT0);
    v.boardedPax = 13;
    EXPECT_TRUE(w.onValues(v, kT0 + 1s).empty());
}

TEST(GsxWatcher, ZeroFlickerKeepsState) {
    GsxWatcher w;
    GsxValues v = all(1);
    v.boarding = 5;
    EXPECT_EQ(w.onValues(v, kT0).size(), 1u);
    EXPECT_TRUE(w.onValues(all(0), kT0 + 1s).empty());
    EXPECT_TRUE(w.onValues(v, kT0 + 2s).empty());
}

TEST(GsxWatcher, FirstPerformingIsLogged) {
    GsxWatcher w;
    GsxValues v = all(1);
    v.boarding = 5;
    const auto events = w.onValues(v, kT0);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data["state"], "performing");
}

TEST(GsxWatcher, ResetForgetsStates) {
    GsxWatcher w;
    GsxValues v = all(1);
    v.boarding = 5;
    w.onValues(v, kT0);
    w.reset();
    EXPECT_EQ(w.onValues(v, kT0 + 1s).size(), 1u);
}

TEST(GsxWatcher, PushbackIsNamedPushback) {
    GsxWatcher w;
    w.onValues(all(1), kT0);
    GsxValues v = all(1);
    v.pushback = 4;
    const auto events = w.onValues(v, kT0 + 1s);
    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].data["service"], "pushback");
    EXPECT_FALSE(events[0].data.contains("passengers"));
}
