// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <chrono>
#include <cstring>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "block_detector.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const TimePoint kT0 = sys_days{2026y / 11 / 22} + 14h;
// Same Earth radius as distanceMeters(), so north(m) moves m metres.
constexpr double kMetersPerDegLat = 6371000.0 * std::numbers::pi / 180.0;

// Parked at KSAN gate 32, brake set, engines running, PMDG 737.
BlockSample gate() {
    BlockSample s;
    s.time = kT0;
    s.parkingBrake = true;
    s.onGround = true;
    s.latDeg = 32.7336;
    s.lonDeg = -117.1897;
    s.altAglFt = 8;
    s.fuelLb = 9000;
    s.fuelGal = 1343;
    s.enginesRunning = 2;
    s.atcId = "N738DV";
    s.title = "PMDG 737-800";
    return s;
}

// Drives a detector through a scripted flight. Every helper changes the current sample, then feeds it
// at kT0 + `at`, and collects whatever the detector returns.
struct Script {
    BlockDetector det;
    BlockSample s = gate();
    std::vector<BlockEvent> events;

    void step(seconds at) {
        s.time = kT0 + at;
        if (auto e = det.onSample(s)) events.push_back(*e);
    }
    void brake(bool set, seconds at) {
        s.parkingBrake = set;
        step(at);
    }
    void north(double m, seconds at) {
        s.latDeg += m / kMetersPerDegLat;
        step(at);
    }
    void engines(int n, seconds at) {
        s.enginesRunning = n;
        step(at);
    }
    void fuel(double lb) { s.fuelLb = lb; }
    void airborne(seconds at) {
        s.onGround = false;
        s.altAglFt = 3000;
        step(at);
    }
    void landed(seconds at) {
        s.onGround = true;
        s.altAglFt = 8;
        step(at);
    }
    // Set, release at +5 s, move 20 m at +40 s: block out at kT0 + 5 s.
    void blockOut() {
        brake(true, 0s);
        brake(false, 5s);
        north(20, 40s);
    }
    // blockOut(), then take off and land, ending on the ground with the brake off and engines running.
    void flyAndLand() {
        blockOut();
        airborne(600s);
        landed(3600s);
        north(500, 3700s);
    }
};

template <class T>
const T& only(const std::vector<BlockEvent>& events) {
    EXPECT_EQ(events.size(), 1u);
    return std::get<T>(events.at(0));
}

}  // namespace

// --- Block out (D-031) -------------------------------------------------------

TEST(BlockOut, ReleaseWithoutMovementDoesNothing) {
    Script f;
    f.brake(true, 0s);
    f.brake(false, 5s);
    f.north(9, 30s);
    EXPECT_TRUE(f.events.empty());
    EXPECT_FALSE(f.det.flightOpen());
}

TEST(BlockOut, TimedAtReleaseWithReleaseFuelAndPosition) {
    Script f;
    f.brake(true, 0s);
    f.brake(false, 5s);
    const double relLat = f.s.latDeg, relLon = f.s.lonDeg;
    f.fuel(8990);
    f.north(10.001, 40s);  // just over the 10 m threshold

    const auto& out = only<BlockOut>(f.events);
    EXPECT_EQ(out.time, kT0 + 5s);
    EXPECT_EQ(out.fuelLb, 9000);
    EXPECT_EQ(out.fuelGal, 1343);
    EXPECT_EQ(out.latDeg, relLat);
    EXPECT_EQ(out.lonDeg, relLon);
    EXPECT_GE(out.movedM, 10.0);
    EXPECT_EQ(out.flightId, "2026-11-22T14:00:05.000Z");
    EXPECT_EQ(out.atcId, "N738DV");
    EXPECT_EQ(out.title, "PMDG 737-800");
    EXPECT_TRUE(f.det.flightOpen());
}

