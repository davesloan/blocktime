// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ATC's words to the ATC card's slot changes (D-197). The first rows are word for word from Dave's KSAN → KSJC leg
// (tests/data/flight-ksan-ksjc); the rest are made up from docs/dashboard.md's table. A phrase BlockTime can't read
// changes nothing: a guessed clearance is an anti-goal.

#include <ostream>

#include <gtest/gtest.h>

#include "atc_instructions.h"

using namespace blocktime;

namespace blocktime {
void PrintTo(const SlotChange& c, std::ostream* os) {
    *os << "{" << static_cast<int>(c.slot) << " " << c.title << " '" << c.value << "' aside '" << c.aside << "'}";
}
}  // namespace blocktime

namespace {

const RoutePlan kPlan{"PADRZ2", "IKAYE"};

std::vector<SlotChange> read(const char* words) { return readInstructions(words, kPlan); }

SlotChange altitude(const char* v, const char* aside = "") { return {Slot::Altitude, "ALTITUDE", v, aside}; }
SlotChange route(const char* v) { return {Slot::Route, "ROUTE", v, ""}; }
SlotChange heading(const char* v, const char* aside = "") { return {Slot::Route, "HEADING", v, aside}; }
SlotChange approach(const char* v, const char* aside = "") { return {Slot::Approach, "APPROACH", v, aside}; }

}  // namespace

TEST(AtcInstructions, Clearance) {
    EXPECT_EQ(read("Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. "
                   "Expect FL400 one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27"),
              (std::vector<SlotChange>{route("PADRZ2 IKAYE"), altitude("CLIMB VIA SID", "EXP FL400 +10 MIN")}));
}

TEST(AtcInstructions, InitialAltitudeClearance) {
    EXPECT_EQ(read("Cleared to San Jose via the PADRZ2 departure. Climb and maintain 5,000. Expect FL350 five minutes "
                   "after departure. Squawk 1234"),
              (std::vector<SlotChange>{route("PADRZ2 IKAYE"), altitude("5,000", "EXP FL350 +5 MIN")}));
}

TEST(AtcInstructions, TaxiOut) {
    EXPECT_EQ(read("Runway 27 taxi via A, A7, B, B1."),
              (std::vector<SlotChange>{{Slot::Taxi, "TAXI", "A, A7, B, B1", "RWY 27"}}));
}

TEST(AtcInstructions, ClimbAndMaintain) {
    EXPECT_EQ(read("Climb and maintain FL220"), (std::vector<SlotChange>{altitude("FL220")}));
}

TEST(AtcInstructions, GreetingThenClimb) {
    EXPECT_EQ(read("Los Angeles Center, Good morning, climb and maintain FL300"),
              (std::vector<SlotChange>{altitude("FL300")}));
}

TEST(AtcInstructions, DescendViaIgnoresTheAltimeter) {
    EXPECT_EQ(read("Descend via SILCN6, TROXX transition into San Jose. San Jose is currently operating west. "
                   "Altimeter 30.19"),
              (std::vector<SlotChange>{altitude("DESCEND VIA SILCN6"), route("SILCN6 TROXX")}));
}

TEST(AtcInstructions, ExpectApproach) {
    EXPECT_EQ(read("NorCal Approach, Juliet is current, thanks for that. Good morning. Expect the ILS runway 30L "
                   "approach into San Jose via KLIDE transition"),
              (std::vector<SlotChange>{approach("EXP ILS 30L", "KLIDE")}));
}

TEST(AtcInstructions, ThreeInOneCall) {
    EXPECT_EQ(read("12 miles from HIVAK. Turn left heading 320. Descend and maintain 2,700 until established on the "
                   "localizer. Cleared for the ILS runway 30L approach."),
              (std::vector<SlotChange>{heading("320", "LEFT"), altitude("2,700", "UNTIL ESTABLISHED"),
                                       approach("CLEARED ILS 30L")}));
}

TEST(AtcInstructions, TrafficCallChangesNothing) {
    EXPECT_TRUE(read("Traffic, 9 o'clock, 5 miles, Airbus, 4,000.").empty());
}

