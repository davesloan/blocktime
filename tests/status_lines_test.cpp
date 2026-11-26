// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "status_lines.h"

using namespace blocktime;
using nlohmann::json;

namespace {

int minute = 0;

// An event at 18:MM:00Z, one minute after the previous one.
json at(json d = json::object()) {
    d["time"] = std::format("2026-11-25T18:{:02}:00Z", minute++ % 60);
    return d;
}

std::vector<std::string> texts(const StatusLines& s) {
    std::vector<std::string> out;
    for (const StatusLine& l : s.lines()) out.push_back(l.text);
    return out;
}

using Texts = std::vector<std::string>;

StatusContext sanJose() {
    StatusContext c;
    c.cruiseAltitudeFt = 37000;
    c.transitionAltitudeFt = 18000;
    c.transitionLevelFt = 18000;
    c.destinationCity = "San Jose";
    c.departureRunway = "27";
    return c;
}

void phase(StatusLines& s, const char* p, int altFt = 0, bool fl = false) {
    s.onEvent("phase_changed", at({{"phase", p}, {"alt_ft", altFt}, {"flight_level", fl}}));
}

void gsx(StatusLines& s, const char* service, const char* state) {
    s.onEvent("gsx_service", at({{"service", service}, {"state", state}}));
}

// Block out through liftoff and the climb.
void departAndClimb(StatusLines& s) {
    s.onEvent("block_out", at({{"flight_id", "F1"}}));
    phase(s, "pushback");
    phase(s, "taxi_out");
    phase(s, "takeoff");
    s.onEvent("liftoff", at({{"runway", "27"}}));
    phase(s, "climb", 1500);
}

void cruiseAndDescend(StatusLines& s) {
    phase(s, "cruise", 37000, true);
    phase(s, "descent", 37000, true);
    phase(s, "approach", 6000);
}

}  // namespace

TEST(StatusLines, PreFlightStartsPreparing) {
    StatusLines s;
    EXPECT_EQ(texts(s), Texts{"Preparing aircraft for flight"});
    EXPECT_TRUE(s.checklist().empty());
}

TEST(StatusLines, GsxSharesOneLine) {
    StatusLines s;
    gsx(s, "catering", "requested");
    EXPECT_EQ(texts(s), Texts{"Preparing aircraft for flight"});
    gsx(s, "catering", "performing");
    EXPECT_EQ(texts(s), Texts{"Catering in progress"});
    gsx(s, "refuelling", "performing");
    EXPECT_EQ(texts(s), Texts{"Catering in progress · Refuelling in progress"});
    gsx(s, "catering", "completed");
    EXPECT_EQ(texts(s), Texts{"Catering complete · Refuelling in progress"});
}

TEST(StatusLines, DepartureSequence) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    EXPECT_EQ(texts(s), (Texts{"Climbing to FL370", "Departed runway 27", "Takeoff roll"}));
    EXPECT_EQ(s.lines()[1].startIso, "2026-11-25T18:" + std::format("{:02}", (minute - 2) % 60) + ":00Z");
}

TEST(StatusLines, TaxiOutNamesTheRunway) {
    StatusLines s;
    s.setContext(sanJose());
    s.onEvent("block_out", at());
    phase(s, "pushback");
    phase(s, "taxi_out");
    EXPECT_EQ(texts(s)[0], "Taxiing to runway 27");
    EXPECT_EQ(texts(s)[1], "Pushback and startup");
    StatusLines t;
    t.onEvent("block_out", at());
    phase(t, "taxi_out");
    EXPECT_EQ(texts(t)[0], "Taxiing to runway");
}

TEST(StatusLines, LevelOffInPlace) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    s.onEvent("level_off", at({{"phase", "climb"}, {"alt_ft", 24000}, {"flight_level", true}}));
    EXPECT_EQ(texts(s), (Texts{"Level at FL240", "Departed runway 27", "Takeoff roll"}));
    s.onEvent("level_off_ended", at({{"phase", "climb"}, {"alt_ft", 24000}, {"flight_level", true}}));
    EXPECT_EQ(texts(s), (Texts{"Climbing to FL370", "Departed runway 27", "Takeoff roll"}));
}

