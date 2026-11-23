// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>
#include <string>

#include <gtest/gtest.h>

#include "flight_draft.h"
#include "test_airline.h"

using namespace blocktime;

namespace {

// Hands out 0.5 forever and counts the calls.
struct Rolls {
    std::size_t calls = 0;
    Roll roll() {
        return [this] {
            ++calls;
            return 0.5;
        };
    }
};

FlightDraft draftFor(db::Database& db, const char* registration, Rolls& rolls) {
    return FlightDraft(*loadFlightAirplane(db, registration), Rates{}, true, rolls.roll());
}

}  // namespace

TEST(Draft, TogglingBellyCargoDoesNotReroll) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    EXPECT_EQ(rolls.calls, 3u);  // two cabins and the belly cargo
    ASSERT_TRUE(d.bellyCargoAvailable());
    EXPECT_TRUE(d.bellyCargo());
    const int rolled = d.loads().cargoLb;
    EXPECT_GT(rolled, 0);
    for (int i = 0; i < 3; ++i) {
        d.setBellyCargo(false);
        EXPECT_EQ(d.loads().cargoLb, 0);
        d.setBellyCargo(true);
        EXPECT_EQ(d.loads().cargoLb, rolled);
    }
    EXPECT_EQ(rolls.calls, 3u);
}

TEST(Draft, SwitchingKindKeepsEachKindsRoll) {
    db::Database db = test::testAirline();
    ASSERT_TRUE(db.exec("UPDATE airplane SET awaiting_delivery = 0 WHERE registration = 'N814SX'"));
    Rolls rolls;
    FlightDraft d = draftFor(db, "N814SX", rolls);
    EXPECT_EQ(d.kind(), FlightKind::Vip);
    EXPECT_FALSE(d.bellyCargoAvailable());
    const int vipPassengers = d.loads().cabins.at(0).passengers;
    d.setKind(FlightKind::Charter);
    d.setKind(FlightKind::Vip);
    EXPECT_EQ(d.loads().cabins.at(0).passengers, vipPassengers);
    EXPECT_EQ(rolls.calls, 2u);
}

TEST(Draft, DeliveryPreviewIsALoss) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N814SX", rolls);
    EXPECT_EQ(d.kind(), FlightKind::Delivery);
    EXPECT_FALSE(d.preview().has_value());  // no destination yet
    d.setDestination(checkDestination(db, d.airplane(), "KSAN"));
    const auto p = d.preview();
    ASSERT_TRUE(p.has_value());
    EXPECT_TRUE(p->income.empty());
    EXPECT_LT(p->totalCents, 0);
    EXPECT_TRUE(d.loads().cabins.empty());
    EXPECT_EQ(rolls.calls, 0u);
}

TEST(Draft, FlightNumberStartsAsTheAirplanesDefault) {
    db::Database db = test::testAirline();
    Rolls rolls;
    EXPECT_EQ(draftFor(db, "N814SX", rolls).flightNumber(), "8");
    EXPECT_EQ(draftFor(db, "N738BT", rolls).flightNumber(), "");
}

TEST(Draft, CannotPlanWithoutFlightNumber) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    d.setDestination(checkDestination(db, d.airplane(), "KSAN"));
    EXPECT_FALSE(d.canPlan());
    d.setFlightNumber("101");
    EXPECT_TRUE(d.canPlan());
    d.setFlightNumber("10A");
    EXPECT_FALSE(d.canPlan());
    d.setFlightNumber("101");
    d.setDestination(checkDestination(db, d.airplane(), "KSHT"));
    EXPECT_FALSE(d.canPlan());
}

TEST(Draft, ToNewCarriesTheChoices) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    d.setDestination(checkDestination(db, d.airplane(), "KSAN"));
    d.setFlightNumber("1414");
    d.setBellyCargo(false);
    const NewPlannedFlight n = d.toNew("2026-11-23T14:00:00Z", {"14R", ""});
    EXPECT_EQ(n.airplaneId, d.airplane().airplaneId);
    EXPECT_EQ(n.originIcao, "KBFI");
    EXPECT_EQ(n.destinationIcao, "KSAN");
    EXPECT_EQ(n.flightNumber, "1414");
    EXPECT_EQ(n.kind, FlightKind::Passenger);
    EXPECT_EQ(n.cargoLb, 0);
    EXPECT_EQ(n.cabins.size(), 2u);
    EXPECT_EQ(n.fuelPriceCentsPer100Lb, 4100);
    EXPECT_EQ(n.originRunway, "14R");
    EXPECT_EQ(n.destinationRunway, "");
}

TEST(Draft, HoldsSplitBagsAndCargo) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    int total = 0;
    for (const HoldLoad& h : d.holds()) total += h.weightLb;
    EXPECT_EQ(total, bagsLb(d.loads().cabins, d.airplane().limits) + d.loads().cargoLb);
}
