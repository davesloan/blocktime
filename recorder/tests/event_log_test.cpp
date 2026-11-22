// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>

#include <gtest/gtest.h>

#include "event_log.h"

using namespace std::chrono;
using namespace blocktime;

TEST(FormatUtc, MillisecondsAndZ) {
    const auto t = sys_days{2026y / 11 / 22} + 14h + 3min + 7s + 123ms;
    EXPECT_EQ(formatUtc(t), "2026-11-22T14:03:07.123Z");
}

TEST(FormatUtc, WholeSecondHasZeroMillis) {
    const auto t = sys_days{2026y / 11 / 22} + 14h;
    EXPECT_EQ(formatUtc(t), "2026-11-22T14:00:00.000Z");
}

TEST(FormatUtc, SubMillisecondIsTruncated) {
    const auto t = sys_days{2026y / 11 / 22} + 14h + 999us;
    EXPECT_EQ(formatUtc(t), "2026-11-22T14:00:00.000Z");
}
