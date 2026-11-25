// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// FlightTracker: one flight's phases, runways, touchdown and altimeter as log events (Task 5 of milestone 7).

#include <chrono>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "flight_tracker.h"
#include "replay.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const TimePoint kT0 = sys_days{2026y / 11 / 23} + 12h;

AirportFacilities airport(const std::string& icao) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/runways-") + icao + ".json");
    AirportRunways r = airportRunwaysFromJson(nlohmann::json::parse(in));
    return AirportFacilities{r.icao, 0, 0, std::move(r.runways), {}};
}

FlightTarget target() {
    FlightTarget t;
    t.plannedFlightId = 1;
    t.originIcao = "KBFI";
    t.destinationIcao = "KPDX";
    t.destinationLat = 45.5887;
    t.destinationLon = -122.5975;
    t.cruiseAltitudeFt = 20000;
    t.transitionAltitudeFt = 18000;
    t.transitionLevelFt = 18000;
    return t;
}

BlockOut blockOut(TimePoint t = kT0) {
    BlockOut b;
    b.flightId = formatUtc(t);
    b.time = t;
    return b;
}

const LogEvent* find(const std::vector<LogEvent>& events, const std::string& type) {
    for (const auto& e : events)
        if (e.type == type) return &e;
    return nullptr;
}

void append(std::vector<LogEvent>& to, const std::vector<LogEvent>& from) { to.insert(to.end(), from.begin(), from.end()); }

// Runs the KBFI→KPDX replay through a tracker with both airports' runways.
std::vector<LogEvent> replayKbfiKpdx(FlightTracker& t) {
    const auto r = replay::loadReplay("replay-kbfi-kpdx.csv");
    t.setAirport(airport("KBFI"));
    t.setAirport(airport("KPDX"));
    std::vector<LogEvent> all = t.onBlockOut(blockOut(r.blockOut), target(), 25);
    for (const auto& s : r.samples) append(all, t.onSample(s));
    return all;
}

PhaseSample airborne(int sec, double altFt, double aglFt, double vsFpm = 0) {
    PhaseSample s;
    s.time = kT0 + seconds(sec);
    s.activeS = sec;
    s.altFt = altFt;
    s.aglFt = aglFt;
    s.vsFpm = vsFpm;
    s.groundSpeedKt = 140;
    return s;
}

AltimeterSample baro(double mb, bool std = false) { return AltimeterSample{mb, mb, std, std}; }

}  // namespace

TEST(FlightTracker, BlockOutStartsPushbackAndLogsTheAltimeter) {
    FlightTracker t;
    EXPECT_TRUE(t.onAltimeter(baro(1016.6)).empty());  // no flight yet
    const auto events = t.onBlockOut(blockOut(), target(), 25);
    const LogEvent* phase = find(events, "phase_changed");
    ASSERT_TRUE(phase);
    EXPECT_EQ(phase->data["phase"], "pushback");
    EXPECT_EQ(phase->data["previous"], "at_gate");
    EXPECT_EQ(phase->data["time"], formatUtc(kT0));
    const LogEvent* alt = find(events, "altimeter");
    ASSERT_TRUE(alt);
    EXPECT_DOUBLE_EQ(alt->data["capt_mb"].get<double>(), 1016.6);
    EXPECT_EQ(alt->data["capt_std"], false);
    EXPECT_TRUE(t.flightOpen());
}

TEST(FlightTracker, AltimeterOnlyOnChangeWhileOpen) {
    FlightTracker t;
    t.onBlockOut(blockOut(), target(), 25);
    EXPECT_EQ(t.onAltimeter(baro(1013.2)).size(), 1u);
    EXPECT_TRUE(t.onAltimeter(baro(1013.2)).empty());
    EXPECT_TRUE(t.onAltimeter(baro(1013.21)).empty());  // logged to 0.1 hPa
    EXPECT_EQ(t.onAltimeter(baro(1013.3)).size(), 1u);
    EXPECT_EQ(t.onAltimeter(baro(1013.3, true)).size(), 1u);
}