TEST(BlockOut, GsxSequenceGivesOneBlockOutAtPushRelease) {
    Script f;
    f.brake(true, 0s);     // startup profile
    f.brake(false, 10s);
    f.brake(true, 20s);    // set for GSX
    f.brake(false, 60s);   // release for the push
    f.north(40, 120s);     // pushed back
    f.brake(true, 180s);   // set after the push
    f.brake(false, 240s);  // release to taxi
    f.north(500, 300s);
    f.brake(true, 400s);   // hold short
    f.brake(false, 460s);

    const auto& out = only<BlockOut>(f.events);
    EXPECT_EQ(out.time, kT0 + 60s);
}

TEST(BlockOut, SetBeforeTenMetersDropsCandidate) {
    Script f;
    f.brake(true, 0s);
    f.brake(false, 5s);
    f.north(5, 10s);
    f.brake(true, 15s);
    f.north(15, 20s);  // moving with the brake set (e.g. sliding) is not a release
    EXPECT_TRUE(f.events.empty());
    EXPECT_FALSE(f.det.flightOpen());
}

TEST(BlockOut, MovementIsMeasuredFromTheReleasePoint) {
    Script f;
    f.brake(true, 0s);
    f.brake(false, 5s);
    f.north(6, 10s);
    f.brake(true, 15s);
    f.brake(false, 20s);  // new candidate here, 6 m from the first release
    f.north(6, 25s);      // 12 m from the first release, 6 m from this one
    EXPECT_TRUE(f.events.empty());
    f.north(5, 30s);      // 11 m from this one
    EXPECT_EQ(only<BlockOut>(f.events).time, kT0 + 20s);
}

TEST(BlockOut, FirstSampleWithBrakeOffIsNotARelease) {
    Script f;
    f.brake(false, 0s);
    f.north(100, 30s);
    EXPECT_TRUE(f.events.empty());
    EXPECT_FALSE(f.det.flightOpen());
}

TEST(BlockOut, ReleaseWhileAirborneIsIgnored) {
    Script f;
    f.s.onGround = false;
    f.s.altAglFt = 3000;
    f.brake(true, 0s);
    f.brake(false, 5s);
    f.north(1000, 30s);
    EXPECT_TRUE(f.events.empty());
}

TEST(BlockOut, ToBlockSampleCopiesFieldsAndCountsRunningEngines) {
    TelemetrySample t{};
    t.parkingBrake = 1;
    t.onGround = 1;
    t.latitudeDeg = 32.7;
    t.longitudeDeg = -117.2;
    t.altitudeAglFt = 8.1;
    t.groundSpeedKt = 3.5;
    t.fuelTotalLb = 9000;
    t.fuelTotalGal = 1343;
    t.numEngines = 2;
    t.engCombustion[0] = 0;
    t.engCombustion[1] = 1;
    t.engCombustion[2] = 1;  // beyond NUMBER OF ENGINES: not counted
    std::memcpy(t.title, "PMDG 737-800", sizeof("PMDG 737-800"));
    std::memcpy(t.atcId, "N738DV", sizeof("N738DV"));

    const BlockSample s = toBlockSample(t, kT0);
    EXPECT_EQ(s.time, kT0);
    EXPECT_TRUE(s.parkingBrake);
    EXPECT_TRUE(s.onGround);
    EXPECT_EQ(s.latDeg, 32.7);
    EXPECT_EQ(s.lonDeg, -117.2);
    EXPECT_EQ(s.altAglFt, 8.1);
    EXPECT_EQ(s.groundSpeedKt, 3.5);
    EXPECT_EQ(s.fuelLb, 9000);
    EXPECT_EQ(s.fuelGal, 1343);
    EXPECT_EQ(s.enginesRunning, 1);
    EXPECT_EQ(s.title, "PMDG 737-800");
    EXPECT_EQ(s.atcId, "N738DV");
}

// --- Block in (D-032) --------------------------------------------------------