TEST(StatusLines, LevelOffInTheApproach) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    cruiseAndDescend(s);
    s.onEvent("level_off", at({{"phase", "approach"}, {"alt_ft", 3000}, {"flight_level", false}}));
    EXPECT_EQ(texts(s)[0], "Level at 3,000 ft");
    EXPECT_EQ(texts(s)[1], "Descending into San Jose");
    s.onEvent("level_off_ended", at({{"phase", "approach"}, {"alt_ft", 3000}, {"flight_level", false}}));
    EXPECT_EQ(texts(s)[0], "Approach into San Jose");
}

TEST(StatusLines, ClimbWithoutPlan) {
    StatusLines s;
    departAndClimb(s);
    EXPECT_EQ(texts(s)[0], "Climbing");
}

TEST(StatusLines, StepClimb) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    phase(s, "cruise", 37000, true);
    EXPECT_EQ(texts(s)[0], "Cruising at FL370");
    s.onEvent("step_climb", at({{"from_ft", 37000}}));
    EXPECT_EQ(texts(s), (Texts{"Leaving FL370", "Cruising at FL370", "Climbing to FL370"}));
    s.onEvent("step_climb_ended", at({{"alt_ft", 39000}, {"flight_level", true}}));
    EXPECT_EQ(texts(s), (Texts{"Cruising at FL390", "Cruising at FL370", "Climbing to FL370"}));
    s.onEvent("step_climb", at({{"from_ft", 39000}}));
    s.onEvent("step_climb_ended", at({{"alt_ft", 35000}, {"flight_level", true}}));
    EXPECT_EQ(texts(s)[0], "Cruising at FL350");
}

TEST(StatusLines, DescentAndLanding) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    cruiseAndDescend(s);
    EXPECT_EQ(texts(s), (Texts{"Approach into San Jose", "Descending into San Jose", "Cruising at FL370"}));
    s.onEvent("lined_up", at({{"runway", "30L"}}));
    EXPECT_EQ(texts(s)[0], "Final approach, runway 30L");
    s.onEvent("touchdown", at({{"vs_fpm", -120.0}}));
    phase(s, "landing");
    EXPECT_EQ(texts(s), (Texts{"Landed on runway 30L", "Final approach, runway 30L", "Approach into San Jose"}));
    phase(s, "taxi_in");
    s.onEvent("landing_runway", json{{"runway", "30R"}});  // no time, after taxi in, as the recorder logs it
    EXPECT_EQ(texts(s), (Texts{"Taxiing to gate", "Landed on runway 30R", "Final approach, runway 30L"}));
}

TEST(StatusLines, GoAroundAndSecondApproach) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    cruiseAndDescend(s);
    s.onEvent("lined_up", at({{"runway", "30L"}}));
    phase(s, "go_around", 800);
    EXPECT_EQ(texts(s)[0], "Go-around");
    phase(s, "approach", 3000);
    s.onEvent("lined_up", at({{"runway", "30L"}}));
    s.onEvent("touchdown", at());
    EXPECT_EQ(texts(s), (Texts{"Landed on runway 30L", "Final approach, runway 30L", "Approach into San Jose"}));
}

TEST(StatusLines, ArrivalGateNames) {
    EXPECT_EQ(arrivalGateName({{"result", "assigned"}, {"gate", "Gate 4"}, {"si_gate", "Terminal A Gate 4"}}), "Gate 4");
    EXPECT_EQ(arrivalGateName({{"result", "unconfirmed"}, {"gate", "Gate 4"}, {"si_gate", "A4"}}), "Gate 4");
    EXPECT_EQ(arrivalGateName({{"result", "kept"}, {"gate", "Gate 4"}, {"si_gate", "Terminal A Gate 4"}}),
              "Terminal A Gate 4");

    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    cruiseAndDescend(s);
    s.onEvent("arrival_gate", at({{"result", "assigned"}, {"gate", "Gate 4"}, {"si_gate", "Terminal A Gate 4"}}));
    s.onEvent("touchdown", at());
    phase(s, "taxi_in");
    EXPECT_EQ(texts(s)[0], "Taxiing to Gate 4");
    s.onEvent("block_in", at({{"flight_id", "F1"}, {"parking", "Parking 70"}}));
    phase(s, "arrived");
    EXPECT_EQ(texts(s), (Texts{"Parked at Parking 70", "Taxiing to Gate 4", "Landed"}));
}

