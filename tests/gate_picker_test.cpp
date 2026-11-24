// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// pickGate: the arrival gate by D-074's rules, with milestone 9's details (D-147).

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "gate_picker.h"

using namespace blocktime;

namespace {

constexpr double kLat = 36.0, kLon = -115.0;
constexpr double kBbjM = 35.8;  // the BBJ2's wingspan

// Runway 09/27, 3,000 m, centred on the reference point: its ends are 1,500 m east and west.
AirportFacilities airport(std::vector<ParkingSpot> spots) {
    AirportFacilities a;
    a.icao = "KTST";
    a.lat = kLat;
    a.lon = kLon;
    a.runways = {Runway{kLat, kLon, 90.0, 3000.0, 45.0, 9, 0, 27, 0}};
    a.spots = std::move(spots);
    return a;
}

ParkingSpot spot(int name, int number, int type, double radiusM, double eastM, double northM) {
    ParkingSpot s;
    s.name = name;
    s.number = number;
    s.type = type;
    s.radiusM = radiusM;
    placeSpot(s, kLat, kLon, eastM, northM);
    return s;
}

GateContext context(SpotUse use, std::vector<std::string> preferred = {}) { return {kBbjM, use, std::move(preferred)}; }

std::string picked(const std::optional<GatePick>& p) { return p ? spotName(p->spot) : "none"; }

}  // namespace

TEST(GatePicker, BbjFitsKsfoD14) {
    EXPECT_TRUE(spotFits(spot(15, 14, 8, 18.0, 0, 0), kBbjM));
    EXPECT_FALSE(spotFits(spot(15, 14, 8, 17.8, 0, 0), kBbjM));
}

TEST(GatePicker, SpotUseByKind) {
    EXPECT_EQ(spotUseFor("passenger"), SpotUse::Gate);
    EXPECT_EQ(spotUseFor("vip"), SpotUse::Gate);
    EXPECT_EQ(spotUseFor("charter"), SpotUse::Ramp);
    EXPECT_EQ(spotUseFor("delivery"), SpotUse::Ramp);
    EXPECT_EQ(spotUseFor("cargo"), SpotUse::CargoRamp);
}

TEST(GatePicker, PreferredFirstThatFits) {
    const auto a = airport({spot(15, 12, 9, 15, 0, 900), spot(15, 14, 9, 18, 0, 1000), spot(14, 1, 9, 20, 0, 100)});
    const auto p = pickGate(a, "27", context(SpotUse::Gate, {"D12", "D14"}));
    EXPECT_EQ(picked(p), "Gate D14");
    EXPECT_EQ(p->reason, PickReason::Preferred);
}

TEST(GatePicker, ClosestGateToRunway) {
    // A is 300 m from the runway's middle, 1,530 m from either threshold; B is 600 m past the east end.
    const auto a = airport({spot(12, 1, 9, 20, 0, 300), spot(13, 1, 9, 20, 2100, 0)});
    const auto p = pickGate(a, "27", context(SpotUse::Gate));
    EXPECT_EQ(picked(p), "Gate A1");
    EXPECT_EQ(p->reason, PickReason::Closest);
}

TEST(GatePicker, CharterGetsRampNotGate) {
    const auto a = airport({spot(12, 1, 9, 20, 0, 100), spot(1, 70, 4, 20, 0, 800)});
    const auto p = pickGate(a, "27", context(SpotUse::Ramp));
    EXPECT_EQ(picked(p), "Parking 70");
    EXPECT_EQ(p->reason, PickReason::Closest);
}

TEST(GatePicker, PreferredIgnoredForCharter) {
    const auto a = airport({spot(15, 14, 9, 20, 0, 100), spot(1, 70, 4, 20, 0, 800)});
    EXPECT_EQ(picked(pickGate(a, "27", context(SpotUse::Ramp, {"D14"}))), "Parking 70");
}

TEST(GatePicker, FreighterGetsCargoRamp) {
    const auto a = airport({spot(1, 1, 4, 20, 0, 100), spot(1, 2, 5, 20, 0, 900)});
    const auto p = pickGate(a, "27", context(SpotUse::CargoRamp));
    EXPECT_EQ(picked(p), "Parking 2");
    EXPECT_EQ(p->reason, PickReason::Closest);
}

TEST(GatePicker, NoGaRampFallsToAnyRamp) {
    const auto a = airport({spot(1, 1, 4, 15, 0, 100), spot(1, 2, 5, 20, 0, 900), spot(12, 1, 9, 20, 0, 50)});
    const auto p = pickGate(a, "27", context(SpotUse::Ramp));
    EXPECT_EQ(picked(p), "Parking 2");
    EXPECT_EQ(p->reason, PickReason::AnyRamp);
}

TEST(GatePicker, NoRampFallsToGate) {
    const auto a = airport({spot(1, 1, 3, 10, 0, 100), spot(12, 1, 9, 20, 0, 500)});
    const auto p = pickGate(a, "27", context(SpotUse::Ramp));
    EXPECT_EQ(picked(p), "Gate A1");
    EXPECT_EQ(p->reason, PickReason::GateFallback);
}

TEST(GatePicker, MilitaryNeverPicked) {
    const auto a = airport({spot(1, 1, 6, 30, 0, 100), spot(1, 2, 7, 30, 0, 100), spot(12, 1, 9, 20, 0, 900)});
    EXPECT_EQ(picked(pickGate(a, "27", context(SpotUse::Ramp))), "Gate A1");
}

TEST(GatePicker, UnknownRunwayUsesReferencePoint) {
    // X is 200 m from the reference point; Y is 100 m past the east end, 1,600 m from it.
    const auto a = airport({spot(12, 1, 9, 20, 0, 200), spot(13, 1, 9, 20, 1600, 0)});
    EXPECT_EQ(picked(pickGate(a, "27", context(SpotUse::Gate))), "Gate B1");
    EXPECT_EQ(picked(pickGate(a, "", context(SpotUse::Gate))), "Gate A1");
    EXPECT_EQ(picked(pickGate(a, "36", context(SpotUse::Gate))), "Gate A1");
}

TEST(GatePicker, NoGateFitsGivesNothing) {
    const auto a = airport({spot(12, 1, 9, 15, 0, 100), spot(1, 70, 4, 30, 0, 200)});
    EXPECT_FALSE(pickGate(a, "27", context(SpotUse::Gate)).has_value());
}

TEST(GatePicker, ReasonCodes) {
    EXPECT_EQ(pickReasonCode(PickReason::Preferred), "preferred");
    EXPECT_EQ(pickReasonCode(PickReason::Closest), "closest");
    EXPECT_EQ(pickReasonCode(PickReason::AnyRamp), "any_ramp");
    EXPECT_EQ(pickReasonCode(PickReason::GateFallback), "gate_fallback");
}
