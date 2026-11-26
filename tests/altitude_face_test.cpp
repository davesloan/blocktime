// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The altitude gauge's two faces (D-203): the radio altitude below 2,000 ft above the ground, the altimeter above
// 3,000 ft, the 1,000 ft between them so terrain under an approach can't make it flicker. A go-around shows the
// altimeter until the next short final.

#include <gtest/gtest.h>

#include "altitude_face.h"

using namespace blocktime;

TEST(AltitudeFace, StartsOnRadio) {
    AltitudeFace f;
    f.reset();
    EXPECT_TRUE(f.radio());
    f.onSample(0);
    EXPECT_TRUE(f.radio());
}

TEST(AltitudeFace, ClimbsOutAbove3000) {
    AltitudeFace f;
    f.reset();
    for (double agl : {500.0, 1990.0, 2400.0, 3000.0}) {
        f.onSample(agl);
        EXPECT_TRUE(f.radio()) << agl;
    }
    f.onSample(3001);
    EXPECT_FALSE(f.radio());
}

TEST(AltitudeFace, TerrainBetween2000And3000KeepsTheFace) {
    AltitudeFace f;
    f.reset();
    f.onSample(4000);
    EXPECT_FALSE(f.radio());
    f.onSample(2400);  // descending over rising terrain: still the altimeter
    EXPECT_FALSE(f.radio());
    f.onSample(1990);
    EXPECT_TRUE(f.radio());
    f.onSample(2600);  // terrain falls away under the approach: still the radio altitude
    EXPECT_TRUE(f.radio());
}

TEST(AltitudeFace, GoAroundShowsAltimeterUntilFinalApproach) {
    AltitudeFace f;
    f.reset();
    f.onSample(4000);
    f.onSample(800);
    EXPECT_TRUE(f.radio());
    f.onGoAround();
    EXPECT_FALSE(f.radio());
    for (double agl : {900.0, 1500.0, 1200.0}) {
        f.onSample(agl);
        EXPECT_FALSE(f.radio()) << agl;
    }
    f.onFinalApproach();
    EXPECT_TRUE(f.radio());
}

TEST(AltitudeFace, GoAroundThenAbove3000Rearms) {
    AltitudeFace f;
    f.reset();
    f.onSample(800);
    f.onGoAround();
    f.onSample(3200);
    EXPECT_FALSE(f.radio());
    f.onSample(1990);
    EXPECT_TRUE(f.radio());
}
