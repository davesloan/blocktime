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

TEST(FlightStages, ShowNextLegStartsOver) {
    FlightStages s;
    phase(s, "pushback", "22:51:08", 25);
    s.showNextLeg();
    EXPECT_TRUE(s.view()[1].lines.empty());
    EXPECT_EQ(s.view()[0].state, StageState::Current);
    EXPECT_FALSE(s.showingFlight());
}

// --- GSX services and the shutdown checklist (milestone 8, D-134..D-136, D-138) ---------------------------------

namespace {

void gsx(FlightStages& s, const std::string& service, const std::string& state, const std::string& hms) {
    s.onEvent("gsx_service", {{"service", service}, {"state", state}, {"time", at(hms)}});
}
void block(FlightStages& s, const std::string& type, const std::string& hms) {
    s.onEvent(type, {{"flight_id", "F1"}, {"time", at(hms)}});
}
// Block out to arrived, with no GSX.
void flyTo(FlightStages& s, const std::string& inHms) {
    block(s, "block_out", "10:00:00");
    phase(s, "pushback", "10:00:00", 25);
    phase(s, "taxi_out", "10:03:00", 25);
    block(s, "block_in", inHms);
    phase(s, "arrived", inHms, 25);
}
std::vector<std::string> checkTexts(const StageLine& l) {
    std::vector<std::string> out;
    for (const auto& c : l.checks) out.push_back(c.text);
    return out;
}

}  // namespace

TEST(FlightStages, NextLegServicesShowBeforeBlockOut) {
    FlightStages s;
    gsx(s, "refuelling", "performing", "09:30:00");
    const auto v = s.view();
    EXPECT_EQ(v[0].state, StageState::Current);
    ASSERT_EQ(v[0].lines.size(), 1u);
    EXPECT_EQ(v[0].lines[0].text, "Refuelling");
    EXPECT_TRUE(v[0].lines[0].running);
    EXPECT_EQ(v[0].lines[0].start, "09:30Z");
    EXPECT_TRUE(s.hasNextLegServices());
    EXPECT_FALSE(s.showingFlight());
}

TEST(FlightStages, DepartedServicesKeptAfterBlockOut) {
    FlightStages s;
    gsx(s, "refuelling", "performing", "09:30:00");
    gsx(s, "refuelling", "completed", "09:40:00");
    block(s, "block_out", "10:00:00");
    phase(s, "pushback", "10:00:00", 25);
    const auto v = s.view();
    EXPECT_EQ(v[0].state, StageState::Done);
    ASSERT_EQ(v[0].lines.size(), 1u);
    EXPECT_EQ(v[0].lines[0].text, "Refuelling");
    EXPECT_EQ(v[0].lines[0].start, "09:30Z");
    EXPECT_EQ(v[0].lines[0].end, "09:40Z");
    EXPECT_EQ(texts(v[1]), std::vector<std::string>{"Pushback"});
    EXPECT_TRUE(s.showingFlight());
    EXPECT_FALSE(s.hasNextLegServices());
}

TEST(FlightStages, NoServicesPreDepartureEmptyAndDone) {
    FlightStages s;
    block(s, "block_out", "10:00:00");
    phase(s, "pushback", "10:00:00", 25);
    const auto v = s.view();
    EXPECT_EQ(v[0].state, StageState::Done);
    EXPECT_TRUE(v[0].lines.empty());
}

TEST(FlightStages, PushbackByGsxLabel) {
    FlightStages s;
    gsx(s, "pushback", "performing", "09:59:50");
    block(s, "block_out", "10:00:00");
    phase(s, "pushback", "10:00:00", 25);
    EXPECT_EQ(texts(s.view()[1]), std::vector<std::string>{"Pushback (GSX)"});
}

TEST(FlightStages, DeboardingAndReadyOnCompletion) {
    FlightStages s;
    flyTo(s, "12:00:00");
    gsx(s, "deboarding", "performing", "12:02:00");
    gsx(s, "deboarding", "completed", "12:08:00");
    const auto v = s.view();
    EXPECT_EQ(texts(v[5]),
              (std::vector<std::string>{"Parked", "Deboarding", "Ready for shutdown or turnaround"}));
    const StageLine& ready = v[5].lines[2];
    EXPECT_EQ(ready.start, "12:08Z");
    EXPECT_EQ(checkTexts(ready), (std::vector<std::string>{"Engines off", "Parking brake set", "Deboarding complete",
                                                            "Beacon off", "Strobes off"}));
    EXPECT_TRUE(ready.checks[2].done);
    EXPECT_EQ(v[5].lines[1].end, "12:08Z");
}

TEST(FlightStages, ReadyOnChecksWithoutGsx) {
    FlightStages s;
    flyTo(s, "12:00:00");
    EXPECT_TRUE(s.onShutdownChecks({true, true, true, true}, at("12:03:00")));
    const auto v = s.view();
    ASSERT_EQ(texts(v[5]), (std::vector<std::string>{"Parked", "Ready for shutdown or turnaround"}));
    EXPECT_EQ(v[5].lines[1].start, "12:03Z");
    EXPECT_EQ(checkTexts(v[5].lines[1]),
              (std::vector<std::string>{"Engines off", "Parking brake set", "Beacon off", "Strobes off"}));
    for (const auto& c : v[5].lines[1].checks) EXPECT_TRUE(c.done);
}

