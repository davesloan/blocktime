// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The in-flight panel's readouts by phase (D-194, D-203), with the values of Dave's KSAN → KSJC leg: SOULS and FUEL
// REMAINING first in every phase, then the phase's own.

#include <ostream>

#include <gtest/gtest.h>

#include "inflight_readouts.h"

using namespace blocktime;

namespace blocktime {
void PrintTo(const ReadoutView& r, std::ostream* os) {
    *os << "{" << r.title << " '" << r.value << "' " << r.unit << " tag '" << r.tag << "'" << (r.flagged ? " FLAG" : "")
        << (r.words ? " words" : "") << (r.wide ? " wide" : "") << (r.ticks ? " ticks" : "") << " stars " << r.stars
        << "}";
}
}  // namespace blocktime

namespace {

const nlohmann::json kKsjcAtis = {
    {"role", "destination"},
    {"icao", "KSJC"},
    {"letter", "J"},
    {"arriving", "30L,30R"},
    {"altimeter", "30.19"},
    {"metar", "KSJC 251853Z VRB04KT 10SM FEW014 FEW030 FEW044 17/12 A3019 RMK AO2 SLP224 T01670117"},
    {"text", "NORMAN Y. MINETA SAN JOSE INTL ARPT, INFO JULIET. 1853Z. ARVG AND DPTG RWYS 30L, 30R. WIND VRB AT 4. "
             "VIS 10. TEMP 17, DP 12. ALTIM 30.19. ILS RWY 30L AND RNAV-Z RWY 30R APCHS IN USE."}};

InFlightFacts ksanKsjc() {
    InFlightFacts f;
    f.departure.runway = "27";
    f.departure.planRunway = "27";
    f.departure.runwayTag = "ATC";
    f.departure.headingDeg = 275;
    f.departure.wind = "Calm";
    f.departure.altimeter = "30.09 inHg";
    f.clearance.squawk = "3166";
    f.cruiseLevelFt = 40000;
    f.plan.star = "SILCN6";
    f.plan.starTransition = "TROXX";
    f.plan.landingRunway = "30L";
    f.plan.rampLb = 13439;
    f.plan.taxiLb = 500;
    f.plan.enduranceS = 9660;
    f.passengers = 44;
    f.crew = 5;
    f.fuelLb = 7590;
    return f;
}

ReadoutView r(std::string title, std::string value, std::string unit = "", std::string tag = "") {
    ReadoutView v;
    v.title = std::move(title);
    v.value = std::move(value);
    v.unit = std::move(unit);
    v.tag = std::move(tag);
    return v;
}
ReadoutView words(ReadoutView v) {
    v.words = true;
    return v;
}
ReadoutView flagged(ReadoutView v) {
    v.flagged = true;
    return v;
}

ReadoutView souls() { return r("SOULS", "49", "", "44 PAX + 5 CREW"); }
ReadoutView fuel(const char* value) {
    ReadoutView v = r("FUEL REMAINING", value, "H:MM");
    v.ticks = true;
    return v;
}

// The predictions' readouts tick in place, never flip (D-213).
ReadoutView ticking(ReadoutView v) {
    v.ticks = true;
    return v;
}
ReadoutView toc(const char* nm = "", const char* at = "") { return ticking(r("TOP OF CLIMB", nm, "NM", at)); }
ReadoutView tod(const char* nm = "", const char* at = "") { return ticking(r("TOP OF DESCENT", nm, "NM", at)); }
ReadoutView landingAt(const char* at = "", const char* plan = "") { return ticking(r("LANDING", at, "", plan)); }
ReadoutView fuelAtLanding(const char* lb = "", const char* plan = "") {
    return ticking(r("FUEL AT LANDING", lb, "LB", plan));
}

std::vector<ReadoutView> withEmergency(const char* fuelValue, std::vector<ReadoutView> rest) {
    std::vector<ReadoutView> out{souls(), fuel(fuelValue)};
    out.insert(out.end(), rest.begin(), rest.end());
    return out;
}

}  // namespace

TEST(InFlightReadouts, PanelPhases) {
    EXPECT_EQ(panelPhase("pushback", false, false), PanelPhase::Departure);
    EXPECT_EQ(panelPhase("taxi_out", false, false), PanelPhase::Departure);
    EXPECT_EQ(panelPhase("takeoff", false, false), PanelPhase::Departure);
    EXPECT_EQ(panelPhase("climb", false, false), PanelPhase::Climb);
    EXPECT_EQ(panelPhase("cruise", false, false), PanelPhase::Cruise);
    EXPECT_EQ(panelPhase("descent", false, false), PanelPhase::Arrival);
    EXPECT_EQ(panelPhase("approach", false, false), PanelPhase::Arrival);
    EXPECT_EQ(panelPhase("approach", true, false), PanelPhase::ShortFinal);
    EXPECT_EQ(panelPhase("landing", true, true), PanelPhase::Landed);
    EXPECT_EQ(panelPhase("taxi_in", false, true), PanelPhase::Landed);
}

