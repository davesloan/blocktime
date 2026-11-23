// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <numbers>
#include <optional>

#include <gtest/gtest.h>

#include "block_detector.h"
#include "flight_match.h"

using namespace blocktime;

namespace {

constexpr double kMetersPerDegLat = 6371000.0 * std::numbers::pi / 180.0;  // as distanceMeters()
constexpr double kKbfiLat = 47.53, kKbfiLon = -122.302;

// The BBJ2 delivery flight's next leg: N814SX at KBFI with 5,000 lb on record.
FlightTarget bbj2() {
    FlightTarget t;
    t.plannedFlightId = 42;
    t.plannedAt = "2026-11-23T10:00:00Z";
    t.simFolder = "PMDG 737-800BBJ2";
    t.typeCode = "737-800 BBJ2";
    t.registration = "N814SX";
    t.originIcao = "KBFI";
    t.originLat = kKbfiLat;
    t.originLon = kKbfiLon;
    t.fuelOnRecordLb = 5000;
    return t;
}

// In the BBJ2 at KBFI's reference point with 6,000 lb.
BlockSample atKbfi() {
    BlockSample s;
    s.parkingBrake = false;
    s.onGround = true;
    s.latDeg = kKbfiLat;
    s.lonDeg = kKbfiLon;
    s.fuelLb = 6000;
    s.aircraftFolder = "pmdg 737-800bbj2";
    return s;
}

BlockSample north(BlockSample s, double m) {
    s.latDeg += m / kMetersPerDegLat;
    return s;
}

}  // namespace

TEST(Match, FolderFromBothPathForms) {
    EXPECT_EQ(aircraftFolder(R"(SimObjects\Airplanes\PMDG 737-800BBJ2\aircraft.CFG)"), "PMDG 737-800BBJ2");
    EXPECT_EQ(aircraftFolder(R"(i:\flightsim\community\pmdg-aircraft-738\simobjects\airplanes\pmdg 737-800bbj2\aircraft.cfg)"),
              "pmdg 737-800bbj2");
    EXPECT_EQ(aircraftFolder("simobjects/airplanes/pmdg 777f/aircraft.cfg"), "pmdg 777f");
    EXPECT_EQ(aircraftFolder(""), "");
}

TEST(Match, FolderMatchRule) {
    EXPECT_TRUE(folderMatches("pmdg 737-800bbj2", "PMDG 737-800BBJ2"));
    EXPECT_TRUE(folderMatches("PMDG 777-200ER GE", "PMDG 777-200ER"));
    EXPECT_TRUE(folderMatches("PMDG 737-800", "PMDG 737-800"));
    EXPECT_FALSE(folderMatches("PMDG 737-800BBJ2", "PMDG 737-800"));
    EXPECT_FALSE(folderMatches("PMDG 737-800", "PMDG 737-800BBJ2"));
    EXPECT_FALSE(folderMatches("", "PMDG 737-800"));
    EXPECT_FALSE(folderMatches("PMDG 737-800", ""));
}

TEST(Match, NoTarget) {
    const MatchResult r = checkMatch(std::nullopt, atKbfi());
    EXPECT_EQ(r.problem, MatchProblem::NoPlannedFlight);
    EXPECT_EQ(r.plannedFlightId, 0);
    EXPECT_EQ(r.aircraftFolder, "pmdg 737-800bbj2");
    EXPECT_EQ(problemCode(r.problem), "no_planned_flight");
}

TEST(Match, Matches) {
    const MatchResult r = checkMatch(bbj2(), atKbfi());
    EXPECT_EQ(r.problem, MatchProblem::None);
    EXPECT_EQ(r.plannedFlightId, 42);
    EXPECT_EQ(problemCode(r.problem), "");
}

TEST(Match, WrongType) {
    BlockSample s = atKbfi();
    s.aircraftFolder = "pmdg 737-800";
    const MatchResult r = checkMatch(bbj2(), s);
    EXPECT_EQ(r.problem, MatchProblem::WrongAircraftType);
    EXPECT_EQ(r.plannedFlightId, 42);
    EXPECT_EQ(problemCode(r.problem), "wrong_aircraft_type");
    s.aircraftFolder = "";  // not known yet
    EXPECT_EQ(checkMatch(bbj2(), s).problem, MatchProblem::WrongAircraftType);
}

TEST(Match, WrongAirport) {
    MatchResult r = checkMatch(bbj2(), north(atKbfi(), 3500));
    EXPECT_EQ(r.problem, MatchProblem::WrongAirport);
    EXPECT_NEAR(r.distanceM, 3500, 1);
    EXPECT_EQ(problemCode(r.problem), "wrong_airport");
    r = checkMatch(bbj2(), north(atKbfi(), 2500));
    EXPECT_EQ(r.problem, MatchProblem::None);
    EXPECT_NEAR(r.distanceM, 2500, 1);
}

TEST(Match, FuelSlack) {
    BlockSample s = atKbfi();
    s.fuelLb = 4700;
    MatchResult r = checkMatch(bbj2(), s);
    EXPECT_EQ(r.problem, MatchProblem::None);
    EXPECT_EQ(r.minFuelLb, 4700);
    s.fuelLb = 4699;
    r = checkMatch(bbj2(), s);
    EXPECT_EQ(r.problem, MatchProblem::FuelBelowRecord);
    EXPECT_EQ(r.fuelLb, 4699);
    EXPECT_EQ(r.minFuelLb, 4700);
    EXPECT_EQ(problemCode(r.problem), "fuel_below_record");

    FlightTarget delivery = bbj2();
    delivery.fuelOnRecordLb = 0;
    s.fuelLb = 0;
    r = checkMatch(delivery, s);
    EXPECT_EQ(r.problem, MatchProblem::None);
    EXPECT_EQ(r.minFuelLb, 0);
}

TEST(Match, TypeIsCheckedBeforeAirport) {
    BlockSample s = north(atKbfi(), 10000);
    s.aircraftFolder = "pmdg 777-300er";
    s.fuelLb = 0;
    EXPECT_EQ(checkMatch(bbj2(), s).problem, MatchProblem::WrongAircraftType);
}

TEST(Match, SlotHandsTheTargetOver) {
    FlightTargetSlot slot;
    EXPECT_FALSE(slot.get().has_value());
    slot.set(bbj2());
    ASSERT_TRUE(slot.get().has_value());
    EXPECT_EQ(slot.get()->registration, "N814SX");
    slot.set(std::nullopt);
    EXPECT_FALSE(slot.get().has_value());
}
