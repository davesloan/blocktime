// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The clearance and the taxi route read from ATC's words (D-153, D-163), and the clearance's countdown.

#include <chrono>

#include <gtest/gtest.h>

#include "clearance.h"

using namespace blocktime;
using namespace std::chrono_literals;

TEST(Clearance, KsanPadrz2) {
    const auto c = parseClearance(
        "Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. Expect FL400 "
        "one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->sid, "PADRZ2");
    EXPECT_EQ(c->initialAltitude, "Climb via SID");
    EXPECT_EQ(c->departureFrequency, "119.6");
    EXPECT_EQ(c->squawk, "3166");
    EXPECT_EQ(c->runway, "27");
}

// The ATC card's ALTITUDE aside before the first altitude clearance (D-197): "EXP FL400 +10 MIN".
TEST(Clearance, ExpectLevel) {
    const auto c = parseClearance(
        "Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. Expect FL400 "
        "one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->expectLevel, "FL400");
    EXPECT_EQ(c->expectMinutes, 10);
    const auto five = parseClearance("Cleared to San Jose. Climb and maintain 5,000. Expect flight level 350 five "
                                     "minutes after departure. Squawk 1234");
    ASSERT_TRUE(five.has_value());
    EXPECT_EQ(five->expectLevel, "FL350");
    EXPECT_EQ(five->expectMinutes, 5);
    const auto none = parseClearance("Cleared to San Jose as filed. Squawk 1234");
    ASSERT_TRUE(none.has_value());
    EXPECT_EQ(none->expectLevel, "");
    EXPECT_EQ(none->expectMinutes, 0);
}

// SimBrief's full SID while ATC's matches it (D-189); ATC's own, marked changed, when it doesn't.
TEST(Clearance, PlanSidForm) {
    bool changed = true;
    EXPECT_EQ(planSidForm("PADRZ2", "PADRZ2", "IKAYE", &changed), "PADRZ2 IKAYE");
    EXPECT_FALSE(changed);
    EXPECT_EQ(planSidForm("", "PADRZ2", "IKAYE", &changed), "PADRZ2 IKAYE");
    EXPECT_FALSE(changed);
    EXPECT_EQ(planSidForm("PADRZ2", "PADRZ2", "", &changed), "PADRZ2");
    EXPECT_EQ(planSidForm("BORDR2", "PADRZ2", "IKAYE", &changed), "BORDR2");
    EXPECT_TRUE(changed);
    EXPECT_EQ(planSidForm("", "", "", &changed), "");
    EXPECT_FALSE(changed);
}

TEST(Clearance, KlasRadyr2) {
    const auto c = parseClearance(
        "Cleared to San Diego via the RADYR2 departure. Then as filed. Climb via the SID. Expect FL360 one-zero minutes "
        "after departure. Departure on 125.9. Squawk 2546. Expect runway 8L");
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->sid, "RADYR2");
    EXPECT_EQ(c->initialAltitude, "Climb via SID");
    EXPECT_EQ(c->departureFrequency, "125.9");
    EXPECT_EQ(c->squawk, "2546");
    EXPECT_EQ(c->runway, "08L");
}

TEST(Clearance, ClimbAndMaintain) {
    const auto feet = parseClearance(
        "Cleared to Seattle via the BANGR9 departure. Then as filed. Climb and maintain 5000. Expect FL350 one-zero "
        "minutes after departure. Departure on 120.4. Squawk 4521.");
    ASSERT_TRUE(feet.has_value());
    EXPECT_EQ(feet->initialAltitude, "5,000 ft");
    const auto level = parseClearance(
        "Cleared to Seattle via radar vectors. Then as filed. Climb and maintain flight level 190. Expect FL350. "
        "Squawk 4521.");
    ASSERT_TRUE(level.has_value());
    EXPECT_EQ(level->initialAltitude, "FL190");
    EXPECT_EQ(level->sid, "");
    const auto onlyExpect = parseClearance("Cleared to Seattle. Then as filed. Expect FL350. Squawk 4521.");
    ASSERT_TRUE(onlyExpect.has_value());
    EXPECT_EQ(onlyExpect->initialAltitude, "");
}

TEST(Clearance, NotAClearance) {
    EXPECT_FALSE(parseClearance("Readback correct. ground will be on 123.9.").has_value());
    EXPECT_FALSE(parseClearance("Lindbergh Tower, Runway 27, Winds calm. RNAV WNFLD, cleared for takeoff.").has_value());
    EXPECT_FALSE(parseClearance("").has_value());
}

TEST(Taxi, KsanRoute) {
    const auto t = parseTaxi("Runway 27 taxi via A, A7, B, B1.");
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->runway, "27");
    EXPECT_EQ(t->route, "A, A7, B, B1");
}

TEST(Taxi, NotTaxi) {
    EXPECT_FALSE(parseTaxi("Push and start approved. Tail West.").has_value());
    EXPECT_FALSE(parseTaxi("Monitor Tower on 118.3. Good day.").has_value());
}

TEST(Countdown, Runs) {
    const TimePoint t = std::chrono::sys_days{std::chrono::year{2026} / 11 / 25} + 18h + 39min + 40s;
    auto c = clearanceCountdown(t, t);
    EXPECT_EQ(c.text, "30:00");
    EXPECT_FALSE(c.urgent);
    EXPECT_FALSE(c.expired);
    EXPECT_EQ(clearanceCountdown(t, t + 3min + 20s).text, "26:40");
    c = clearanceCountdown(t, t + 25min);
    EXPECT_EQ(c.text, "5:00");
    EXPECT_TRUE(c.urgent);
    EXPECT_FALSE(c.expired);
    c = clearanceCountdown(t, t + 31min);
    EXPECT_EQ(c.text, "0:00");
    EXPECT_TRUE(c.urgent);
    EXPECT_TRUE(c.expired);
}