TEST(BlockIn, TimedAtSetWithShutdownFuel) {
    Script f;
    f.flyAndLand();
    f.fuel(5000);
    f.brake(true, 4000s);
    f.engines(1, 4060s);
    f.fuel(4990);
    f.engines(0, 4090s);

    ASSERT_EQ(f.events.size(), 2u);
    const auto& out = std::get<BlockOut>(f.events[0]);
    const auto& in = std::get<BlockIn>(f.events[1]);
    EXPECT_EQ(in.flightId, out.flightId);
    EXPECT_EQ(in.time, kT0 + 4000s);
    EXPECT_EQ(in.fuelLb, 5000);
    EXPECT_EQ(in.fuelGal, 1343);
    EXPECT_EQ(in.latDeg, f.s.latDeg);
    EXPECT_EQ(in.lonDeg, f.s.lonDeg);
    EXPECT_EQ(in.shutdownTime, kT0 + 4090s);
    EXPECT_EQ(in.shutdownFuelLb, 4990);
    EXPECT_EQ(in.blockTimeS, 4000 - 5);
    EXPECT_FALSE(f.det.flightOpen());
}

TEST(BlockIn, SetReleasedWithEngineRunningIsIgnored) {
    Script f;
    f.flyAndLand();
    f.brake(true, 3800s);   // taxiway stop
    f.engines(1, 3850s);    // single-engine taxi
    f.brake(false, 3900s);
    f.north(300, 4000s);
    f.brake(true, 4100s);   // gate
    f.engines(0, 4150s);

    ASSERT_EQ(f.events.size(), 2u);
    EXPECT_EQ(std::get<BlockIn>(f.events[1]).time, kT0 + 4100s);
}

TEST(BlockIn, EnginesAlreadyOffAtSet) {
    Script f;
    f.flyAndLand();
    f.engines(0, 3800s);
    f.brake(true, 3900s);

    ASSERT_EQ(f.events.size(), 2u);
    const auto& in = std::get<BlockIn>(f.events[1]);
    EXPECT_EQ(in.time, kT0 + 3900s);
    EXPECT_EQ(in.shutdownTime, kT0 + 3900s);
}

TEST(BlockIn, GroundFlickerIsNotALanding) {
    Script f;
    f.blockOut();
    f.s.onGround = false;  // one bad sample during taxi, still at ground AGL
    f.step(100s);
    f.s.onGround = true;
    f.north(100, 110s);
    f.brake(true, 200s);
    f.engines(0, 260s);

    ASSERT_EQ(f.events.size(), 1u);
    EXPECT_TRUE(f.det.flightOpen());
}

TEST(BlockIn, ReturnToGateWithoutFlyingStaysOpen) {
    Script f;
    f.blockOut();
    f.brake(true, 300s);
    f.engines(0, 360s);

    ASSERT_EQ(f.events.size(), 1u);
    EXPECT_TRUE(f.det.flightOpen());
}

TEST(BlockIn, SetWhileAirborneDoesNotCount) {
    Script f;
    f.blockOut();
    f.airborne(600s);
    f.brake(true, 700s);  // brake set in the air, still set on landing
    f.landed(3600s);
    f.engines(0, 3700s);

    ASSERT_EQ(f.events.size(), 1u);
    EXPECT_TRUE(f.det.flightOpen());
}

TEST(BlockIn, NewFlightAfterBlockIn) {
    Script f;
    f.flyAndLand();
    f.brake(true, 4000s);
    f.engines(0, 4090s);
    f.engines(2, 7000s);
    f.brake(false, 7200s);
    f.north(20, 7260s);

    ASSERT_EQ(f.events.size(), 3u);
    const auto& first = std::get<BlockOut>(f.events[0]);
    const auto& second = std::get<BlockOut>(f.events[2]);
    EXPECT_EQ(second.time, kT0 + 7200s);
    EXPECT_NE(second.flightId, first.flightId);
    EXPECT_TRUE(f.det.flightOpen());
}

// --- Session end (D-044) -----------------------------------------------------

