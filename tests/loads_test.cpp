// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "loads.h"

using namespace blocktime;

namespace {

// A Roll that returns `values` in order and counts its calls.
struct Rolls {
    std::vector<double> values;
    std::size_t calls = 0;
    Roll roll() {
        return [this] { return values.at(calls++); };
    }
};

const std::vector<Cabin> kVip{{"economy", 48}};
const std::vector<Cabin> kTwoClass{{"business", 16}, {"economy", 144}};
const std::vector<Hold> kBbjHolds{{"forward", 7842, 4}, {"aft", 9803, 5}};
const std::vector<Hold> k738Holds{{"forward", 7500, 4}, {"aft", 9500, 5}};
const AirplaneLimits kBbj{7000, 146300, 190, 55, 36113};
const AirplaneLimits k738{7000, 146300, 190, 55, 47000};

int passengers(const LoadRoll& l, const std::string& cabinClass) {
    for (const CabinLoad& c : l.cabins) {
        if (c.cabinClass == cabinClass) return c.passengers;
    }
    return -1;
}

}  // namespace

TEST(Loads, DaytimeBand) {
    Rolls r{{0.5}};
    EXPECT_EQ(passengers(rollLoads(kVip, kBbjHolds, kBbj, FlightKind::Vip, true, r.roll()), "economy"), 45);
}

TEST(Loads, NightBand) {
    Rolls r{{0.0}};
    EXPECT_EQ(passengers(rollLoads(kVip, kBbjHolds, kBbj, FlightKind::Vip, false, r.roll()), "economy"), 19);
}

TEST(Loads, CharterBand) {
    Rolls r{{0.0}};
    const LoadRoll l = rollLoads(kVip, kBbjHolds, kBbj, FlightKind::Charter, true, r.roll());
    EXPECT_EQ(passengers(l, "economy"), 4);
    EXPECT_EQ(l.cargoLb, 0);
    EXPECT_EQ(r.calls, 1u);  // no cargo roll on an executive flight
}

TEST(Loads, EachCabinRolledSeparately) {
    Rolls r{{0.0, 0.999, 0.0}};
    const LoadRoll l = rollLoads(kTwoClass, k738Holds, k738, FlightKind::Passenger, true, r.roll());
    EXPECT_EQ(passengers(l, "business"), 14);
    EXPECT_EQ(passengers(l, "economy"), 143);
}

TEST(Loads, BellyCargoFromSpaceLeftAfterBags) {
    Rolls r{{0.0, 0.0, 0.0}};
    const LoadRoll l = rollLoads(kTwoClass, k738Holds, k738, FlightKind::Passenger, true, r.roll());
    EXPECT_EQ(passengers(l, "business"), 14);
    EXPECT_EQ(passengers(l, "economy"), 129);
    EXPECT_EQ(bagsLb(l.cabins, k738), 7865);
    EXPECT_EQ(l.cargoLb, 913);  // 10% of 17,000 - 7,865
    EXPECT_EQ(r.calls, 3u);
}

TEST(Loads, CargoTrimmedToMaxPayload) {
    AirplaneLimits tight = k738;
    tight.maxPayloadLb = 36000;
    Rolls r{{0.0, 0.0, 0.999}};
    const LoadRoll l = rollLoads(kTwoClass, k738Holds, tight, FlightKind::Passenger, true, r.roll());
    EXPECT_EQ(l.cargoLb, 965);  // 36,000 - 143 x 190 - 7,865
}

TEST(Loads, FreighterLoad) {
    const std::vector<Hold> holds{{"main_deck", 40000, 20}, {"forward", 7500, 4}, {"aft", 9500, 5}};
    Rolls r{{0.0}};
    const LoadRoll l = rollLoads({}, holds, {7000, 146300, 190, 55, 52000}, FlightKind::Cargo, true, r.roll());
    EXPECT_TRUE(l.cabins.empty());
    EXPECT_EQ(l.cargoLb, 28500);
}

TEST(Loads, DeliveryHasNoLoadAndNoRolls) {
    Rolls r{{}};
    const LoadRoll l = rollLoads(kVip, kBbjHolds, kBbj, FlightKind::Delivery, true, r.roll());
    EXPECT_TRUE(l.cabins.empty());
    EXPECT_EQ(l.cargoLb, 0);
    EXPECT_EQ(r.calls, 0u);
}

