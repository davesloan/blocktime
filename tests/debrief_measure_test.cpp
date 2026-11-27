// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief's measurements from one flight's log lines (docs/debrief.md section 2, D-157, D-160), on the real
// KLAS → KSAN leg and on copies of it with data taken out.

#include <algorithm>
#include <functional>
#include <filesystem>
#include <set>
#include <string>

#include <gtest/gtest.h>

#include "debrief_measure.h"
#include "event_log.h"
#include "flight_events.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const std::filesystem::path kFixtureDir = BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan";
constexpr const char* kFlight = "2026-11-24T23:53:16.837Z";

const std::vector<LogLine>& fixture() {
    static const std::vector<LogLine> lines = loadFlightEvents(kFixtureDir, kFlight);
    return lines;
}

std::optional<int> value(const FlightMeasurements& m, const std::string& part) {
    for (const auto& v : m.values)
        if (v.part == part) return v.value;
    return std::nullopt;
}

std::string skipReason(const FlightMeasurements& m, const std::string& part) {
    for (const auto& s : m.skipped)
        if (s.part == part) return s.reason;
    return "";
}

void expectEveryPartOnce(const FlightMeasurements& m) {
    std::multiset<std::string> seen;
    for (const auto& v : m.values) seen.insert(v.part);
    for (const auto& s : m.skipped) seen.insert(s.part);
    EXPECT_EQ(seen.size(), kCheckParts.size());
    for (const char* p : kCheckParts) EXPECT_EQ(seen.count(p), 1u) << p;
}

std::vector<LogLine> without(std::vector<LogLine> lines, const std::function<bool(const LogLine&)>& drop) {
    lines.erase(std::remove_if(lines.begin(), lines.end(), drop), lines.end());
    return lines;
}

// A line inserted in time order, as the log would hold it.
std::vector<LogLine> with(std::vector<LogLine> lines, const std::string& at, std::string type, nlohmann::json data) {
    const auto t = *parseUtc(at);
    data["flight_id"] = kFlight;
    data["time"] = at;
    const auto pos = std::find_if(lines.begin(), lines.end(), [&](const LogLine& l) { return l.ts > t; });
    lines.insert(pos, LogLine{t, std::move(type), std::move(data)});
    return lines;
}

}  // namespace

TEST(DebriefMeasure, KlasKsanFixture) {
    ASSERT_FALSE(fixture().empty());
    const FlightMeasurements m = measureFlight(fixture(), false);
    expectEveryPartOnce(m);
    EXPECT_EQ(m.family, Family::B737);
    EXPECT_TRUE(m.skipped.empty());
    EXPECT_NEAR(*value(m, "beacon_late_s"), 163, 1);
    EXPECT_GT(*value(m, "belts_off_departure_s"), 700);
    EXPECT_EQ(*value(m, "taxi_max_kt"), 25);
    EXPECT_LE(*value(m, "origin_baro_off_tenths"), 3);
    EXPECT_EQ(*value(m, "takeoff_strobes"), 1);
    EXPECT_EQ(*value(m, "takeoff_landing_lights"), 1);
    // Descending through 10,000 ft at 275 kt, under 260 kt 17 s later (the spike's looser filter missed it).
    EXPECT_NEAR(*value(m, "over_250_s"), 17, 1);
    EXPECT_NEAR(*value(m, "std_late_s"), 111, 1);
    EXPECT_EQ(*value(m, "belts_off_descent_s"), 0);
    EXPECT_EQ(*value(m, "destination_baro_off_tenths"), 0);
    EXPECT_GT(*value(m, "landing_lights_off_s"), 360);
    EXPECT_EQ(*value(m, "stable_gear_down"), 1);
    EXPECT_EQ(*value(m, "stable_landing_flaps"), 1);
    EXPECT_NEAR(*value(m, "stable_sink_fpm"), 791, 1);
    EXPECT_NEAR(*value(m, "stable_vref_offset_kt"), 0, 1);
    EXPECT_EQ(*value(m, "stable_lined_up"), 1);
    EXPECT_LE(*value(m, "strobes_on_after_landing_s"), 60);
    EXPECT_EQ(*value(m, "apu_on_after_transition_s"), 0);  // off at 23:58, before takeoff
    EXPECT_EQ(*value(m, "apu_on_at_shutdown"), 1);    // switched on at 00:46 on the approach
    ASSERT_TRUE(m.landing);
    EXPECT_EQ(m.landing->vsFpm, 591);
    EXPECT_EQ(m.landing->gHundredths, 147);
    EXPECT_EQ(m.landing->bounces, 0);
    EXPECT_EQ(m.landing->iasKt, 125);
    EXPECT_EQ(m.landing->vrefKt, 125);
    EXPECT_EQ(m.landing->windDirDeg, 330);
    EXPECT_EQ(m.landing->windKt, 9);
    EXPECT_FALSE(m.landing->simPitchTenths);  // not in the fixture's log
    EXPECT_EQ(m.landing->offCentrelineDm, 27);
    EXPECT_EQ(m.landing->offHeadingTenths, 6);
}

