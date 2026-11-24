// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The touchdown from sim-frame samples (D-125).

#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#include "touchdown_capture.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const TimePoint kT0 = sys_days{2026y / 11 / 23} + 23h;

FastSample frame(int ms, bool onGround, double vsFpm, double g = 1.0) {
    FastSample s;
    s.time = kT0 + milliseconds(ms);
    s.onGround = onGround;
    s.vsFpm = vsFpm;
    s.aglFt = onGround ? 8 : 20;
    s.gForce = g;
    s.iasKt = onGround ? 128 : 131;
    s.lat = 45.59;
    s.lon = -122.60 + ms * 1e-7;
    return s;
}

// Feeds frames 1/30 s apart from `fromMs` and returns the first touchdown reported.
std::optional<Touchdown> feed(TouchdownCapture& c, int fromMs, int frames, bool onGround, double vsFpm, double g = 1.0) {
    std::optional<Touchdown> got;
    for (int i = 0; i < frames; ++i) {
        auto t = c.onSample(frame(fromMs + i * 33, onGround, vsFpm, g));
        if (t && !got) got = t;
    }
    return got;
}

}  // namespace

TEST(TouchdownCapture, RateIsTheLastAirborneSample) {
    TouchdownCapture c;
    EXPECT_FALSE(feed(c, 0, 30, false, -300));
    EXPECT_FALSE(c.onSample(frame(990, false, -180)));
    EXPECT_FALSE(c.onSample(frame(1023, true, 15, 1.3)));  // contact: reported a second later
    const auto t = feed(c, 1056, 40, true, 0);
    ASSERT_TRUE(t);
    EXPECT_DOUBLE_EQ(t->vsFpm, -180);
    EXPECT_DOUBLE_EQ(t->iasKt, 131);
    EXPECT_EQ(t->time, kT0 + 1023ms);
    EXPECT_DOUBLE_EQ(t->lon, -122.60 + 1023 * 1e-7);
    EXPECT_TRUE(c.captured());
}

TEST(TouchdownCapture, GIsThePeakOverOneSecond) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -200);
    c.onSample(frame(400, true, 0, 1.2));
    c.onSample(frame(600, true, 0, 1.45));
    c.onSample(frame(900, true, 0, 1.1));
    const auto t = c.onSample(frame(1400, true, 0, 1.9));  // after the second: not counted
    ASSERT_TRUE(t);
    EXPECT_DOUBLE_EQ(t->gForce, 1.45);
}

TEST(TouchdownCapture, ReturnedOneSecondAfterContact) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -200);
    EXPECT_FALSE(c.onSample(frame(400, true, 0)));
    EXPECT_FALSE(c.onSample(frame(1399, true, 0)));
    EXPECT_TRUE(c.onSample(frame(1400, true, 0)));
}

TEST(TouchdownCapture, ABounceIsNotASecondTouchdown) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -400);
    feed(c, 330, 10, true, 0);        // contact at 330 ms
    feed(c, 660, 10, false, 200);     // bounce
    feed(c, 990, 10, true, 0);
    const auto t = feed(c, 1320, 40, true, 0);
    ASSERT_TRUE(t);
    EXPECT_EQ(t->time, kT0 + 330ms);
    EXPECT_DOUBLE_EQ(t->vsFpm, -400);
    EXPECT_FALSE(feed(c, 3000, 60, true, 0));  // never a second one
}

TEST(TouchdownCapture, NothingWithoutAnAirborneSample) {
    TouchdownCapture c;
    EXPECT_FALSE(feed(c, 0, 90, true, 0));  // armed while already on the ground
    EXPECT_FALSE(c.captured());
}

TEST(TouchdownCapture, ResetAllowsAnother) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -200);
    ASSERT_TRUE(feed(c, 330, 60, true, 0));
    c.reset();
    EXPECT_FALSE(c.captured());
    feed(c, 5000, 10, false, -250);
    const auto t = feed(c, 5330, 60, true, 0);
    ASSERT_TRUE(t);
    EXPECT_DOUBLE_EQ(t->vsFpm, -250);
}
