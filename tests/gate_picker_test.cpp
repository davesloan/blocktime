// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// pickGate: whether BlockTime asks for another arrival gate than SayIntentions' (D-147, D-155).

#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "gate_picker.h"

using namespace blocktime;

namespace {

constexpr double kLat = 36.0, kLon = -115.0;
constexpr double kBbjM = 35.8;  // the BBJ2's wingspan

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

// SayIntentions' gate is BlockTime's default: it knows which gates and ramps the scenery has (D-155).
TEST(GatePicker, KeepsSayIntentionsGateThatFits) {
    const auto a = airport({spot(16, 7, 9, 20, 0, 200), spot(16, 9, 9, 20, 0, 600)});
    EXPECT_FALSE(pickGate(a, a.spots[1], context(SpotUse::Gate)).has_value());
    EXPECT_FALSE(pickGate(a, a.spots[1], context(SpotUse::Ramp)).has_value());
}

TEST(GatePicker, KeepsSayIntentionsGateItCantPlace) {
    const auto a = airport({spot(16, 7, 9, 20, 0, 200)});
    EXPECT_FALSE(pickGate(a, std::nullopt, context(SpotUse::Gate)).has_value());
}

TEST(GatePicker, PreferredFirstThatFits) {
    const auto a = airport({spot(15, 12, 9, 15, 0, 900), spot(15, 14, 9, 18, 0, 1000), spot(14, 1, 9, 20, 0, 100)});
    const auto p = pickGate(a, a.spots[2], context(SpotUse::Gate, {"D12", "D14"}));
    EXPECT_EQ(picked(p), "Gate D14");
    EXPECT_EQ(p->reason, PickReason::Preferred);
}

TEST(GatePicker, PreferredEvenWhenSayIntentionsCantBePlaced) {
    const auto a = airport({spot(15, 14, 9, 20, 0, 1000)});
    EXPECT_EQ(picked(pickGate(a, std::nullopt, context(SpotUse::Gate, {"D14"}))), "Gate D14");
}

TEST(GatePicker, SayIntentionsGivingAPreferredGateIsKept) {
    const auto a = airport({spot(15, 12, 9, 20, 0, 900), spot(15, 14, 9, 20, 0, 1000)});
    EXPECT_FALSE(pickGate(a, a.spots[1], context(SpotUse::Gate, {"D12", "D14"})).has_value());
}

// A favourite ramp works for charter and cargo; a favourite gate isn't used for them, nor a ramp for a gate flight.
TEST(GatePicker, PreferredMatchesHowTheFlightParks) {
    const auto a = airport({spot(15, 14, 9, 20, 0, 100), spot(1, 70, 4, 20, 0, 800), spot(16, 7, 9, 20, 0, 200)});
    EXPECT_FALSE(pickGate(a, a.spots[2], context(SpotUse::Ramp, {"D14"})).has_value());
    EXPECT_EQ(picked(pickGate(a, a.spots[2], context(SpotUse::Ramp, {"D14", "70"}))), "Parking 70");
    EXPECT_EQ(picked(pickGate(a, a.spots[2], context(SpotUse::CargoRamp, {"70"}))), "Parking 70");
    EXPECT_FALSE(pickGate(a, a.spots[2], context(SpotUse::Gate, {"70"})).has_value());
}

// SayIntentions' gate too small for the type: the nearest one of the same kind that fits.
TEST(GatePicker, LargerGateNearestToSayIntentions) {
    const auto a = airport({spot(16, 7, 9, 15, 0, 200), spot(16, 9, 9, 20, 0, 600), spot(16, 5, 9, 20, 0, 350),
                            spot(1, 70, 4, 30, 0, 210)});
    const auto p = pickGate(a, a.spots[0], context(SpotUse::Gate));
    EXPECT_EQ(picked(p), "Gate E5");
    EXPECT_EQ(p->reason, PickReason::Larger);
}

TEST(GatePicker, LargerRampForARamp) {
    const auto a = airport({spot(1, 70, 4, 15, 0, 200), spot(12, 1, 9, 20, 0, 210), spot(1, 2, 5, 20, 0, 400)});
    EXPECT_EQ(picked(pickGate(a, a.spots[0], context(SpotUse::Ramp))), "Parking 2");
}

TEST(GatePicker, NothingLargerKeepsSayIntentions) {
    const auto a = airport({spot(16, 7, 9, 15, 0, 200), spot(1, 70, 4, 30, 0, 210)});
    EXPECT_FALSE(pickGate(a, a.spots[0], context(SpotUse::Gate)).has_value());
}

TEST(GatePicker, MilitaryNeverPicked) {
    const auto a = airport({spot(1, 1, 4, 15, 0, 100), spot(1, 2, 6, 30, 0, 110), spot(1, 3, 3, 20, 0, 900)});
    EXPECT_EQ(picked(pickGate(a, a.spots[0], context(SpotUse::Ramp))), "Parking 3");
}

TEST(GatePicker, ReasonCodes) {
    EXPECT_EQ(pickReasonCode(PickReason::Preferred), "preferred");
    EXPECT_EQ(pickReasonCode(PickReason::Larger), "larger");
}