TEST(DebriefMeasure, NoCockpitPmdgFieldsSkipsBeltsAndVref) {
    auto lines = fixture();
    for (auto& l : lines) {
        if (l.type != "cockpit") continue;
        l.data["pmdg"] = false;
        l.data.erase("vref_kt");
        l.data.erase("landing_flaps");
        l.data.erase("autopilot");
        l.data.erase("apu_switch");
        l.data.erase("apu_running");
    }
    const FlightMeasurements m = measureFlight(lines, false);
    expectEveryPartOnce(m);
    EXPECT_EQ(skipReason(m, "belts_off_departure_s"), "no_pmdg_data");
    EXPECT_EQ(skipReason(m, "belts_off_descent_s"), "no_pmdg_data");
    EXPECT_EQ(skipReason(m, "stable_landing_flaps"), "no_pmdg_data");
    EXPECT_EQ(skipReason(m, "stable_vref_offset_kt"), "no_pmdg_data");
    EXPECT_EQ(skipReason(m, "apu_on_after_transition_s"), "no_pmdg_data");
    EXPECT_EQ(skipReason(m, "apu_on_at_shutdown"), "no_pmdg_data");
    EXPECT_EQ(*value(m, "stable_gear_down"), 1);  // the plain gear SimVar
    // LIGHT LANDING misses the 737's retractable lights: without PMDG data they aren't judged.
    EXPECT_EQ(skipReason(m, "landing_lights_off_s"), "no_pmdg_data");
    EXPECT_EQ(skipReason(m, "takeoff_landing_lights"), "no_pmdg_data");
    EXPECT_EQ(*value(m, "takeoff_strobes"), 1);
    ASSERT_TRUE(m.landing);
    EXPECT_FALSE(m.landing->vrefKt);
}

TEST(DebriefMeasure, NoAtisSkipsAltimeters) {
    const auto m = measureFlight(without(fixture(), [](const LogLine& l) { return l.type == "atis"; }), false);
    expectEveryPartOnce(m);
    EXPECT_EQ(skipReason(m, "origin_baro_off_tenths"), "no_atis");
    EXPECT_EQ(skipReason(m, "destination_baro_off_tenths"), "no_atis");
    EXPECT_TRUE(value(m, "std_late_s"));
}

// D-200: check 2 runs from taxi out to the transition altitude, not 10,000 ft. On this leg above_10000 is at
// 00:08:13 and transition_altitude at 00:11:04, with the belts on from 00:09:28: 40 s off in between now count.
TEST(DebriefMeasure, BeltsWindowEndsAtTransitionAltitude) {
    const auto base = measureFlight(fixture(), false);
    nlohmann::json off = {{"pmdg", true}, {"seat_belts", 0}, {"landing_lights", false}, {"gear_down", false},
                          {"landing_flaps", 0}, {"vref_kt", 0}, {"autopilot", true}};
    nlohmann::json on = off;
    on["seat_belts"] = 2;
    auto lines = with(fixture(), "2026-11-25T00:10:00.000Z", "cockpit", off);
    lines = with(std::move(lines), "2026-11-25T00:10:40.000Z", "cockpit", on);
    const auto m = measureFlight(lines, false);
    EXPECT_EQ(*value(m, "belts_off_departure_s"), *value(base, "belts_off_departure_s") + 40);
}

TEST(DebriefMeasure, LowFlightSkipsTenThousandChecks) {
    const auto m = measureFlight(without(fixture(), [](const LogLine& l) { return l.type == "flight_marker"; }), false);
    expectEveryPartOnce(m);
    EXPECT_TRUE(value(m, "belts_off_departure_s"));  // to the descent instead
    EXPECT_EQ(skipReason(m, "std_late_s"), "no_marker");
    EXPECT_EQ(skipReason(m, "belts_off_descent_s"), "no_marker");
    EXPECT_EQ(skipReason(m, "destination_baro_off_tenths"), "no_marker");
    EXPECT_EQ(skipReason(m, "landing_lights_off_s"), "no_marker");
    EXPECT_EQ(skipReason(m, "stable_sink_fpm"), "no_marker");  // final_approach is a marker too
    EXPECT_EQ(skipReason(m, "apu_on_after_transition_s"), "no_marker");
    EXPECT_EQ(*value(m, "apu_on_at_shutdown"), 1);
}

