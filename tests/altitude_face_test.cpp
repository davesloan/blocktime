// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The altitude gauge's two faces (D-203, D-209): the radio altitude below 300 ft above the ground, the altimeter above
// 500 ft (the radio dial's top), the 200 ft between them so terrain under a short final can't make it flicker. A
// go-around shows the altimeter until the airplane has climbed above 500 ft or reached the next short final, then
// the 300 ft rule brings the radio face back.

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

TEST(AltitudeFace, ClimbsOutAbove500) {
    AltitudeFace f;
    f.reset();
    for (double agl : {100.0, 290.0, 400.0, 500.0}) {
        f.onSample(agl);
        EXPECT_TRUE(f.radio()) << agl;
    }
    f.onSample(501);
    EXPECT_FALSE(f.radio());
}

TEST(AltitudeFace, TerrainBetween300And500KeepsTheFace) {
    AltitudeFace f;
    f.reset();
    f.onSample(4000);
    EXPECT_FALSE(f.radio());
    f.onSample(1000);  // the final_approach marker's height: still the altimeter
    EXPECT_FALSE(f.radio());
    f.onSample(400);   // descending over rising terrain: still the altimeter
    EXPECT_FALSE(f.radio());
    f.onSample(290);
    EXPECT_TRUE(f.radio());
    f.onSample(450);   // terrain falls away under the final: still the radio altitude
    EXPECT_TRUE(f.radio());
}

TEST(AltitudeFace, FinalApproachMarkerDoesNotShowTheRadioFace) {
    AltitudeFace f;
    f.reset();
    f.onSample(4000);
    f.onFinalApproach();  // at 1,000 ft the 0-500 ft dial would sit pegged
    EXPECT_FALSE(f.radio());
    f.onSample(290);
    EXPECT_TRUE(f.radio());
}

TEST(AltitudeFace, GoAroundShowsAltimeterUntilRearmed) {
    AltitudeFace f;
    f.reset();
    f.onSample(4000);
    f.onSample(200);
    EXPECT_TRUE(f.radio());
    f.onGoAround();
    EXPECT_FALSE(f.radio());
    f.onSample(250);  // still low in the go-around: the altimeter
    EXPECT_FALSE(f.radio());
    f.onSample(1500);
    EXPECT_FALSE(f.radio());
    f.onSample(290);  // the next short final
    EXPECT_TRUE(f.radio());
}

TEST(AltitudeFace, GoAroundThenFinalApproachRearms) {
    AltitudeFace f;
    f.reset();
    f.onSample(200);
    f.onGoAround();
    f.onSample(450);  // a low go-around that never passed 500 ft
    f.onFinalApproach();
    EXPECT_FALSE(f.radio());
    f.onSample(290);
    EXPECT_TRUE(f.radio());
}
