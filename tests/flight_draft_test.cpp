// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstddef>
#include <string>
#include <vector>

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

// Review 4.3: New Flight's preview is what the planned flight shows once it's created, for every kind the test
// airplanes fly. (No freighter in the test airline, so no cargo flight.)
TEST(Draft, PreviewMatchesThePlannedFlight) {
    db::Database db = test::testAirline();
    const auto same = [&](FlightDraft& d) {
        d.setDestination(checkDestination(db, d.airplane(), "KSAN"));
        d.setFlightNumber("101");
        const auto draft = d.preview();
        ASSERT_TRUE(draft.has_value());
        std::string error;
        ASSERT_TRUE(createPlannedFlight(db, d.toNew("2026-11-23T14:00:00Z", {}), &error).has_value()) << error;
        const Preview planned = pricePreview(priceInput(*currentPlannedFlight(db)), Rates{});
        const auto flat = [](const Preview& p) {
            std::vector<std::string> out;
            for (const auto* lines : {&p.income, &p.costs}) {
                for (const MoneyLine& m : *lines) {
                    out.push_back(m.category + " " + m.label + " " + std::to_string(m.cents));
                }
            }
            return out;
        };
        EXPECT_EQ(flat(*draft), flat(planned)) << "kind " << static_cast<int>(d.kind());
        EXPECT_EQ(draft->totalCents, planned.totalCents);
        EXPECT_EQ(draft->fuelIncluded, planned.fuelIncluded);
        ASSERT_TRUE(cancelPlannedFlight(db, &error)) << error;
    };
    Rolls rolls;
    FlightDraft airliner = draftFor(db, "N738BT", rolls);  // passengers with belly cargo
    same(airliner);
    FlightDraft delivery = draftFor(db, "N814SX", rolls);
    same(delivery);
    ASSERT_TRUE(db.exec("UPDATE airplane SET awaiting_delivery = 0 WHERE registration = 'N814SX'"));
    for (FlightKind kind : {FlightKind::Vip, FlightKind::Charter}) {
        FlightDraft d = draftFor(db, "N814SX", rolls);
        d.setKind(kind);
        same(d);
    }
}

TEST(FlightDraft, FullyLoadedOnlyForPassengerAndCargo) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft airliner = draftFor(db, "N738BT", rolls);
    EXPECT_TRUE(airliner.fullyLoadedAvailable());
    FlightDraft delivery = draftFor(db, "N814SX", rolls);
    EXPECT_FALSE(delivery.fullyLoadedAvailable());
    ASSERT_TRUE(db.exec("UPDATE airplane SET awaiting_delivery = 0 WHERE registration = 'N814SX'"));
    FlightDraft vip = draftFor(db, "N814SX", rolls);
    EXPECT_FALSE(vip.fullyLoadedAvailable());
    vip.setKind(FlightKind::Charter);
    EXPECT_FALSE(vip.fullyLoadedAvailable());
}

TEST(FlightDraft, FullyLoadedKeepsTheRandomRoll) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    const LoadRoll random = d.loads();
    d.setFullyLoaded(true);
    EXPECT_TRUE(d.fullyLoaded());
    EXPECT_FALSE(d.bellyCargoAvailable());  // fully loaded always carries cargo
    EXPECT_EQ(d.loads().cabins.at(0).passengers, 16);
    EXPECT_EQ(d.loads().cabins.at(1).passengers, 144);
    d.setFullyLoaded(false);
    EXPECT_EQ(d.loads().cabins.at(1).passengers, random.cabins.at(1).passengers);
    EXPECT_EQ(d.loads().cargoLb, random.cargoLb);
    EXPECT_EQ(rolls.calls, 3u);
}

TEST(FlightDraft, ExtraFuelCarriedIntoToNew) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    EXPECT_EQ(d.additionalFuelLb(), 0);
    d.setAdditionalFuelLb(1500);
    d.setDestination(checkDestination(db, d.airplane(), "KSAN"));
    EXPECT_EQ(d.toNew("2026-11-23T14:00:00Z", {}).additionalFuelLb, 1500);
}

TEST(FlightDraft, FromPlannedDoesNotRoll) {
    db::Database db = test::testAirline();
    Rolls rolls;
    FlightDraft d = draftFor(db, "N738BT", rolls);
    d.setDestination(checkDestination(db, d.airplane(), "KSAN"));
    d.setFlightNumber("1414");
    d.setAdditionalFuelLb(1000);
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, d.toNew("2026-11-23T14:00:00Z", {}), &error).has_value()) << error;
    const auto view = currentPlannedFlight(db);
    ASSERT_TRUE(view.has_value());
    const Roll never = [] {
        ADD_FAILURE() << "fromPlanned rolled";
        return 0.5;
    };
    const FlightDraft again =
        FlightDraft::fromPlanned(*loadFlightAirplane(db, "N738BT"), Rates{}, *view, true, never);
    EXPECT_EQ(again.kind(), FlightKind::Passenger);
    EXPECT_EQ(again.flightNumber(), "1414");
    EXPECT_EQ(again.additionalFuelLb(), 1000);
    EXPECT_EQ(again.loads().cargoLb, view->cargoLb);
    ASSERT_EQ(again.loads().cabins.size(), view->cabins.size());
    for (std::size_t i = 0; i < view->cabins.size(); ++i) {
        EXPECT_EQ(again.loads().cabins[i].passengers, view->cabins[i].passengers);
    }
    EXPECT_TRUE(again.destination().ok);
    EXPECT_EQ(again.destination().icao, "KSAN");
    EXPECT_TRUE(again.canPlan());
}
