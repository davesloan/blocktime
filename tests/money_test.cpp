// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "money.h"

using namespace blocktime;

TEST(Money, RoundsToNearestFive) {
    EXPECT_EQ(roundToFiveDollars(250), 500);
    EXPECT_EQ(roundToFiveDollars(249), 0);
    EXPECT_EQ(roundToFiveDollars(749), 500);
    EXPECT_EQ(roundToFiveDollars(750), 1000);
    EXPECT_EQ(roundToFiveDollars(-250), -500);
    EXPECT_EQ(roundToFiveDollars(-249), 0);
    EXPECT_EQ(roundToFiveDollars(12'750'000), 12'750'000);
}

TEST(Money, FullForm) {
    EXPECT_EQ(formatMoneyFull(140'000'000), "$1,400,000");
    EXPECT_EQ(formatMoneyFull(-90'000'000), "-$900,000");
    EXPECT_EQ(formatMoneyFull(0), "$0");
    EXPECT_EQ(formatMoneyFull(50'000), "$500");
    EXPECT_EQ(formatMoneyFull(100'000), "$1,000");
}

TEST(Money, ShortForm) {
    EXPECT_EQ(formatMoneyShort(140'000'000), "$1.4M");
    EXPECT_EQ(formatMoneyShort(1'248'000'000), "$12.48M");
    EXPECT_EQ(formatMoneyShort(38'250'000), "$382.5k");
    EXPECT_EQ(formatMoneyShort(95'000), "$950");
    EXPECT_EQ(formatMoneyShort(500'000'000), "$5M");
    EXPECT_EQ(formatMoneyShort(-140'000'000), "-$1.4M");
    EXPECT_EQ(formatMoneyShort(99'995'000), "$1M");
    EXPECT_EQ(formatMoneyShort(0), "$0");
}
