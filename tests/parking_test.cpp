// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Parking spots from the sim's facility data: their names, positions and the nearest one (milestone 9, D-147).

#include <gtest/gtest.h>

#include "geo.h"
#include "parking.h"

using namespace blocktime;

namespace {

ParkingSpot spot(int name, int number, int type = 9) {
    ParkingSpot s;
    s.name = name;
    s.number = number;
    s.type = type;
    return s;
}

// A spot `northM` metres north of (36.0, -115.0).
ParkingSpot spotNorth(double northM, int number) {
    ParkingSpot s = spot(16, number);
    placeSpot(s, 36.0, -115.0, 0.0, northM);
    return s;
}

}  // namespace

TEST(Parking, NamesGateWithLetter) {
    EXPECT_EQ(spotName(spot(16, 7)), "Gate E7");
    EXPECT_EQ(spotShortName(spot(16, 7)), "E7");
    EXPECT_EQ(spotName(spot(12, 1)), "Gate A1");
    EXPECT_EQ(spotName(spot(37, 3)), "Gate Z3");
}

TEST(Parking, NamesPlainGate) {
    EXPECT_EQ(spotName(spot(10, 46)), "Gate 46");
    EXPECT_EQ(spotShortName(spot(10, 46)), "46");
}

TEST(Parking, NamesRamp) {
    EXPECT_EQ(spotName(spot(1, 70, 3)), "Parking 70");
    EXPECT_EQ(spotShortName(spot(1, 70, 3)), "70");
    EXPECT_EQ(spotName(spot(2, 3, 3)), "N Parking 3");
    EXPECT_EQ(spotShortName(spot(2, 3, 3)), "3");
    EXPECT_EQ(spotName(spot(9, 4, 3)), "NW Parking 4");
    EXPECT_EQ(spotName(spot(11, 2, 3)), "Dock 2");
    EXPECT_EQ(spotName(spot(0, 5, 3)), "Spot 5");
}

TEST(Parking, PlaceSpotOffsets) {
    ParkingSpot s = spot(16, 7);
    placeSpot(s, 36.0, -115.0, 0.0, 1113.2);
    EXPECT_NEAR(s.lat, 36.01, 1e-6);
    EXPECT_NEAR(s.lon, -115.0, 1e-9);
    placeSpot(s, 36.0, -115.0, 500.0, 0.0);
    EXPECT_NEAR(distanceMeters(36.0, -115.0, s.lat, s.lon), 500.0, 1.0);
}

TEST(Parking, NearestWithin40m) {
    const std::vector<ParkingSpot> spots{spotNorth(30, 1), spotNorth(5, 2)};
    const auto s = nearestSpot(spots, 36.0, -115.0);
    ASSERT_TRUE(s.has_value());
    EXPECT_EQ(s->number, 2);
}

TEST(Parking, NoneBeyond40m) {
    const std::vector<ParkingSpot> spots{spotNorth(41, 1)};
    EXPECT_FALSE(nearestSpot(spots, 36.0, -115.0).has_value());
    EXPECT_FALSE(nearestSpot({}, 36.0, -115.0).has_value());
}