TEST(AtcInstructions, ClearedToLand) {
    EXPECT_EQ(read("San Jose Tower, Runway 30L, Winds 360 at 4. Cleared to land."),
              (std::vector<SlotChange>{approach("CLEARED TO LAND 30L")}));
}

TEST(AtcInstructions, TaxiIn) {
    EXPECT_EQ(read("San Jose Ground, Taxi to Terminal Alpha Gate 4 via Kilo, hold short of Runway 30R"),
              (std::vector<SlotChange>{{Slot::Taxi, "TAXI", "K", "TERMINAL A GATE 4"},
                                       {Slot::HoldShort, "HOLD SHORT", "30R", ""}}));
}

TEST(AtcInstructions, TaxiInPhoneticRoute) {
    EXPECT_EQ(read("Taxi to the ramp via Alpha 7, Bravo"),
              (std::vector<SlotChange>{{Slot::Taxi, "TAXI", "A7, B", "THE RAMP"}}));
}

TEST(AtcInstructions, CrossRunway) {
    EXPECT_EQ(read("cross-runway 30R"), (std::vector<SlotChange>{{Slot::HoldShort, "HOLD SHORT", "CROSS 30R", ""}}));
}

TEST(AtcInstructions, FrequencyChangesNothing) {
    EXPECT_TRUE(read("Contact Los Angeles Center on 134.575.").empty());
    EXPECT_TRUE(read("Monitor Tower on 118.3. Good day.").empty());
    EXPECT_TRUE(read("Readback correct. ground will be on 123.9.").empty());
}

TEST(AtcInstructions, OtherCallsChangeNothing) {
    for (const char* words :
         {"Push and start approved. Tail West.", "Follow the seven-three ahead.", "Runway 27, line up and wait",
          "SoCal Departure, Radar contact.", "All aircraft, information Kilo is now current. altimeter 30.19.",
          "Welcome to San jose. Contact ground point seven. We'll see ya.", "Los Angeles Center, Good morning"}) {
        EXPECT_TRUE(read(words).empty()) << words;
    }
}

TEST(AtcInstructions, TakeoffClearanceChangesNothing) {
    EXPECT_TRUE(read("Lindbergh Tower, Runway 27, Winds calm. RNAV WNFLD, cleared for takeoff.").empty());
}

TEST(AtcInstructions, FlyHeading) { EXPECT_EQ(read("Fly heading 340"), (std::vector<SlotChange>{heading("340")})); }

TEST(AtcInstructions, Speed) {
    EXPECT_EQ(read("Maintain 210 knots"), (std::vector<SlotChange>{{Slot::Speed, "SPEED", "210 KT", ""}}));
    EXPECT_EQ(read("Resume normal speed"), (std::vector<SlotChange>{{Slot::Speed, "SPEED", "", ""}}));
}

TEST(AtcInstructions, ClearedWithTransition) {
    EXPECT_EQ(read("Cleared RNAV Y runway 27 approach, LYNDI transition"),
              (std::vector<SlotChange>{approach("CLEARED RNAV-Y 27 LYNDI")}));
}

TEST(AtcInstructions, Direct) {
    EXPECT_EQ(read("Proceed direct KLIDE"), (std::vector<SlotChange>{route("DIRECT KLIDE")}));
    EXPECT_EQ(read("Resume own navigation"), (std::vector<SlotChange>{route("OWN NAV")}));
}

TEST(AtcInstructions, FeetAndFlightLevel) {
    EXPECT_EQ(read("Descend and maintain 11,000"), (std::vector<SlotChange>{altitude("11,000")}));
    EXPECT_EQ(read("Climb and maintain flight level 240"), (std::vector<SlotChange>{altitude("FL240")}));
    EXPECT_EQ(read("Climb and maintain 5000"), (std::vector<SlotChange>{altitude("5,000")}));
}

TEST(AtcInstructions, ClimbViaSidLater) {
    EXPECT_EQ(read("Climb via the SID"), (std::vector<SlotChange>{altitude("CLIMB VIA SID")}));
}