TEST(DebriefMeasure, GoAroundJudgesTheLandingApproach) {
    // A first approach and a go-around before the one that landed: the stable approach is the last one's.
    auto lines = with(fixture(), "2026-11-25T00:44:00.000Z", "flight_marker", {{"marker", "final_approach"}});
    lines = with(lines, "2026-11-25T00:44:20.000Z", "phase_changed", {{"phase", "go_around"}});
    auto m = measureFlight(lines, false);
    expectEveryPartOnce(m);
    EXPECT_NEAR(*value(m, "stable_sink_fpm"), 791, 1);
    EXPECT_EQ(*value(m, "stable_lined_up"), 1);  // lined up at 00:45:02, after the go-around
    // Lined up only before a go-around: not lined up for the approach that landed.
    m = measureFlight(with(fixture(), "2026-11-25T00:46:00.000Z", "phase_changed", {{"phase", "go_around"}}), false);
    EXPECT_EQ(*value(m, "stable_lined_up"), 0);
}

TEST(DebriefMeasure, DiversionSkipsDestinationAltimeter) {
    const auto m = measureFlight(fixture(), true);
    expectEveryPartOnce(m);
    EXPECT_EQ(skipReason(m, "destination_baro_off_tenths"), "diverted");
    EXPECT_EQ(m.skipped.size(), 1u);
}

TEST(DebriefMeasure, BeaconDeadlineIsTheEngineStartWhenEarlier) {
    // Engines started 5 minutes before the brake release, beacon on 60 s after that start.
    std::vector<LogLine> lines;
    const auto at = [](const char* iso) { return *parseUtc(iso); };
    const auto tel = [&](const char* iso, bool running, bool beacon) {
        lines.push_back({at(iso), "telemetry", {{"eng_running", {running, running}}, {"beacon", beacon},
                                                {"on_ground", true}, {"gs_kt", 0}}});
    };
    tel("2026-11-24T10:00:00.000Z", true, true);   // the previous leg's engines, still running
    tel("2026-11-24T10:01:00.000Z", false, false);  // shut down at the gate
    tel("2026-11-24T10:10:00.000Z", true, false);   // started at the gate
    tel("2026-11-24T10:11:00.000Z", true, true);
    lines.push_back({at("2026-11-24T10:15:00.000Z"), "block_out",
                     {{"flight_id", "F"}, {"time", "2026-11-24T10:15:00.000Z"}, {"aircraft_folder", "pmdg 777-300er"}}});
    lines.push_back({at("2026-11-24T11:15:00.000Z"), "block_in", {{"flight_id", "F"}, {"time", "2026-11-24T11:15:00.000Z"}}});
    const auto m = measureFlight(lines, false);
    expectEveryPartOnce(m);
    EXPECT_EQ(m.family, Family::B777);
    EXPECT_EQ(*value(m, "beacon_late_s"), 60);
    EXPECT_FALSE(m.landing);
}

TEST(DebriefMeasure, LandingLightsAfterAGoAround) {
    // Lights off on an abandoned approach don't count: the window starts at the go-around (00:46:00), and the lights
    // came on at 00:46:38.
    const auto m = measureFlight(with(fixture(), "2026-11-25T00:46:00.000Z", "phase_changed", {{"phase", "go_around"}}),
                                 false);
    EXPECT_NEAR(*value(m, "landing_lights_off_s"), 38, 1);
}

TEST(DebriefMeasure, TakeoffLightsHaveAGrace) {
    // Landing lights switched on 5 s into the takeoff roll (00:05:11) still count as on for takeoff.
    auto lines = fixture();
    nlohmann::json late;
    for (auto& l : lines) {
        if (l.type == "cockpit" && formatUtc(l.ts).starts_with("2026-11-25T00:04:25")) {
            late = l.data;
            l.data["landing_lights"] = false;
        }
    }
    ASSERT_FALSE(late.is_null());
    const auto at = *parseUtc("2026-11-25T00:05:16.000Z");
    lines.insert(std::find_if(lines.begin(), lines.end(), [&](const LogLine& l) { return l.ts > at; }),
                 LogLine{at, "cockpit", late});
    EXPECT_EQ(*value(measureFlight(lines, false), "takeoff_landing_lights"), 1);
}

