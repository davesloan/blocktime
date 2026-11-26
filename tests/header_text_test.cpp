// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>

#include <gtest/gtest.h>

#include "header_text.h"

using namespace blocktime;
using namespace std::chrono_literals;

TEST(HeaderText, FlightLevelRule) {
    EXPECT_TRUE(isFlightLevel("climb", 18000, 18000, 18000));
    EXPECT_FALSE(isFlightLevel("climb", 17900, 18000, 18000));
    EXPECT_TRUE(isFlightLevel("cruise", 33000, 18000, 18000));
    EXPECT_TRUE(isFlightLevel("descent", 18000, 18000, 18000));
    EXPECT_FALSE(isFlightLevel("approach", 17000, 18000, 18000));
    EXPECT_FALSE(isFlightLevel("climb", 30000, 0, 18000));
    EXPECT_FALSE(isFlightLevel("taxi_out", 30000, 18000, 18000));
}

TEST(HeaderText, AltitudeText) {
    EXPECT_EQ(altitudeText(33000, true), "FL330");
    EXPECT_EQ(altitudeText(8500, false), "8,500 ft");
    EXPECT_EQ(altitudeText(32960, true), "FL330");
}

TEST(HeaderText, Durations) {
    EXPECT_EQ(durationText(3900s, false), "1:05");
    EXPECT_EQ(durationText(2470s, true), "0:41:10");
}

TEST(HeaderText, BlockInEstimate) {
    const TimePoint now = std::chrono::sys_days{std::chrono::November / 25 / 2026} + 18h;
    const TimePoint out = now - 20min;
    EXPECT_EQ(blockInEstimate(std::nullopt, now, 3900), now + 3900s);
    EXPECT_EQ(blockInEstimate(out, now, 3900), out + 3900s);
    EXPECT_FALSE(blockInEstimate(out, now, 0).has_value());
}

TEST(HeaderText, NoPlanDefaults) {
    EXPECT_EQ(effectiveTransitionFt(0), 18000);
    EXPECT_EQ(effectiveTransitionFt(5000), 5000);
    EXPECT_TRUE(isFlightLevel("climb", 18000, effectiveTransitionFt(0), effectiveTransitionFt(0)));
}
