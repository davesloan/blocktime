// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The six stages of design.md §13.5, built from the recorder's flight events (D-128).

#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "flight_stages.h"

using namespace blocktime;
using nlohmann::json;

namespace {

std::string at(const std::string& hms) { return "2026-11-23T" + hms + ".000Z"; }

void phase(FlightStages& s, const std::string& p, const std::string& hms, int alt = 0, bool fl = false) {
    s.onEvent("phase_changed", {{"phase", p}, {"time", at(hms)}, {"alt_ft", alt}, {"flight_level", fl}});
}

std::vector<std::string> texts(const StageView& v) {
    std::vector<std::string> out;
    for (const auto& l : v.lines) out.push_back(l.text);
    return out;
}

}  // namespace

TEST(FlightStages, SixStagesBeforeAnything) {
    FlightStages s;
    const auto v = s.view();
    ASSERT_EQ(v.size(), 6u);
    const std::vector<std::string> names{"Pre-Departure", "Departure", "Climb", "Cruise", "Descent", "Arrival"};
    for (std::size_t i = 0; i < 6; ++i) {
        EXPECT_EQ(v[i].name, names[i]);
        EXPECT_TRUE(v[i].lines.empty());
    }
    EXPECT_EQ(v[0].state, StageState::Current);
    EXPECT_EQ(v[1].state, StageState::Upcoming);
    EXPECT_EQ(s.phaseLabel(), "");
}

TEST(FlightStages, ADepartureInOrder) {
    FlightStages s;
    phase(s, "pushback", "22:51:08", 25);
    phase(s, "taxi_out", "22:54:17", 25);
    phase(s, "takeoff", "22:56:08", 25);
    s.onEvent("liftoff", {{"time", at("22:56:32")}, {"runway", "14R"}, {"airport", "KBFI"}});
    phase(s, "climb", "22:56:51", 1100);
    const auto v = s.view();
    EXPECT_EQ(v[0].state, StageState::Done);
    EXPECT_EQ(v[1].state, StageState::Done);
    EXPECT_EQ(v[2].state, StageState::Current);
    EXPECT_EQ(texts(v[1]), (std::vector<std::string>{"Pushback", "Taxiing to runway", "Takeoff roll", "Takeoff"}));
    EXPECT_EQ(v[1].lines[0].start, "22:51Z");
    EXPECT_EQ(v[1].lines[0].end, "22:54Z");
    EXPECT_EQ(v[1].lines[2].end, "22:56Z");  // the roll ends at liftoff
    EXPECT_FALSE(v[1].lines[2].running);
    EXPECT_EQ(v[1].lines[3].end, "");  // an instant
    EXPECT_EQ(texts(v[2]), (std::vector<std::string>{"Departed runway 14R"}));
}

TEST(FlightStages, LevelOffTextUsesTheEndedAltitude) {
    FlightStages s;
    phase(s, "descent", "01:00:00", 30000, true);
    phase(s, "approach", "01:18:19", 9700);
    s.onEvent("level_off", {{"phase", "approach"}, {"time", at("01:25:02")}, {"alt_ft", 3100}, {"flight_level", false}});
    EXPECT_EQ(s.view()[4].lines.back().text, "Level at 3,100 ft");
    EXPECT_TRUE(s.view()[4].lines.back().running);
    EXPECT_EQ(s.phaseLabel(), "Approach — level at 3,100 ft");
    s.onEvent("level_off_ended",
              {{"phase", "approach"}, {"time", at("01:26:25")}, {"alt_ft", 3000}, {"flight_level", false}});
    const auto line = s.view()[4].lines.back();
    EXPECT_EQ(line.text, "Level at 3,000 ft");
    EXPECT_EQ(line.start, "01:25Z");
    EXPECT_EQ(line.end, "01:26Z");
    EXPECT_FALSE(line.running);
    EXPECT_EQ(s.phaseLabel(), "Approach");
}

TEST(FlightStages, FlightLevels) {
    FlightStages s;
    phase(s, "cruise", "00:23:12", 39000, true);
    EXPECT_EQ(texts(s.view()[3]), (std::vector<std::string>{"Cruising at FL390"}));
    s.onEvent("step_climb", {{"time", at("01:00:00")}, {"from_ft", 39000}, {"flight_level", true}});
    EXPECT_EQ(s.phaseLabel(), "Cruise — step climb");
    s.onEvent("step_climb_ended", {{"time", at("01:03:00")}, {"alt_ft", 41000}, {"flight_level", true}});
    const auto v = s.view()[3];
    EXPECT_EQ(texts(v), (std::vector<std::string>{"Cruising at FL390", "Step climb to FL410", "Cruising at FL410"}));
    EXPECT_EQ(v.lines[0].end, "01:00Z");
    EXPECT_TRUE(v.lines[2].running);
}

TEST(FlightStages, StepDownReadsDescended) {
    FlightStages s;
    phase(s, "cruise", "00:23:12", 39000, true);
    s.onEvent("step_climb", {{"time", at("01:00:00")}, {"from_ft", 39000}, {"flight_level", true}});
    s.onEvent("step_climb_ended", {{"time", at("01:03:00")}, {"alt_ft", 37000}, {"flight_level", true}});
    EXPECT_EQ(s.view()[3].lines[1].text, "Descended to FL370");
}

