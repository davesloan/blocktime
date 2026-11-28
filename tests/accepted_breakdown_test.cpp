// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "accepted_breakdown.h"
#include "planned_flight.h"
#include "test_airline.h"

using namespace blocktime;

namespace {

std::int64_t planTo(db::Database& db, const char* destination = "KSAN") {
    const auto a = loadFlightAirplane(db, "N738BT");
    EXPECT_TRUE(a.has_value());
    NewPlannedFlight f;
    f.airplaneId = a ? a->airplaneId : 0;
    f.originIcao = "KBFI";
    f.destinationIcao = destination;
    f.flightNumber = "814";
    f.kind = FlightKind::Passenger;
    f.cabins = {{"economy", 140}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = "2026-11-27T14:00:00Z";
    std::string error;
    const auto id = createPlannedFlight(db, f, &error);
    EXPECT_TRUE(id.has_value()) << error;
    return id.value_or(0);
}

Preview samplePreview() {
    Preview p;
    p.income.push_back({"economy_fares", "Economy fares", 2205000, {{140, "passengers"}}, 157.5, 1, ""});
    p.costs.push_back({"fuel", "Fuel", -242000, {{5900, "lb"}}, 41, 100, "lb"});
    p.costs.push_back({"pilots", "Pilots", -148000, {{2.96, "h"}, {2, "pilots"}}, 250, 1, ""});
    p.totalCents = 2205000 - 242000 - 148000;
    p.fuelIncluded = true;
    return p;
}

std::int64_t count(db::Database& db, const char* sql) {
    db::Statement q(db, sql);
    EXPECT_TRUE(q.next()) << q.error();
    return q.integer(0);
}

void expectSameLines(const std::vector<MoneyLine>& a, const std::vector<MoneyLine>& b) {
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].category, b[i].category);
        EXPECT_EQ(a[i].label, b[i].label);
        EXPECT_EQ(a[i].cents, b[i].cents);
        EXPECT_NEAR(a[i].rateDollars, b[i].rateDollars, 1e-6);
        EXPECT_EQ(a[i].rateBasis, b[i].rateBasis);
        EXPECT_EQ(a[i].rateUnit, b[i].rateUnit);
        ASSERT_EQ(a[i].quantities.size(), b[i].quantities.size());
        for (std::size_t k = 0; k < a[i].quantities.size(); ++k) {
            EXPECT_DOUBLE_EQ(a[i].quantities[k].value, b[i].quantities[k].value);
            EXPECT_EQ(a[i].quantities[k].unit, b[i].quantities[k].unit);
        }
    }
}

}  // namespace

TEST(AcceptedBreakdown, RoundTripsLinesAndWorkings) {
    db::Database db = test::testAirline();
    const std::int64_t id = planTo(db);
    std::string error;
    ASSERT_TRUE(acceptPlannedFlight(db, id, samplePreview(), "2026-11-27T14:05:00Z", false, &error)) << error;
    const auto got = plannedAcceptance(db, id);
    ASSERT_TRUE(got.has_value());
    EXPECT_EQ(got->acceptedAt, "2026-11-27T14:05:00Z");
    EXPECT_FALSE(got->atBlockOut);
    expectSameLines(got->money.income, samplePreview().income);
    expectSameLines(got->money.costs, samplePreview().costs);
    EXPECT_EQ(got->money.totalCents, samplePreview().totalCents);
    EXPECT_TRUE(got->money.fuelIncluded);
}

