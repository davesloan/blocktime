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

// Frames 1/30 s apart, each checked for the bounce count; returns the count once it's reported.
std::optional<int> feedBounces(TouchdownCapture& c, int fromMs, int frames, bool onGround) {
    std::optional<int> got;
    for (int i = 0; i < frames; ++i) {
        const FastSample f = frame(fromMs + i * 33, onGround, onGround ? 0 : 100);
        c.onSample(f);
        if (auto b = c.bouncesDue(f); b && !got) got = b;
    }
    return got;
}

TEST(TouchdownCapture, CountsABounce) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -400);
    EXPECT_FALSE(feedBounces(c, 330, 6, true));     // contact at 330 ms
    EXPECT_FALSE(feedBounces(c, 528, 24, false));   // airborne for about 0.8 s
    EXPECT_FALSE(feedBounces(c, 1320, 400, true));  // to 14.5 s after contact
    const auto b = feedBounces(c, 15330, 30, true);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, 1);
    EXPECT_FALSE(feedBounces(c, 16400, 30, true));  // reported once
}

TEST(TouchdownCapture, IgnoresAFlicker) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -400);
    feedBounces(c, 330, 6, true);
    feedBounces(c, 528, 9, false);  // 0.3 s
    const auto b = feedBounces(c, 825, 500, true);
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, 0);
}

TEST(TouchdownCapture, FinishBouncesEarly) {
    TouchdownCapture c;
    EXPECT_FALSE(c.finishBounces());  // no contact yet
    feed(c, 0, 10, false, -400);
    feedBounces(c, 330, 60, true);
    const auto b = c.finishBounces();
    ASSERT_TRUE(b);
    EXPECT_EQ(*b, 0);
    EXPECT_FALSE(c.finishBounces());
}

TEST(TouchdownCapture, KeepsAttitudeAndWind) {
    TouchdownCapture c;
    feed(c, 0, 10, false, -200);
    FastSample f = frame(330, true, 0);
    f.pitchDeg = -4.2;
    f.bankDeg = 1.1;
    f.headingTrueDeg = 286;
    f.windDirDeg = 340;
    f.windKt = 9;
    c.onSample(f);
    const auto t = feed(c, 363, 40, true, 0);
    ASSERT_TRUE(t);
    EXPECT_DOUBLE_EQ(t->pitchDeg, -4.2);
    EXPECT_DOUBLE_EQ(t->bankDeg, 1.1);
    EXPECT_DOUBLE_EQ(t->headingTrueDeg, 286);
    EXPECT_DOUBLE_EQ(t->windDirDeg, 340);
    EXPECT_DOUBLE_EQ(t->windKt, 9);
}