TEST(DebriefMeasure, PitchIsStoredAsTheSimGivesIt) {
    auto lines = fixture();
    for (auto& l : lines)
        if (l.type == "touchdown") l.data["sim_pitch_deg"] = -5.06;
    const auto m = measureFlight(lines, false);
    ASSERT_TRUE(m.landing);
    EXPECT_EQ(m.landing->simPitchTenths, -51);  // raw: the sign is the sim's (D-158, checked in the sim)
}

TEST(DebriefMeasure, ApuSwitchedOffLate) {
    // The APU left on until 00:13:00, 1 min 55 s after the transition altitude (00:11:04.853).
    auto lines = fixture();
    for (auto& l : lines) {
        if (l.type == "cockpit" && formatUtc(l.ts) >= "2026-11-24T23:58" && formatUtc(l.ts) < "2026-11-25T00:13") {
            l.data["apu_switch"] = true;
            l.data["apu_running"] = true;
        }
    }
    nlohmann::json off;
    for (const auto& l : lines)
        if (l.type == "cockpit" && formatUtc(l.ts) < "2026-11-25T00:13") off = l.data;
    off["apu_switch"] = false;
    off["apu_running"] = false;
    const auto at = *parseUtc("2026-11-25T00:13:00.000Z");
    lines.insert(std::find_if(lines.begin(), lines.end(), [&](const LogLine& l) { return l.ts > at; }),
                 LogLine{at, "cockpit", off});
    EXPECT_NEAR(*value(measureFlight(lines, false), "apu_on_after_transition_s"), 115, 1);
}

TEST(DebriefMeasure, ApuOnAfterParkingButBeforeShutdown) {
    // Dave sometimes switches the APU on only once parked: the brake was set at 00:52:48 and the last engine stopped
    // at 00:53:05, so on at 00:52:55 counts.
    auto lines = fixture();
    nlohmann::json on;
    for (auto& l : lines) {
        if (l.type != "cockpit" || formatUtc(l.ts) < "2026-11-25T00:46") continue;
        l.data["apu_switch"] = false;
        l.data["apu_running"] = false;
        on = l.data;
    }
    on["apu_switch"] = true;
    const auto at = *parseUtc("2026-11-25T00:52:55.000Z");
    lines.insert(std::find_if(lines.begin(), lines.end(), [&](const LogLine& l) { return l.ts > at; }),
                 LogLine{at, "cockpit", on});
    EXPECT_EQ(*value(measureFlight(lines, false), "apu_on_at_shutdown"), 1);
}

TEST(DebriefMeasure, ApuOffAtShutdown) {
    auto lines = fixture();
    for (auto& l : lines)
        if (l.type == "cockpit" && formatUtc(l.ts) >= "2026-11-25T00:46") l.data["apu_switch"] = false;
    EXPECT_EQ(*value(measureFlight(lines, false), "apu_on_at_shutdown"), 0);
}

// Review 1.6: NaN is logged as null, and a field can come with the wrong type. Every field the measurement reads is
// made null, or a string, on a copy of the real flight: it still measures, with nothing thrown.
TEST(DebriefMeasure, NullAndWrongTypedFieldsDontThrow) {
    for (const nlohmann::json& bad : {nlohmann::json(nullptr), nlohmann::json("x")}) {
        std::vector<LogLine> lines = fixture();
        for (LogLine& l : lines) {
            if (!l.data.is_object()) continue;
            for (auto& [key, v] : l.data.items()) {
                if (key != "flight_id" && key != "time") v = bad;
            }
        }
        FlightMeasurements m;
        EXPECT_NO_THROW(m = measureFlight(lines, false)) << bad.dump();
        expectEveryPartOnce(m);
        EXPECT_FALSE(m.landing) << "a touchdown without vs_fpm and g isn't a 0 fpm landing";
    }
}

TEST(DebriefMeasure, LandingFactsFromNullFields) {
    const nlohmann::json touchdown = {{"vs_fpm", nullptr}, {"g", "1.2"}, {"sim_pitch_deg", nullptr},
                                      {"bank_deg", nullptr}, {"ias_kt", 131.0}};
    LandingFacts f;
    EXPECT_NO_THROW(f = landingFacts(touchdown, 0, std::nullopt, nullptr));
    EXPECT_EQ(f.vsFpm, 0);
    EXPECT_EQ(f.gHundredths, 0);
    EXPECT_EQ(f.iasKt, 131);
    EXPECT_FALSE(f.simPitchTenths);
    EXPECT_FALSE(f.bankTenths);
}