TEST(StatusLines, ChecklistFromBlockIn) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    cruiseAndDescend(s);
    s.onEvent("touchdown", at());
    phase(s, "taxi_in");
    EXPECT_TRUE(s.checklist().empty());
    s.onEvent("block_in", at({{"flight_id", "F1"}, {"parking", "Gate C11"}}));
    phase(s, "arrived");
    ASSERT_EQ(s.checklist().size(), 4u);
    for (const ChecklistItem& c : s.checklist()) EXPECT_FALSE(c.done) << c.text;
    EXPECT_EQ(texts(s)[0], "Parked at Gate C11");

    EXPECT_TRUE(s.onShutdownChecks({true, true, true, true}, "2026-11-25T19:30:00Z"));
    EXPECT_EQ(texts(s)[0], "Ready for next flight");
    for (const ChecklistItem& c : s.checklist()) EXPECT_TRUE(c.done) << c.text;

    EXPECT_TRUE(s.onShutdownChecks({true, true, false, true}, "2026-11-25T19:31:00Z"));  // the beacon back on
    EXPECT_EQ(texts(s)[0], "Ready for next flight");
    EXPECT_FALSE(s.checklist()[2].done);

    s.showNextLeg();
    EXPECT_TRUE(s.checklist().empty());
    EXPECT_EQ(texts(s), Texts{"Preparing aircraft for flight"});
}

TEST(StatusLines, DeboardingThenReady) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    cruiseAndDescend(s);
    s.onEvent("touchdown", at());
    phase(s, "taxi_in");
    s.onEvent("block_in", at({{"flight_id", "F1"}, {"parking", "Gate C11"}}));
    phase(s, "arrived");
    gsx(s, "deboarding", "performing");
    EXPECT_EQ(texts(s)[0], "Deboarding in progress");
    ASSERT_EQ(s.checklist().size(), 5u);
    gsx(s, "deboarding", "completed");
    EXPECT_EQ(texts(s), (Texts{"Ready for next flight", "Deboarding complete", "Parked at Gate C11"}));
    EXPECT_TRUE(s.checklist()[2].done);  // Deboarding complete
}

TEST(StatusLines, InterruptedClearsTheFlight) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    s.onEvent("flight_interrupted", at({{"flight_id", "F1"}}));
    EXPECT_EQ(texts(s), Texts{"Preparing aircraft for flight"});
}

TEST(StatusLines, MarkersAreNotLines) {
    StatusLines s;
    s.setContext(sanJose());
    departAndClimb(s);
    const Texts before = texts(s);
    s.onEvent("flight_marker", at({{"marker", "above_10000"}}));
    EXPECT_EQ(texts(s), before);
}

TEST(StatusLines, BlockOutKeepsThePreFlightLine) {
    StatusLines s;
    gsx(s, "boarding", "performing");
    gsx(s, "boarding", "completed");
    s.onEvent("block_out", at());
    phase(s, "pushback");
    EXPECT_EQ(texts(s), (Texts{"Pushback and startup", "Boarding complete"}));
}

TEST(StatusLines, KsanKsjcFixture) {
    StatusLines s;
    StatusContext c = sanJose();
    c.departureRunway = "";
    s.setContext(c);
    std::ifstream in(BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc/events-2026-11-25.jsonl");
    std::string line;
    Texts seen;
    while (std::getline(in, line)) {
        const json j = json::parse(line, nullptr, false);
        if (!j.is_object() || j.value("type", "") == "telemetry") continue;
        json d = j.value("data", json::object());
        if (!d.contains("time")) d["time"] = j.value("ts", "");
        s.onEvent(j.value("type", ""), d);
        for (const StatusLine& l : s.lines()) {
            EXPECT_FALSE(l.text.empty()) << "after " << j.value("type", "");
            if (seen.empty() || seen.back() != l.text) seen.push_back(l.text);
        }
    }
    const Texts last = texts(s);
    ASSERT_FALSE(last.empty());
    const bool parkedOrReady = std::any_of(last.begin(), last.end(), [](const std::string& t) {
        return t.starts_with("Parked") || t == "Ready for next flight";
    });
    EXPECT_TRUE(parkedOrReady) << last[0];
    EXPECT_NE(std::find(seen.begin(), seen.end(), "Climbing to FL370"), seen.end());
    EXPECT_NE(std::find(seen.begin(), seen.end(), "Descending into San Jose"), seen.end());
}
