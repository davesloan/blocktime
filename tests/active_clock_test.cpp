// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The clock that times phase holds (D-058): silence and pauses add (almost) nothing.

#include <chrono>

#include <gtest/gtest.h>

#include "active_clock.h"

using namespace std::chrono;
using blocktime::ActiveClock;

namespace {
const steady_clock::time_point kT0{};
}

TEST(ActiveClock, MessagesASecondApartCountInFull) {
    ActiveClock c;
    for (int i = 0; i <= 10; ++i) c.onMessage(kT0 + seconds(i));
    EXPECT_DOUBLE_EQ(c.seconds(), 10.0);
}

TEST(ActiveClock, AGapCountsAtMostTwoSeconds) {
    ActiveClock c;
    c.onMessage(kT0);
    c.onMessage(kT0 + 51s);  // a scenery-loading freeze
    EXPECT_DOUBLE_EQ(c.seconds(), 2.0);
}

TEST(ActiveClock, PausedMessagesAddNothing) {
    ActiveClock c;
    c.onMessage(kT0);
    // Paused: the sim still answers the 5 s heartbeat, but none of it counts.
    for (int i = 1; i <= 24; ++i) c.onMessage(kT0 + seconds(5 * i), false);
    c.onMessage(kT0 + 121s);  // the first message after unpausing
    EXPECT_DOUBLE_EQ(c.seconds(), 1.0);
}

TEST(ActiveClock, AnOlderTimeAddsNothing) {
    ActiveClock c;
    c.onMessage(kT0 + 10s);
    c.onMessage(kT0 + 5s);
    c.onMessage(kT0 + 11s);
    EXPECT_DOUBLE_EQ(c.seconds(), 1.0);
}
