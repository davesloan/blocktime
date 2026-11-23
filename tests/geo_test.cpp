// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "geo.h"

using namespace blocktime;

TEST(Geo, ZeroForSamePoint) {
    EXPECT_EQ(distanceMeters(32.7336, -117.1897, 32.7336, -117.1897), 0.0);
}

TEST(Geo, ThousandthDegreeLatitudeIsAbout111m) {
    EXPECT_NEAR(distanceMeters(32.733, -117.19, 32.734, -117.19), 111.2, 0.5);
}

TEST(Geo, LongitudeShrinksWithLatitude) {
    EXPECT_NEAR(distanceMeters(60.0, 10.0, 60.0, 10.001), 55.6, 0.5);
}