TEST(FlightTracker, LiftoffNamesTheRunway) {
    FlightTracker t;
    const auto events = replayKbfiKpdx(t);
    const LogEvent* liftoff = find(events, "liftoff");
    ASSERT_TRUE(liftoff);
    EXPECT_EQ(liftoff->data["airport"], "KBFI");
    EXPECT_EQ(liftoff->data["runway"], "14R");
    EXPECT_EQ(liftoff->data["time"].get<std::string>().substr(11, 8), "22:56:32");
    EXPECT_LT(liftoff->data["off_centreline_m"].get<double>(), 10.0);
}

TEST(FlightTracker, LandingRunwayAtTaxiIn) {
    FlightTracker t;
    const auto events = replayKbfiKpdx(t);
    const LogEvent* landing = find(events, "landing_runway");
    ASSERT_TRUE(landing);
    EXPECT_EQ(landing->data["airport"], "KPDX");
    EXPECT_EQ(landing->data["runway"], "10R");
}

TEST(FlightTracker, FastSamplingWindow) {
    FlightTracker t;
    t.onBlockOut(blockOut(), target(), 25);
    // Ground roll and climb out.
    PhaseSample g;
    g.onGround = true;
    g.aglFt = 8;
    for (int i = 0; i < 12; ++i) {
        g.time = kT0 + seconds(i);
        g.activeS = i;
        g.groundSpeedKt = 6 + i * 5;
        t.onSample(g);
    }
    t.onSample(airborne(12, 230, 200, 1500));
    EXPECT_FALSE(t.wantsFastSampling());  // Takeoff, not on the way down
    t.onSample(airborne(13, 1500, 1500, 1500));
    for (int sec = 14; sec <= 80; ++sec) t.onSample(airborne(sec, 9000 - (sec - 14) * 25, 9000 - (sec - 14) * 25, -1500));
    t.onSample(airborne(81, 2500, 2400, -800));  // approach: under 3,000 ft AGL is near enough
    ASSERT_FALSE(t.wantsFastSampling());
    t.onSample(airborne(82, 250, 250, -700));
    EXPECT_TRUE(t.wantsFastSampling());
    // A go-around climbs back above 500 ft.
    t.onSample(airborne(90, 600, 600, 1500));
    EXPECT_FALSE(t.wantsFastSampling());
    t.onSample(airborne(120, 280, 280, -700));
    ASSERT_TRUE(t.wantsFastSampling());
    // The touchdown, from sim frames.
    FastSample f;
    f.time = kT0 + 120s;
    f.vsFpm = -150;
    EXPECT_TRUE(t.onFastSample(f).empty());
    f.onGround = true;
    f.time = kT0 + 121s;
    EXPECT_TRUE(t.onFastSample(f).empty());
    f.time = kT0 + 122s;
    const auto events = t.onFastSample(f);
    const LogEvent* td = find(events, "touchdown");
    ASSERT_TRUE(td);
    EXPECT_DOUBLE_EQ(td->data["vs_fpm"].get<double>(), -150);
    EXPECT_TRUE(t.wantsFastSampling());  // on for the bounce count (D-158)
    f.time = kT0 + 136s;
    ASSERT_TRUE(find(t.onFastSample(f), "bounces"));
    EXPECT_FALSE(t.wantsFastSampling());
}

