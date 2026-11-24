// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Live's airspeed tile: knots, or Mach at altitude, with a gap so it doesn't flick at the line (D-140).

#include <gtest/gtest.h>

#include "airspeed_display.h"

using namespace blocktime;

TEST(AirspeedDisplay, SwitchesAt070) {
    EXPECT_FALSE(showMach(false, 0.699));
    EXPECT_TRUE(showMach(false, 0.70));
    EXPECT_TRUE(showMach(false, 0.78));
}

TEST(AirspeedDisplay, BackBelow066) {
    EXPECT_TRUE(showMach(true, 0.66));
    EXPECT_FALSE(showMach(true, 0.659));
}

TEST(AirspeedDisplay, HoldsBetween) {
    EXPECT_TRUE(showMach(true, 0.68));
    EXPECT_FALSE(showMach(false, 0.68));
}

TEST(AirspeedDisplay, MachText) {
    EXPECT_EQ(machText(0.784), "M.78");
    EXPECT_EQ(machText(0.706), "M.71");
    EXPECT_EQ(machText(1.0), "M1.00");
}