TEST(AcceptedBreakdown, ReacceptReplaces) {
    db::Database db = test::testAirline();
    const std::int64_t id = planTo(db);
    std::string error;
    ASSERT_TRUE(acceptPlannedFlight(db, id, samplePreview(), "2026-11-27T14:05:00Z", false, &error)) << error;
    Preview second = samplePreview();
    second.costs[0].cents = -262500;
    second.costs[0].quantities[0].value = 6400;
    second.totalCents -= 20500;
    ASSERT_TRUE(acceptPlannedFlight(db, id, second, "2026-11-27T14:20:00Z", true, &error)) << error;
    EXPECT_EQ(count(db, "SELECT count(*) FROM accepted_breakdown"), 1);
    EXPECT_EQ(count(db, "SELECT count(*) FROM planned_flight_acceptance"), 1);
    EXPECT_EQ(count(db, "SELECT count(*) FROM accepted_line"), 3);
    const auto got = plannedAcceptance(db, id);
    ASSERT_TRUE(got.has_value());
    EXPECT_TRUE(got->atBlockOut);
    expectSameLines(got->money.costs, second.costs);
}

TEST(AcceptedBreakdown, CancelLeavesNothing) {
    db::Database db = test::testAirline();
    const std::int64_t id = planTo(db);
    std::string error;
    ASSERT_TRUE(acceptPlannedFlight(db, id, samplePreview(), "2026-11-27T14:05:00Z", false, &error)) << error;
    ASSERT_TRUE(cancelPlannedFlight(db, &error)) << error;
    EXPECT_EQ(count(db, "SELECT count(*) FROM accepted_breakdown"), 0);
    EXPECT_EQ(count(db, "SELECT count(*) FROM accepted_line"), 0);
    EXPECT_EQ(count(db, "SELECT count(*) FROM accepted_line_quantity"), 0);
    EXPECT_EQ(count(db, "SELECT count(*) FROM planned_flight_acceptance"), 0);
    db::Statement fk(db, "PRAGMA foreign_key_check");
    EXPECT_FALSE(fk.next());
}

TEST(AcceptedBreakdown, MovesToTheFlight) {
    db::Database db = test::testAirline();
    const std::int64_t id = planTo(db);
    std::string error;
    ASSERT_TRUE(acceptPlannedFlight(db, id, samplePreview(), "2026-11-27T14:05:00Z", false, &error)) << error;
    // A settled flight row to move it to.
    ASSERT_TRUE(db.exec(
        "INSERT INTO flight (flight_id, log_flight_id, airplane_id, origin_airport_id, planned_destination_airport_id,"
        " arrival_airport_id, flight_number, kind, cargo_lb, fuel_price_cents_per_100lb, planned_at, block_out_at,"
        " block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 1, 'flt-1', airplane_id, origin_airport_id, destination_airport_id, destination_airport_id,"
        " flight_number, kind, cargo_lb, fuel_price_cents_per_100lb, planned_at, '2026-11-27T15:00:00Z',"
        " '2026-11-27T17:00:00Z', 0, 6000, 1000 FROM planned_flight",
        &error))
        << error;
    {
        db::Transaction tx(db);
        ASSERT_TRUE(moveAcceptanceToFlight(db, id, 1, &error)) << error;
        ASSERT_TRUE(tx.commit(&error)) << error;
    }
    EXPECT_FALSE(plannedAcceptance(db, id).has_value());
    ASSERT_TRUE(cancelPlannedFlight(db, &error)) << error;
    const auto got = flightAcceptance(db, 1);
    ASSERT_TRUE(got.has_value());
    expectSameLines(got->money.income, samplePreview().income);
    expectSameLines(got->money.costs, samplePreview().costs);
}

TEST(PlannedFlight, ExtraFuelStored) {
    db::Database db = test::testAirline();
    const auto a = loadFlightAirplane(db, "N738BT");
    ASSERT_TRUE(a.has_value());
    NewPlannedFlight f;
    f.airplaneId = a->airplaneId;
    f.originIcao = "KBFI";
    f.destinationIcao = "KSAN";
    f.flightNumber = "814";
    f.cabins = {{"economy", 140}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = "2026-11-27T14:00:00Z";
    f.additionalFuelLb = 1000;
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, f, &error).has_value()) << error;
    const auto v = currentPlannedFlight(db);
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(v->additionalFuelLb, 1000);
    EXPECT_FALSE(v->acceptedAt.has_value());
}