namespace {

// Block out to 280 ft on the way down, with fast sampling on; returns the second it got there.
int armForLanding(FlightTracker& t) {
    t.onBlockOut(blockOut(), target(), 25);
    PhaseSample g;
    g.onGround = true;
    g.aglFt = 8;
    for (int i = 0; i < 12; ++i) {
        g.time = kT0 + seconds(i);
        g.activeS = i;
        g.groundSpeedKt = 6 + i * 5;
        t.onSample(g);
    }
    t.onSample(airborne(12, 230, 200, 1500));
    t.onSample(airborne(13, 1500, 1500, 1500));
    for (int sec = 14; sec <= 80; ++sec) t.onSample(airborne(sec, 9000 - (sec - 14) * 25, 9000 - (sec - 14) * 25, -1500));
    t.onSample(airborne(81, 2500, 2400, -800));
    t.onSample(airborne(82, 250, 250, -700));
    return 82;
}

FastSample fast(TimePoint at, bool onGround) {
    FastSample f;
    f.time = at;
    f.onGround = onGround;
    f.vsFpm = onGround ? 0 : -500;
    f.pitchDeg = -5.06;
    f.bankDeg = 0.84;
    f.headingTrueDeg = 286.4;
    f.windDirDeg = 339.6;
    f.windKt = 9.4;
    return f;
}

}  // namespace

TEST(FlightTracker, TouchdownCarriesAttitudeAndWind) {
    FlightTracker t;
    const int s0 = armForLanding(t);
    ASSERT_TRUE(t.wantsFastSampling());
    t.onFastSample(fast(kT0 + seconds(s0), false));
    t.onFastSample(fast(kT0 + seconds(s0 + 1), true));
    const LogEvent* td = nullptr;
    std::vector<LogEvent> events;
    for (int ms = 1100; ms <= 2000 && !td; ms += 100) {
        events = t.onFastSample(fast(kT0 + seconds(s0) + milliseconds(ms), true));
        td = find(events, "touchdown");
    }
    ASSERT_TRUE(td);
    EXPECT_DOUBLE_EQ(td->data["sim_pitch_deg"].get<double>(), -5.1);
    EXPECT_DOUBLE_EQ(td->data["bank_deg"].get<double>(), 0.8);
    EXPECT_EQ(td->data["heading_true_deg"], 286);
    EXPECT_EQ(td->data["wind_dir_deg"], 340);
    EXPECT_EQ(td->data["wind_kt"], 9);
}

TEST(FlightTracker, BouncesEventAfterTouchdown) {
    FlightTracker t;
    const int s0 = armForLanding(t);
    const TimePoint c = kT0 + seconds(s0 + 1);  // contact
    t.onFastSample(fast(c - 100ms, false));
    std::vector<LogEvent> all;
    for (int ms = 0; ms < 200; ms += 33) append(all, t.onFastSample(fast(c + milliseconds(ms), true)));
    for (int ms = 200; ms < 900; ms += 33) append(all, t.onFastSample(fast(c + milliseconds(ms), false)));
    for (int ms = 900; ms < 15100; ms += 100) append(all, t.onFastSample(fast(c + milliseconds(ms), true)));
    const LogEvent* b = find(all, "bounces");
    ASSERT_TRUE(b);
    EXPECT_EQ(b->data["bounces"], 1);
    EXPECT_EQ(b->data["flight_id"], formatUtc(kT0));
    EXPECT_FALSE(t.wantsFastSampling());
}

TEST(FlightTracker, TaxiInEmitsBouncesEarly) {
    FlightTracker t;
    const int s0 = armForLanding(t);
    const TimePoint c = kT0 + seconds(s0 + 1);
    t.onFastSample(fast(c - 100ms, false));
    for (int ms = 0; ms < 1500; ms += 100) t.onFastSample(fast(c + milliseconds(ms), true));
    // A short rollout: on the ground and slow well before 15 s.
    std::vector<LogEvent> all;
    for (int sec = s0 + 2; sec <= s0 + 12; ++sec) {
        PhaseSample g;
        g.time = kT0 + seconds(sec);
        g.activeS = sec;
        g.onGround = true;
        g.aglFt = 8;
        g.groundSpeedKt = sec < s0 + 6 ? 100 : 20;
        append(all, t.onSample(g));
    }
    ASSERT_TRUE(find(all, "phase_changed"));
    const LogEvent* b = find(all, "bounces");
    ASSERT_TRUE(b);
    EXPECT_EQ(b->data["bounces"], 0);
    EXPECT_FALSE(t.wantsFastSampling());
}