TEST(InFlightReadouts, GoAroundLeavesShortFinal) { EXPECT_EQ(panelPhase("go_around", false, false), PanelPhase::Arrival); }

TEST(InFlightReadouts, Departure) {
    InFlightFacts f = ksanKsjc();
    f.fuelLb = 13420;
    EXPECT_EQ(inFlightReadouts(PanelPhase::Departure, f),
              withEmergency("2:47", {r("RUNWAY", "27", "", "275° · ATC"), r("SQUAWK", "3166"),
                                     words(r("WIND", "Calm", "", "ATIS")), r("ALTIMETER", "30.09", "inHg")}));
}

TEST(InFlightReadouts, DepartureRunwayChangedIsFlagged) {
    InFlightFacts f = ksanKsjc();
    f.departure.runway = "09";
    f.departure.runwayChanged = true;
    f.departure.runwayTag = "PLAN 27";
    f.departure.headingDeg = 95;
    EXPECT_EQ(inFlightReadouts(PanelPhase::Departure, f)[2], flagged(r("RUNWAY", "09", "", "PLAN 27")));
}

TEST(InFlightReadouts, Climb) {
    EXPECT_EQ(inFlightReadouts(PanelPhase::Climb, ksanKsjc()),
              withEmergency("1:34", {r("CRUISE", "FL400"), toc(), tod(), landingAt()}));
}

// D-192, D-213: the predictions in the climb.
TEST(InFlightReadouts, ClimbPredictions) {
    InFlightFacts f = ksanKsjc();
    f.tocNm = 42;
    f.tocTime = "11:01 AM";
    f.todNm = 180;
    f.todTime = "11:22 AM";
    f.landingTime = "12:20 PM";
    f.planLandingTime = "12:24 PM";
    EXPECT_EQ(inFlightReadouts(PanelPhase::Climb, f),
              withEmergency("1:34", {r("CRUISE", "FL400"), toc("42", "11:01 AM"), tod("180", "11:22 AM"),
                                     landingAt("12:20 PM", "PLAN 12:24 PM")}));
}

// Two rows of four in the cruise.
TEST(InFlightReadouts, CruisePredictions) {
    InFlightFacts f = ksanKsjc();
    f.destinationAtis = &kKsjcAtis;
    f.gate = "Gate 4";
    f.todNm = 63;
    f.todTime = "11:22 AM";
    f.landingTime = "12:20 PM";
    f.planLandingTime = "12:24 PM";
    f.fuelAtLandingLb = 6380;
    f.planLandingFuelLb = 6175;
    EXPECT_EQ(inFlightReadouts(PanelPhase::Cruise, f),
              withEmergency("1:34", {r("CRUISE", "FL400"), words(r("DEST ATIS", "JULIETT", "", "30L 30R")),
                                     tod("63", "11:22 AM"), landingAt("12:20 PM", "PLAN 12:24 PM"),
                                     fuelAtLanding("6,380", "PLAN 6,175"), r("ARR GATE", "4")}));
}

TEST(InFlightReadouts, Cruise) {
    InFlightFacts f = ksanKsjc();
    f.destinationAtis = &kKsjcAtis;
    f.gate = "Gate 4";  // SayIntentions' pick, usually in cruise (0.15.1)
    EXPECT_EQ(inFlightReadouts(PanelPhase::Cruise, f),
              withEmergency("1:34", {r("CRUISE", "FL400"), words(r("DEST ATIS", "JULIETT", "", "30L 30R")), tod(),
                                     landingAt(), fuelAtLanding(), r("ARR GATE", "4")}));
}

TEST(InFlightReadouts, CruiseBeforeDestAtis) {
    EXPECT_EQ(inFlightReadouts(PanelPhase::Cruise, ksanKsjc()),
              withEmergency("1:34", {r("CRUISE", "FL400"), words(r("DEST ATIS", "")), tod(), landingAt(),
                                     fuelAtLanding(), r("ARR GATE", "")}));
}

