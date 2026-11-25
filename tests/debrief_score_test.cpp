// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief's rules (docs/debrief.md sections 2-4, D-158, D-160): measurements to outcomes, landing stars and the
// 0-100 score.

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "debrief_measure.h"
#include "debrief_score.h"
#include "flight_events.h"

using namespace blocktime;

namespace {

// Every part measured and passing, and a 200 fpm landing with nothing else wrong.
FlightMeasurements clean() {
    FlightMeasurements m;
    m.values = {{"beacon_late_s", 0},        {"belts_off_departure_s", 0},   {"taxi_max_kt", 22},
                {"origin_baro_off_tenths", 1}, {"takeoff_strobes", 1},        {"takeoff_landing_lights", 1},
                {"over_250_s", 0},           {"std_late_s", 12},             {"belts_off_descent_s", 0},
                {"destination_baro_off_tenths", 0}, {"landing_lights_off_s", 0}, {"stable_gear_down", 1},
                {"stable_landing_flaps", 1}, {"stable_sink_fpm", 750},       {"stable_vref_offset_kt", 5},
                {"stable_lined_up", 1},      {"strobes_on_after_landing_s", 40},
                {"apu_on_after_transition_s", 0}, {"apu_on_at_shutdown", 1}};
    LandingFacts f;
    f.vsFpm = 200;
    f.gHundredths = 120;
    f.iasKt = 140;
    f.vrefKt = 140;
    f.headingTrueDeg = 270;
    f.windDirDeg = 270;
    f.windKt = 10;
    f.simPitchTenths = -45;  // the sim's sign: nose up is negative
    f.bankTenths = 10;
    m.landing = f;
    return m;
}

void set(FlightMeasurements& m, const std::string& part, int v) {
    for (auto& x : m.values)
        if (x.part == part) x.value = v;
}

void skipPart(FlightMeasurements& m, const std::string& part, const std::string& reason) {
    std::erase_if(m.values, [&](const CheckValue& v) { return v.part == part; });
    m.skipped.push_back({part, reason});
}

const CheckOutcome& check(const ScoredFlight& s, const std::string& label) {
    for (const auto& c : s.checks)
        if (c.label == label) return c;
    static CheckOutcome none;
    ADD_FAILURE() << "no check " << label;
    return none;
}

int starsFor(int vsFpm, int gHundredths = 120) {
    FlightMeasurements m = clean();
    m.landing->vsFpm = vsFpm;
    m.landing->gHundredths = gHundredths;
    return *scoreFlight(m).stars;
}

}  // namespace

TEST(DebriefScore, CleanFlightScores100) {
    const ScoredFlight s = scoreFlight(clean());
    ASSERT_EQ(s.checks.size(), 14u);
    for (const auto& c : s.checks) EXPECT_EQ(c.state, CheckState::Passed) << c.label;
    EXPECT_EQ(*s.stars, 5);
    EXPECT_EQ(s.score, 100);
    EXPECT_EQ(s.checks.front().label, "Beacon on before pushback");
    EXPECT_EQ(s.checks[11].label, "Strobes off after landing");
    EXPECT_EQ(s.checks[12].label, "APU off by the transition altitude");
    EXPECT_EQ(s.checks.back().label, "APU on at the gate");
}