TEST(FlightTracker, FlightLevelFlag) {
    FlightTracker t;
    const auto events = replayKbfiKpdx(t);
    bool sawCruise = false, sawLowLevel = false;
    for (const auto& e : events) {
        if (e.type == "phase_changed" && e.data["phase"] == "cruise") {
            sawCruise = true;
            EXPECT_EQ(e.data["flight_level"], true);
        }
        if (e.type == "level_off" && e.data["alt_ft"] == 2000) {
            sawLowLevel = true;
            EXPECT_EQ(e.data["flight_level"], false);
        }
    }
    EXPECT_TRUE(sawCruise);
    EXPECT_TRUE(sawLowLevel);
}

TEST(FlightTracker, InterruptForgetsTheFlight) {
    FlightTracker t;
    t.onBlockOut(blockOut(), target(), 25);
    t.onSample(airborne(5, 9000, 9000, 3000));
    t.onFlightEnded();
    EXPECT_FALSE(t.flightOpen());
    EXPECT_TRUE(t.onSample(airborne(6, 9000, 9000)).empty());
    EXPECT_FALSE(t.wantsFastSampling());
    const auto again = t.onBlockOut(blockOut(kT0 + 1h), target(), 25);
    const LogEvent* phase = find(again, "phase_changed");
    ASSERT_TRUE(phase);
    EXPECT_EQ(phase->data["phase"], "pushback");
    EXPECT_EQ(phase->data["flight_id"], formatUtc(kT0 + 1h));
}

TEST(FlightTracker, BlockInClosesTheFlight) {
    FlightTracker t;
    t.onBlockOut(blockOut(), target(), 25);
    BlockIn in;
    in.flightId = formatUtc(kT0);
    in.time = kT0 + 2h;
    const auto events = t.onBlockIn(in, 27);
    const LogEvent* phase = find(events, "phase_changed");
    ASSERT_TRUE(phase);
    EXPECT_EQ(phase->data["phase"], "arrived");
    EXPECT_FALSE(t.flightOpen());
}

TEST(FlightTracker, EveryEventCarriesTheFlightId) {
    FlightTracker t;
    const auto events = replayKbfiKpdx(t);
    ASSERT_GT(events.size(), 20u);
    const auto r = replay::loadReplay("replay-kbfi-kpdx.csv");
    for (const auto& e : events) EXPECT_EQ(e.data["flight_id"], formatUtc(r.blockOut)) << e.type;
}

TEST(FlightTracker, ParkingAtNamesNearestSpot) {
    FlightTracker t;
    AirportFacilities ksfo = airport("KSFO");
    ParkingSpot d14;
    d14.name = 15;
    d14.number = 14;
    d14.type = 8;
    d14.radiusM = 18;
    placeSpot(d14, 37.6188, -122.3750, 0, 0);
    ksfo.spots.push_back(d14);
    t.setAirport(std::move(ksfo));
    EXPECT_EQ(t.parkingAt(37.6188 + 5 / 111320.0, -122.3750), "Gate D14");
    EXPECT_EQ(t.parkingAt(37.6188 + 100 / 111320.0, -122.3750), "");
}

TEST(FlightTracker, LogsLinedUp) {
    FlightTracker t;
    const auto all = replayKbfiKpdx(t);
    const LogEvent* e = find(all, "lined_up");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->data["runway"], "10R");
    EXPECT_GT(e->data["distance_nm"].get<double>(), 0.0);
    EXPECT_LE(e->data["distance_nm"].get<double>(), 10.0);
    EXPECT_EQ(e->data["flight_id"], formatUtc(replay::loadReplay("replay-kbfi-kpdx.csv").blockOut));
}