TEST(Session, EndWithOpenFlightReportsInterrupted) {
    Script f;
    f.blockOut();
    const auto end = f.det.onSessionEnded("sim_quit");

    ASSERT_TRUE(end.has_value());
    EXPECT_EQ(end->flightId, std::get<BlockOut>(f.events.at(0)).flightId);
    EXPECT_EQ(end->reason, "sim_quit");
    EXPECT_FALSE(f.det.flightOpen());
}

TEST(Session, EndWithoutFlightReportsNothing) {
    Script f;
    f.brake(true, 0s);
    EXPECT_FALSE(f.det.onSessionEnded("recorder_exit").has_value());
}

TEST(Session, CandidateAndBrakeStateDoNotSurvive) {
    Script f;
    f.brake(true, 0s);
    f.brake(false, 5s);  // candidate
    EXPECT_FALSE(f.det.onSessionEnded("sim_process_gone").has_value());
    f.north(50, 300s);   // first sample of the new session: brake off, 50 m away
    f.north(100, 330s);
    EXPECT_TRUE(f.events.empty());
    EXPECT_FALSE(f.det.flightOpen());
}

TEST(Session, LandedStateDoesNotSurvive) {
    Script f;
    f.flyAndLand();
    f.det.onSessionEnded("sim_quit");
    f.brake(true, 5000s);   // new session at a gate: baseline only
    f.brake(false, 5100s);
    f.north(20, 5160s);     // a fresh block out
    f.brake(true, 5200s);
    f.engines(0, 5260s);    // no landing in this flight: no block in
    ASSERT_EQ(f.events.size(), 2u);
    EXPECT_EQ(std::get<BlockOut>(f.events[1]).time, kT0 + 5100s);
    EXPECT_TRUE(f.det.flightOpen());
}

// --- Log format (D-043) ------------------------------------------------------

namespace {
std::vector<std::string> keys(const nlohmann::json& j) {
    std::vector<std::string> k;
    for (const auto& [key, value] : j.items()) k.push_back(key);
    std::sort(k.begin(), k.end());
    return k;
}
}  // namespace

TEST(Json, BlockOutFields) {
    Script f;
    f.blockOut();
    const BlockEvent& e = f.events.at(0);
    const nlohmann::json j = toJson(e);

    EXPECT_EQ(eventType(e), "block_out");
    EXPECT_EQ(keys(j), (std::vector<std::string>{"atc_id", "flight_id", "fuel_gal", "fuel_lb", "lat", "lon",
                                                 "moved_m", "time", "title"}));
    EXPECT_EQ(j["flight_id"], "2026-11-22T14:00:05.000Z");
    EXPECT_EQ(j["time"], "2026-11-22T14:00:05.000Z");
    EXPECT_EQ(j["fuel_lb"], 9000.0);
    EXPECT_EQ(j["atc_id"], "N738DV");
}

TEST(Json, BlockInFields) {
    Script f;
    f.flyAndLand();
    f.brake(true, 4000s);
    f.engines(0, 4090s);
    const BlockEvent& e = f.events.at(1);
    const nlohmann::json j = toJson(e);

    EXPECT_EQ(eventType(e), "block_in");
    EXPECT_EQ(keys(j), (std::vector<std::string>{"block_time_s", "flight_id", "fuel_gal", "fuel_lb", "lat", "lon",
                                                 "shutdown_fuel_lb", "shutdown_time", "time"}));
    EXPECT_EQ(j["time"], "2026-11-22T15:06:40.000Z");
    EXPECT_EQ(j["shutdown_time"], "2026-11-22T15:08:10.000Z");
    EXPECT_TRUE(j["block_time_s"].is_number_integer());
    EXPECT_EQ(j["block_time_s"], 3995);
}

TEST(Json, InterruptedFields) {
    const BlockEvent e = FlightInterrupted{"2026-11-22T14:00:05.000Z", "sim_quit"};
    const nlohmann::json j = toJson(e);

    EXPECT_EQ(eventType(e), "flight_interrupted");
    EXPECT_EQ(keys(j), (std::vector<std::string>{"flight_id", "reason"}));
    EXPECT_EQ(j["reason"], "sim_quit");
}
