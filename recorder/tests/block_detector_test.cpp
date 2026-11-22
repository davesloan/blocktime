// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

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