TEST(InFlightReadouts, Arrival) {
    InFlightFacts f = ksanKsjc();
    f.destinationAtis = &kKsjcAtis;
    f.atcStar = "SILCN6 TROXX";
    f.atcRunway = "30L";
    ReadoutView approaches = words(r("APPROACHES", "ILS 30L · RNAV-Z 30R"));
    approaches.wide = true;
    EXPECT_EQ(inFlightReadouts(PanelPhase::Arrival, f),
              withEmergency("1:34", {words(r("ATIS", "JULIETT")), words(r("RUNWAYS", "30L 30R")), approaches,
                                     words(r("WIND", "Variable / 4 kt")), r("OAT", "17", "°C"),
                                     r("ALTIMETER", "30.19", "inHg", "SET AT TL"), words(r("CONDITION", "DRY")),
                                     words(r("STAR", "SILCN6 TROXX", "", "ATC")), r("LANDING RWY", "30L", "", "ATC"),
                                     r("ARR GATE", "")}));
}

TEST(InFlightReadouts, ArrivalBeforeAtcUsesThePlan) {
    InFlightFacts f = ksanKsjc();
    f.destinationAtis = &kKsjcAtis;
    const auto out = inFlightReadouts(PanelPhase::Arrival, f);
    EXPECT_EQ(out[9], words(r("STAR", "SILCN6 TROXX", "", "PLAN")));
    EXPECT_EQ(out[10], r("LANDING RWY", "30L", "", "ATIS"));  // the ATIS's first arriving runway, the plan's too
}

TEST(InFlightReadouts, ArrivalFlagsAtcRunway) {
    InFlightFacts f = ksanKsjc();
    f.atcRunway = "30R";
    f.atcStar = "BRIXX1 TROXX";
    const auto out = inFlightReadouts(PanelPhase::Arrival, f);
    EXPECT_EQ(out[10], flagged(r("LANDING RWY", "30R", "", "PLAN 30L")));
    EXPECT_EQ(out[9], flagged(words(r("STAR", "BRIXX1 TROXX", "", "PLAN SILCN6"))));
}

TEST(InFlightReadouts, ShortFinal) {
    InFlightFacts f = ksanKsjc();
    f.destinationAtis = &kKsjcAtis;
    f.atcRunway = "30L";
    f.towerWind = "360° / 4 kt";
    f.vrefKt = 129;
    f.landingFlaps = 40;
    f.fuelLb = 7410;
    f.gate = "Parking 70";
    EXPECT_EQ(inFlightReadouts(PanelPhase::ShortFinal, f),
              withEmergency("1:32", {words(r("WIND", "360° / 4 kt", "", "TOWER")), r("RUNWAY", "30L", "", "ATC"),
                                     r("VREF", "129", "KT", "FLAPS 40"), words(r("ARR GATE", "Parking 70"))}));
}

TEST(InFlightReadouts, ShortFinalWindFromTheAtisWithoutTheTower) {
    InFlightFacts f = ksanKsjc();
    f.destinationAtis = &kKsjcAtis;
    EXPECT_EQ(inFlightReadouts(PanelPhase::ShortFinal, f)[2], words(r("WIND", "Variable / 4 kt", "", "ATIS")));
}

TEST(InFlightReadouts, Landed) {
    InFlightFacts f = ksanKsjc();
    f.stars = 5;
    f.touchdownFpm = 100;
    f.touchdownG = 1.18;  // 0.15.1: the G after the sink rate
    f.gate = "Gate 4";
    f.fuelLb = 7190;
    ReadoutView landing = r("LANDING", "", "", "−100 FPM · 1.18 G");
    landing.stars = 5;
    EXPECT_EQ(inFlightReadouts(PanelPhase::Landed, f), withEmergency("1:29", {landing, r("ARR GATE", "4")}));
}

TEST(InFlightReadouts, SoulsAndFuelFirstInEveryPhase) {
    InFlightFacts f = ksanKsjc();
    for (PanelPhase p : {PanelPhase::Departure, PanelPhase::Climb, PanelPhase::Cruise, PanelPhase::Arrival,
                         PanelPhase::ShortFinal, PanelPhase::Landed}) {
        const auto out = inFlightReadouts(p, f);
        ASSERT_GE(out.size(), 2u);
        EXPECT_EQ(out[0], souls());
        EXPECT_EQ(out[1], fuel("1:34"));
    }
    f.plan.enduranceS = 0;  // no endurance figure: a dash
    EXPECT_EQ(inFlightReadouts(PanelPhase::Climb, f)[1], fuel(""));
    f.passengers = 0;
    f.crew = 2;  // a freighter
    EXPECT_EQ(inFlightReadouts(PanelPhase::Climb, f)[0], r("SOULS", "2", "", "2 CREW"));
}
