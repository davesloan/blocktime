// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// planGate / finishSend: whether to ask SayIntentions for BlockTime's gate, and the arrival_gate event (D-141, D-147, D-155).

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "gate_assigner.h"

using namespace blocktime;

namespace {

constexpr double kLat = 36.0, kLon = -115.0, kMPerDeg = 111320.0;

ParkingSpot spot(int name, int number, double northM, double radiusM = 20) {
    ParkingSpot s;
    s.name = name;
    s.number = number;
    s.type = 9;
    s.radiusM = radiusM;
    placeSpot(s, kLat, kLon, 0, northM);
    return s;
}

// Runway 09/27 through the reference point; E7 is 200 m north of it, E9 600 m north.
AirportFacilities klas(std::vector<ParkingSpot> spots = {spot(16, 7, 200), spot(16, 9, 600)}) {
    AirportFacilities a;
    a.icao = "KLAS";
    a.lat = kLat;
    a.lon = kLon;
    a.runways = {Runway{kLat, kLon, 90.0, 3000.0, 45.0, 9, 0, 27, 0}};
    a.spots = std::move(spots);
    return a;
}

SiFlight si(double gateNorthM, std::string gate = "Terminal 3 Gate E9A") {
    SiFlight f;
    f.destination = "KLAS";
    f.assignedGate = std::move(gate);
    f.arrivingRunway = "27";
    f.gateLat = kLat + gateNorthM / kMPerDeg;
    f.gateLon = kLon;
    return f;
}

const GateContext kBbj{35.8, SpotUse::Gate, {}};

}  // namespace

TEST(GateAssigner, SayIntentionsGateKept) {
    const auto p = planGate(si(610), "KLAS", klas(), kBbj);
    ASSERT_TRUE(p.has_value());
    EXPECT_FALSE(p->send);
    EXPECT_EQ(p->event["result"], "kept");
    EXPECT_EQ(p->event["sent"], false);
    EXPECT_EQ(p->event["gate"], "Gate E9");
    EXPECT_EQ(p->event["si_gate"], "Terminal 3 Gate E9A");
    EXPECT_EQ(p->event["icao"], "KLAS");
    EXPECT_EQ(p->event["runway"], "27");
}

TEST(GateAssigner, SayIntentionsGateKeptWhenItMatchesNoSpot) {
    const auto p = planGate(si(2000), "KLAS", klas(), kBbj);
    ASSERT_TRUE(p.has_value());
    EXPECT_FALSE(p->send);
    EXPECT_EQ(p->event["result"], "kept");
    EXPECT_EQ(p->event["gate"], "");
}

TEST(GateAssigner, PreferredGateSent) {
    const GateContext preferE7{35.8, SpotUse::Gate, {"E7"}};
    const auto p = planGate(si(610), "KLAS", klas(), preferE7);
    ASSERT_TRUE(p.has_value());
    EXPECT_TRUE(p->send);
    EXPECT_EQ(p->shortName, "E7");
    EXPECT_EQ(p->event["gate"], "Gate E7");
    EXPECT_EQ(p->event["reason"], "preferred");
}

TEST(GateAssigner, LargerGateSentWhenSayIntentionsIsTooSmall) {
    const auto p = planGate(si(210, "Terminal 3 Gate E7A"), "KLAS", klas({spot(16, 7, 200, 15), spot(16, 9, 600)}),
                            kBbj);
    ASSERT_TRUE(p.has_value());
    EXPECT_TRUE(p->send);
    EXPECT_EQ(p->shortName, "E9");
    EXPECT_EQ(p->event["reason"], "larger");
}

TEST(GateAssigner, WaitsWithoutGate) { EXPECT_FALSE(planGate(si(610, ""), "KLAS", klas(), kBbj).has_value()); }

TEST(GateAssigner, OtherDestinationIgnored) {
    EXPECT_FALSE(planGate(si(610), "KSAN", klas(), kBbj).has_value());
}

TEST(GateAssigner, NoneFits) {
    const auto p = planGate(si(210), "KLAS", klas({spot(16, 7, 200, 15)}), kBbj);
    ASSERT_TRUE(p.has_value());
    EXPECT_FALSE(p->send);
    EXPECT_EQ(p->event["result"], "none_fits");
    EXPECT_EQ(p->event["gate"], "");
}

GatePlan preferE7() {
    return *planGate(si(610), "KLAS", klas(), GateContext{35.8, SpotUse::Gate, {"E7"}});
}

// SayIntentions moved its gate: getParking reads back somewhere else than its own gate.
TEST(GateAssigner, AssignedWhenTheReadBackMoves) {
    const auto p = preferE7();
    const auto e = finishSend(p, "", SiParking{"E7", kLat + 200 / kMPerDeg, kLon});
    EXPECT_EQ(e["result"], "assigned");
    EXPECT_EQ(e["sent"], true);
    EXPECT_EQ(e["si_parking"], "E7");
    EXPECT_EQ(e["error"], "");
}

// KLAS in milestone 9's sim test: assignGate("239") answered without an error, but getParking still read back E7A.
TEST(GateAssigner, IgnoredWhenTheReadBackStays) {
    const auto p = preferE7();
    const auto e = finishSend(p, "", SiParking{"E9A", kLat + 612 / kMPerDeg, kLon});
    EXPECT_EQ(e["result"], "ignored");
    EXPECT_EQ(e["si_parking"], "E9A");
}

TEST(GateAssigner, UnconfirmedWithoutReadBack) {
    EXPECT_EQ(finishSend(preferE7(), "", std::nullopt)["result"], "unconfirmed");
}

TEST(GateAssigner, UnknownGateResult) {
    const auto p = preferE7();
    const auto e = finishSend(p, "SayIntentions: Unknown gate: E7", std::nullopt);
    EXPECT_EQ(e["result"], "unknown_gate");
    EXPECT_EQ(e["sent"], true);
    EXPECT_EQ(e["error"], "SayIntentions: Unknown gate: E7");
    EXPECT_EQ(finishSend(p, "SayIntentions didn't answer", std::nullopt)["result"], "error");
}

// Before departure, SayIntentions' assigned_gate may be the departure gate: it only counts once its position is at
// the destination (D-141; review of milestone 9).
TEST(GateAssigner, WaitsWhileTheGateIsntAtTheDestination) {
    SiFlight atOrigin = si(0);
    atOrigin.gateLat = 37.6188;  // KSFO
    atOrigin.gateLon = -122.375;
    EXPECT_FALSE(planGate(atOrigin, "KLAS", klas(), kBbj).has_value());
    SiFlight noPosition = si(610);
    noPosition.gateLat.reset();
    noPosition.gateLon.reset();
    EXPECT_FALSE(planGate(noPosition, "KLAS", klas(), kBbj).has_value());
}
