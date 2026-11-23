// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>

#include <gtest/gtest.h>

#include "dates.h"

using namespace blocktime;
using namespace std::chrono;

namespace {
constexpr Date d(int y, unsigned m, unsigned day) { return Date{year{y}, month{m}, std::chrono::day{day}}; }
}  // namespace

TEST(Dates, ParseRejectsBadShapes) {
    EXPECT_EQ(parseDate("2026-11-22"), d(2026, 11, 22));
    EXPECT_FALSE(parseDate("2026-11-31").has_value());
    EXPECT_FALSE(parseDate("2026-1-22").has_value());
    EXPECT_FALSE(parseDate("2026-11-22T00:00:00Z").has_value());
    EXPECT_FALSE(parseDate("").has_value());
    EXPECT_FALSE(parseDate("2026/11/22").has_value());
}

TEST(Dates, FormatDate) { EXPECT_EQ(formatDate(d(2026, 1, 5)), "2026-01-05"); }

TEST(Dates, LeasePeriodsClampToMonthEnd) {
    const Date leased = d(2027, 1, 31);
    EXPECT_EQ(leasePeriodStart(leased, 0), leased);
    EXPECT_EQ(leasePeriodStart(leased, 1), d(2027, 2, 28));
    EXPECT_EQ(leasePeriodStart(leased, 2), d(2027, 3, 31));
    EXPECT_EQ(leasePeriodStart(leased, 13), d(2028, 2, 29));
    EXPECT_EQ(leasePeriodStart(d(2026, 11, 22), 2), d(2027, 1, 22));
}

TEST(Dates, LeasePeriodText) {
    EXPECT_EQ(formatLeasePeriod(d(2026, 11, 22), d(2026, 12, 22)), "22 Nov – 21 Dec");
    EXPECT_EQ(formatLeasePeriod(d(2027, 1, 31), d(2027, 2, 28)), "31 Jan – 27 Feb");
}

TEST(Dates, InstantShape) { EXPECT_EQ(formatInstant(d(2026, 11, 22)), "2026-11-22T00:00:00Z"); }

TEST(Dates, DayMonthYear) { EXPECT_EQ(formatDayMonthYear(d(2026, 11, 2)), "2 Nov 2026"); }