TEST(FlightStages, LandedWithoutARunway) {
    FlightStages s;
    phase(s, "approach", "23:12:34", 9000);
    s.onEvent("touchdown", {{"time", at("23:27:16")}, {"vs_fpm", -142.0}, {"g", 1.21}});
    phase(s, "landing", "23:27:16", 27);
    const auto v = s.view()[4];
    ASSERT_EQ(texts(v), (std::vector<std::string>{"Approach", "Landed, −142 fpm"}));
    EXPECT_EQ(v.lines[0].end, "23:27Z");
}

TEST(FlightStages, LandedThenRunwayUpdatesTheLine) {
    FlightStages s;
    phase(s, "approach", "23:12:34", 9000);
    phase(s, "landing", "23:27:16", 27);
    EXPECT_EQ(texts(s.view()[4]).back(), "Landed");  // the touchdown can come a second after the phase
    s.onEvent("touchdown", {{"time", at("23:27:15")}, {"vs_fpm", -142.0}, {"g", 1.21}});
    s.onEvent("landing_runway", {{"runway", "10R"}, {"airport", "KPDX"}});
    phase(s, "taxi_in", "23:27:40", 27);
    const auto v = s.view()[4];
    EXPECT_EQ(texts(v), (std::vector<std::string>{"Approach", "Landed on 10R, −142 fpm", "Taxiing to gate"}));
    phase(s, "arrived", "23:30:23", 27);
    EXPECT_EQ(texts(s.view()[5]), (std::vector<std::string>{"Parked"}));
    EXPECT_EQ(s.view()[4].lines[2].end, "23:30Z");
    EXPECT_EQ(s.view()[5].state, StageState::Current);
}

TEST(FlightStages, Markers) {
    FlightStages s;
    phase(s, "climb", "22:56:51", 1100);
    s.onEvent("flight_marker", {{"marker", "above_10000"}, {"time", at("22:59:34")}, {"alt_ft", 10010}});
    s.onEvent("flight_marker", {{"marker", "transition_altitude"}, {"time", at("23:02:20")}, {"alt_ft", 18010}});
    phase(s, "cruise", "23:03:10", 20000, true);
    s.onEvent("flight_marker", {{"marker", "within_200nm"}, {"time", at("23:03:30")}, {"alt_ft", 20000}});
    phase(s, "descent", "23:04:32", 19900, true);
    s.onEvent("flight_marker", {{"marker", "transition_level"}, {"time", at("23:06:33")}, {"alt_ft", 17990}});
    s.onEvent("flight_marker", {{"marker", "below_10000"}, {"time", at("23:11:41")}, {"alt_ft", 9990}});
    s.onEvent("flight_marker", {{"marker", "final_approach"}, {"time", at("23:25:36")}, {"alt_ft", 990}});
    const auto v = s.view();
    EXPECT_EQ(texts(v[2]), (std::vector<std::string>{"Above 10,000 ft", "Passed transition altitude"}));
    EXPECT_EQ(texts(v[3]), (std::vector<std::string>{"Cruising at FL200", "200 nm from destination"}));
    EXPECT_EQ(texts(v[4]), (std::vector<std::string>{"Top of descent", "Passed transition level", "Below 10,000 ft",
                                                     "Final approach"}));
}

TEST(FlightStages, LinesSortByTimeWithinAStage) {
    FlightStages s;
    phase(s, "climb", "22:56:51", 1100);
    s.onEvent("flight_marker", {{"marker", "above_10000"}, {"time", at("22:59:34")}, {"alt_ft", 10010}});
    // A level-off is confirmed 20 s after it began, so it can arrive after a later marker.
    s.onEvent("level_off", {{"phase", "climb"}, {"time", at("22:57:15")}, {"alt_ft", 2000}, {"flight_level", false}});
    EXPECT_EQ(texts(s.view()[2]), (std::vector<std::string>{"Level at 2,000 ft", "Above 10,000 ft"}));
}

TEST(FlightStages, PhaseLabels) {
    FlightStages s;
    phase(s, "taxi_out", "22:54:17");
    EXPECT_EQ(s.phaseLabel(), "Taxi out");
    phase(s, "climb", "22:56:51", 1100);
    s.onEvent("level_off", {{"phase", "climb"}, {"time", at("23:00:05")}, {"alt_ft", 11000}, {"flight_level", false}});
    EXPECT_EQ(s.phaseLabel(), "Climb — level at 11,000 ft");
    phase(s, "go_around", "23:30:00", 800);
    EXPECT_EQ(s.phaseLabel(), "Go-around");
}

TEST(FlightStages, RunningLinesHaveNoEnd) {
    FlightStages s;
    phase(s, "pushback", "22:51:08", 25);
    const auto line = s.view()[1].lines[0];
    EXPECT_TRUE(line.running);
    EXPECT_EQ(line.end, "");
}

TEST(FlightStages, ResetStartsOver) {
    FlightStages s;
    phase(s, "pushback", "22:51:08", 25);
    s.reset();
    EXPECT_TRUE(s.view()[1].lines.empty());
    EXPECT_EQ(s.view()[0].state, StageState::Current);
}