TEST(DebriefScore, KlasKsanScores62) {
    const auto lines = loadFlightEvents(BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan", "2026-11-24T23:53:16.837Z");
    const ScoredFlight s = scoreFlight(measureFlight(lines, false));
    int missed = 0;
    for (const auto& c : s.checks) missed += c.state == CheckState::Missed;
    EXPECT_EQ(missed, 5);
    EXPECT_EQ(check(s, "Beacon on before pushback").state, CheckState::Missed);
    EXPECT_EQ(check(s, "Beacon on before pushback").detail, "2 min 43 s late");
    EXPECT_EQ(check(s, "Beacon on before pushback").points, -6);
    EXPECT_EQ(check(s, "Seat belt sign, taxi to 10,000 ft").state, CheckState::Missed);
    EXPECT_EQ(check(s, "250 kt below 10,000 ft").state, CheckState::Missed);
    EXPECT_EQ(check(s, "STD by the transition altitude").state, CheckState::Missed);
    EXPECT_EQ(check(s, "STD by the transition altitude").detail, "1 min 51 s after the transition altitude");
    EXPECT_EQ(check(s, "Landing lights below 10,000 ft").state, CheckState::Missed);
    EXPECT_EQ(check(s, "Taxi speed").detail, "25 kt");
    EXPECT_EQ(check(s, "Stable at 1,000 ft").state, CheckState::Passed);
    EXPECT_EQ(*s.stars, 3);
    EXPECT_EQ(s.landingPoints, -8);
    EXPECT_EQ(s.score, 62);
}

TEST(DebriefScore, StarBands) {
    EXPECT_EQ(starsFor(240), 5);
    EXPECT_EQ(starsFor(241), 4);
    EXPECT_EQ(starsFor(360), 4);
    EXPECT_EQ(starsFor(600), 3);
    EXPECT_EQ(starsFor(900), 2);
    EXPECT_EQ(starsFor(1200), 1);
    EXPECT_EQ(starsFor(1201), 0);
}

TEST(DebriefScore, CrosswindGivesAStarBack) {
    FlightMeasurements m = clean();
    m.landing->vsFpm = 700;
    m.landing->windDirDeg = 360;  // 90° off a 270° heading
    m.landing->windKt = 16;
    const ScoredFlight s = scoreFlight(m);
    EXPECT_EQ(*s.crosswindKt, 16);
    EXPECT_EQ(*s.stars, 3);
    m.landing->windKt = 14;
    EXPECT_EQ(*scoreFlight(m).stars, 2);
}

TEST(DebriefScore, GCaps) {
    EXPECT_EQ(starsFor(200, 180), 2);
    EXPECT_EQ(starsFor(200, 200), 1);
    EXPECT_EQ(starsFor(200, 169), 5);
}

TEST(DebriefScore, EachLandingDeduction) {
    FlightMeasurements m = clean();
    m.landing->bounces = 2;
    EXPECT_EQ(*scoreFlight(m).stars, 3);
    m = clean();
    m.landing->bankTenths = -51;
    EXPECT_EQ(*scoreFlight(m).stars, 4);
    m = clean();
    m.landing->simPitchTenths = -81;  // 8.1° nose up: the 737's limit is 8°
    EXPECT_EQ(*scoreFlight(m).stars, 4);
    m.family = Family::B777;
    m.landing->simPitchTenths = -76;  // the 777's is 7.5°
    EXPECT_EQ(*scoreFlight(m).stars, 4);
    m = clean();
    m.landing->simPitchTenths = 1;  // nose first
    EXPECT_EQ(*scoreFlight(m).stars, 4);
    m = clean();
    m.landing->iasKt = 151;
    EXPECT_EQ(*scoreFlight(m).stars, 4);
    m.landing->vrefKt.reset();  // no VREF: no speed rule
    m.landing->simPitchTenths.reset();
    m.landing->bankTenths.reset();
    EXPECT_EQ(*scoreFlight(m).stars, 5);
}

TEST(DebriefScore, DeductionsFloorAtZero) {
    FlightMeasurements m = clean();
    m.landing->vsFpm = 1100;
    m.landing->bounces = 3;
    EXPECT_EQ(*scoreFlight(m).stars, 0);
    EXPECT_EQ(scoreFlight(m).landingPoints, -30);
}

TEST(DebriefScore, NotCheckedCountsNeitherWay) {
    FlightMeasurements m = clean();
    skipPart(m, "belts_off_departure_s", "no_pmdg_data");
    skipPart(m, "origin_baro_off_tenths", "no_atis");
    const ScoredFlight s = scoreFlight(m);
    EXPECT_EQ(check(s, "Seat belt sign, taxi to 10,000 ft").state, CheckState::NotChecked);
    EXPECT_EQ(check(s, "Seat belt sign, taxi to 10,000 ft").detail, "No PMDG data");
    EXPECT_EQ(check(s, "Origin altimeter").detail, "No ATIS logged");
    EXPECT_EQ(check(s, "Origin altimeter").points, 0);
    EXPECT_EQ(s.score, 100);
}

TEST(DebriefScore, StableIsOneCheck) {
    FlightMeasurements m = clean();
    set(m, "stable_sink_fpm", 1100);
    set(m, "stable_vref_offset_kt", 25);
    const ScoredFlight s = scoreFlight(m);
    EXPECT_EQ(check(s, "Stable at 1,000 ft").state, CheckState::Missed);
    EXPECT_EQ(check(s, "Stable at 1,000 ft").detail, "gear down, landing flaps, 1,100 fpm, VREF +25, lined up");
    EXPECT_EQ(s.score, 94);
    skipPart(m, "stable_vref_offset_kt", "no_vref");
    set(m, "stable_sink_fpm", 800);
    EXPECT_EQ(check(scoreFlight(m), "Stable at 1,000 ft").state, CheckState::Passed);
}

TEST(DebriefScore, ScoreFloorsAtZero) {
    FlightMeasurements m = clean();
    for (auto& v : m.values) v.value = 9999;
    m.landing->vsFpm = 2000;
    EXPECT_EQ(scoreFlight(m).score, 0);
}

TEST(DebriefScore, NoLandingNoStars) {
    FlightMeasurements m = clean();
    m.landing.reset();
    const ScoredFlight s = scoreFlight(m);
    EXPECT_FALSE(s.stars);
    EXPECT_EQ(s.landingPoints, 0);
    EXPECT_EQ(s.score, 100);
}

TEST(DebriefScore, AveragesLastTen) {
    std::vector<ScoredFlight> flights;
    for (int i = 0; i < 12; ++i) {
        ScoredFlight s;
        s.score = i < 10 ? 80 : 50;  // newest first: the two oldest scored 50
        s.stars = i < 10 ? 4 : 1;
        flights.push_back(s);
    }
    flights[3].stars.reset();
    const Averages a = averages(flights);
    EXPECT_NEAR(*a.score, (80.0 * 10 + 50 * 2) / 12, 1e-9);
    EXPECT_NEAR(*a.scoreLast10, 80, 1e-9);
    EXPECT_NEAR(*a.stars, (4.0 * 9 + 1 * 2) / 11, 1e-9);
    EXPECT_NEAR(*a.starsLast10, 4, 1e-9);
    EXPECT_FALSE(averages({}).score);
    EXPECT_EQ(starsText(3), "★★★☆☆");
}

TEST(DebriefScore, PitchUpFromTheSimsSign) {
    LandingFacts f;
    f.simPitchTenths = -51;
    EXPECT_EQ(pitchUpTenths(f), 51);
    f.simPitchTenths.reset();
    EXPECT_FALSE(pitchUpTenths(f));
}

TEST(DebriefScore, ApuChecks) {
    FlightMeasurements m = clean();
    set(m, "apu_on_after_transition_s", 115);
    ScoredFlight s = scoreFlight(m);
    EXPECT_EQ(check(s, "APU off by the transition altitude").state, CheckState::Missed);
    EXPECT_EQ(check(s, "APU off by the transition altitude").detail, "1 min 55 s after the transition altitude");
    EXPECT_EQ(check(s, "APU off by the transition altitude").points, -6);
    EXPECT_EQ(s.score, 94);
    m = clean();
    set(m, "apu_on_at_shutdown", 0);
    s = scoreFlight(m);
    EXPECT_EQ(check(s, "APU on at the gate").state, CheckState::Missed);
    EXPECT_EQ(check(s, "APU on at the gate").detail, "Not switched on by engine shutdown");
    EXPECT_EQ(check(s, "APU on at the gate").points, -2);  // Dave: only about 2 points (D-172)
    EXPECT_EQ(s.score, 98);
    EXPECT_EQ(check(scoreFlight(clean()), "APU off by the transition altitude").detail, "Off by the transition altitude");
    EXPECT_EQ(check(scoreFlight(clean()), "APU on at the gate").detail, "Switched on by engine shutdown");
}

TEST(DebriefScore, PartsAddedLaterAreNotChecked) {
    // A flight measured before the APU checks existed has neither part, valued or skipped.
    FlightMeasurements m = clean();
    std::erase_if(m.values, [](const CheckValue& v) { return v.part.starts_with("apu_"); });
    const ScoredFlight s = scoreFlight(m);
    EXPECT_EQ(check(s, "APU on at the gate").state, CheckState::NotChecked);
    EXPECT_EQ(check(s, "APU on at the gate").detail, "Not measured on this flight");
    EXPECT_EQ(s.score, 100);
}