TEST(FlightStages, ChecksNotAllPassedNoReady) {
    FlightStages s;
    flyTo(s, "12:00:00");
    s.onShutdownChecks({true, true, false, true}, at("12:03:00"));
    EXPECT_EQ(texts(s.view()[5]), std::vector<std::string>{"Parked"});
}

TEST(FlightStages, ReadyWaitsForRunningDeboarding) {
    FlightStages s;
    flyTo(s, "12:00:00");
    gsx(s, "deboarding", "performing", "12:02:00");
    s.onShutdownChecks({true, true, true, true}, at("12:03:00"));
    EXPECT_EQ(texts(s.view()[5]), (std::vector<std::string>{"Parked", "Deboarding"}));
    gsx(s, "deboarding", "completed", "12:08:00");
    EXPECT_EQ(texts(s.view()[5]),
              (std::vector<std::string>{"Parked", "Deboarding", "Ready for shutdown or turnaround"}));
}

TEST(FlightStages, LateDeboardingMovesReady) {
    FlightStages s;
    flyTo(s, "12:00:00");
    // Dave shuts down and turns the beacon off first, then calls GSX deboarding.
    EXPECT_TRUE(s.onShutdownChecks({true, true, true, true}, at("12:03:00")));
    EXPECT_EQ(s.view()[5].lines.back().text, "Ready for shutdown or turnaround");
    gsx(s, "deboarding", "performing", "12:05:00");
    EXPECT_EQ(texts(s.view()[5]), (std::vector<std::string>{"Parked", "Deboarding"}));
    gsx(s, "deboarding", "completed", "12:10:00");
    const auto v = s.view();
    ASSERT_EQ(texts(v[5]), (std::vector<std::string>{"Parked", "Deboarding", "Ready for shutdown or turnaround"}));
    EXPECT_EQ(v[5].lines[2].start, "12:10Z");
    EXPECT_TRUE(v[5].lines[2].checks[2].done);  // Deboarding complete
}

TEST(FlightStages, ChecksUpdateLive) {
    FlightStages s;
    flyTo(s, "12:00:00");
    gsx(s, "deboarding", "performing", "12:02:00");
    s.onShutdownChecks({true, true, false, true}, at("12:03:00"));
    gsx(s, "deboarding", "completed", "12:08:00");
    EXPECT_FALSE(s.view()[5].lines[2].checks[3].done);  // beacon still on
    EXPECT_TRUE(s.onShutdownChecks({true, true, true, true}, at("12:09:00")));
    EXPECT_TRUE(s.view()[5].lines[2].checks[3].done);
    EXPECT_FALSE(s.onShutdownChecks({true, true, true, true}, at("12:10:00")));
}

TEST(FlightStages, ChecksBeforeBlockInChangeNothing) {
    FlightStages s;
    block(s, "block_out", "10:00:00");
    phase(s, "pushback", "10:00:00", 25);
    EXPECT_FALSE(s.onShutdownChecks({true, true, true, true}, at("10:01:00")));
    EXPECT_TRUE(s.view()[5].lines.empty());
}

TEST(FlightStages, ShowNextLegDropsTheFlight) {
    FlightStages s;
    flyTo(s, "12:00:00");
    gsx(s, "catering", "performing", "12:05:00");
    gsx(s, "catering", "completed", "12:15:00");
    EXPECT_TRUE(s.view()[0].lines.empty());  // still the last flight's view: it had no pre-departure services
    s.showNextLeg();
    const auto v = s.view();
    for (int i = 1; i < 6; ++i) EXPECT_TRUE(v[static_cast<std::size_t>(i)].lines.empty());
    EXPECT_EQ(v[0].state, StageState::Current);
    EXPECT_EQ(texts(v[0]), std::vector<std::string>{"Catering"});
    EXPECT_EQ(s.phaseLabel(), "");
    EXPECT_FALSE(s.onShutdownChecks({true, true, true, true}, at("12:20:00")));
}

TEST(FlightStages, AbandonedLineDisappears) {
    FlightStages s;
    gsx(s, "catering", "performing", "09:30:00");
    gsx(s, "catering", "bypassed", "09:31:00");
    EXPECT_TRUE(s.view()[0].lines.empty());
}

TEST(FlightStages, InterruptedShowsTheNextLeg) {
    FlightStages s;
    block(s, "block_out", "10:00:00");
    phase(s, "pushback", "10:00:00", 25);
    block(s, "flight_interrupted", "10:05:00");
    EXPECT_FALSE(s.showingFlight());
    EXPECT_TRUE(s.view()[1].lines.empty());
}

TEST(FlightStages, BlockOutStartsAFreshFlight) {
    FlightStages s;
    flyTo(s, "12:00:00");
    block(s, "block_out", "13:00:00");
    EXPECT_TRUE(s.view()[5].lines.empty());
    EXPECT_TRUE(s.view()[1].lines.empty());
}