TEST(Loads, KindCodesRoundTrip) {
    for (FlightKind k : {FlightKind::Passenger, FlightKind::Vip, FlightKind::Charter, FlightKind::Cargo,
                         FlightKind::Delivery}) {
        EXPECT_EQ(parseKind(kindCode(k)), k);
    }
    EXPECT_EQ(kindCode(FlightKind::Vip), "vip");
    EXPECT_FALSE(parseKind("ferry").has_value());
}

TEST(Loads, Daytime) {
    EXPECT_FALSE(isDaytime(6));
    EXPECT_TRUE(isDaytime(7));
    EXPECT_TRUE(isDaytime(18));
    EXPECT_FALSE(isDaytime(19));
}

TEST(Holds, SplitByRatio) {
    const auto h = splitHolds(kBbjHolds, 9000);
    ASSERT_EQ(h.size(), 2u);
    EXPECT_EQ(h[0].hold, "forward");
    EXPECT_EQ(h[0].weightLb, 4000);
    EXPECT_EQ(h[1].weightLb, 5000);
}

TEST(Holds, OverflowSpillsToRoom) {
    const auto h = splitHolds({{"forward", 1000, 4}, {"aft", 9000, 5}}, 9000);
    EXPECT_EQ(h[0].weightLb, 1000);
    EXPECT_EQ(h[1].weightLb, 8000);
}

TEST(Holds, RemainderToLargestRatio) {
    const auto h = splitHolds(kBbjHolds, 1001);
    EXPECT_EQ(h[0].weightLb, 444);
    EXPECT_EQ(h[1].weightLb, 557);
}

TEST(Holds, ZeroIsEmptyHolds) {
    const auto h = splitHolds(kBbjHolds, 0);
    EXPECT_EQ(h[0].weightLb, 0);
    EXPECT_EQ(h[1].weightLb, 0);
}

// D-215: every seat, then cargo up to the hold space after bags or the payload left, whichever is less.
TEST(Loads, FullyLoadedPassengerFillsSeatsThenCargo) {
    const LoadRoll l = fullLoads(kTwoClass, k738Holds, k738, FlightKind::Passenger);
    EXPECT_EQ(passengers(l, "business"), 16);
    EXPECT_EQ(passengers(l, "economy"), 144);
    const int bags = 160 * 55;                             // 8,800
    const int space = 7500 + 9500 - bags;                  // 8,200
    const int payloadLeft = 47000 - 160 * 190 - bags;      // 7,800
    EXPECT_EQ(l.cargoLb, std::min(space, payloadLeft));
}

TEST(Loads, FullyLoadedTrimsCargoNeverBags) {
    AirplaneLimits tight = k738;
    tight.maxPayloadLb = 45000;  // seats and bags weigh 39,200: 5,800 left for cargo, less than the space
    LoadRoll l = fullLoads(kTwoClass, k738Holds, tight, FlightKind::Passenger);
    EXPECT_EQ(passengers(l, "economy"), 144);
    EXPECT_EQ(160 * 190 + bagsLb(l.cabins, tight) + l.cargoLb, 45000);
    tight.maxPayloadLb = 35000;  // the seats and bags alone are over it
    l = fullLoads(kTwoClass, k738Holds, tight, FlightKind::Passenger);
    EXPECT_EQ(passengers(l, "business"), 16);
    EXPECT_EQ(l.cargoLb, 0);
}

TEST(Loads, FullyLoadedFreighter) {
    const std::vector<Hold> bcf{{"main_deck", 40000, 8}, {"forward", 5000, 1}, {"aft", 6000, 1}};
    AirplaneLimits limits = k738;
    limits.maxPayloadLb = 46000;
    EXPECT_EQ(fullLoads({}, bcf, limits, FlightKind::Cargo).cargoLb, 46000);
    limits.maxPayloadLb = 60000;
    EXPECT_EQ(fullLoads({}, bcf, limits, FlightKind::Cargo).cargoLb, 51000);
    EXPECT_TRUE(fullLoads({}, bcf, limits, FlightKind::Cargo).cabins.empty());
}
