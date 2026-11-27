// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "airline_db.h"
#include "airline_queries.h"
#include "catalog.h"
#include "fleet_rules.h"
#include "leasing.h"
#include "rates.h"

using namespace blocktime;
using namespace std::chrono;

namespace {

constexpr Date d(int y, unsigned m, unsigned day) { return Date{year{y}, month{m}, std::chrono::day{day}}; }

// N814SX's SimBrief airframe (docs/notes/simbrief-airframes.md), with another registration and Internal ID.
SimbriefReading bbjReading(const std::string& registration, const std::string& internalId) {
    SimbriefReading r;
    r.registration = registration;
    r.finNumber = "1001";
    r.internalId = internalId;
    r.baseType = "BBJ2";
    r.weightUnits = "LBS";
    r.maxPassengers = 48;
    r.passengerWeightLb = 190;
    r.bagWeightLb = 55;
    r.emptyWeightLb = 102187;
    r.mzfwLb = 138300;
    r.mtowLb = 174700;
    r.mlwLb = 146300;
    r.maxFuelLb = 69986;
    r.maxCargoLb = 17645;
    r.minRunwayFt = 7000;
    return r;
}

SimbriefReading b738Reading(const std::string& registration, const std::string& internalId) {
    SimbriefReading r = bbjReading(registration, internalId);
    r.finNumber = "";
    r.baseType = "B738";
    r.maxPassengers = 160;
    r.emptyWeightLb = 91300;
    r.maxCargoLb = 18000;
    return r;
}

class Leasing : public ::testing::Test {
protected:
    void SetUp() override {
        std::string error;
        ASSERT_TRUE(migrateAirlineDatabase(db_, &error)) << error;
        ASSERT_TRUE(db_.exec(
            "INSERT INTO airport (icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
            " longest_runway_ft) VALUES"
            " ('KBFI', 'Boeing Field', 47.53, -122.30, 21, 'US', 'America/Los_Angeles', 10007),"
            " ('KSAN', 'San Diego', 32.73, -117.19, 17, 'US', 'America/Los_Angeles', 9401),"
            " ('KSHT', 'Short Field', 47.0, -120.0, 1000, 'US', 'America/Los_Angeles', 5000)"));
        catalog_ = loadCatalogDir(BLOCKTIME_SOURCE_DIR "/data/aircraft", nullptr);
        ASSERT_EQ(catalog_.types.size(), 7u);
        config_.identity = {"BlockTime Air", "BTA", "BLOCKTIME"};
        config_.foundedOn = d(2026, 11, 22);
        config_.startingCashCents = 500'000'000;
        ASSERT_TRUE(syncCatalogTypes(db_, catalog_, &error)) << error;
        ASSERT_TRUE(postOpeningCash(db_, config_, &error)) << error;
    }

    LeaseRequest bbj(const std::string& registration, const std::string& internalId, Date today) const {
        return {bbjReading(registration, internalId), catalog_.type("737-800 BBJ2"),
                catalog_.airframe("737-800 BBJ2 VIP"), "", std::nullopt, today};
    }
    LeaseRequest b738(const std::string& registration, const std::string& internalId, Date today) const {
        return {b738Reading(registration, internalId), catalog_.type("737-800"), catalog_.airframe("737-800 Two-class"),
                "", std::nullopt, today};
    }

    std::int64_t lease(const LeaseRequest& request) {
        const LeaseResult result = leaseAirplane(db_, request, rates_, slots_);
        EXPECT_EQ(result.refusal, "");
        EXPECT_TRUE(result.airplaneId.has_value());
        return result.airplaneId.value_or(0);
    }

    LeaseOutcome settle(Date today) {
        LeaseOutcome outcome = settleLeases(db_, today);
        EXPECT_EQ(outcome.error, "");
        return outcome;
    }

    std::int64_t count(const std::string& sql) {
        db::Statement q(db_, sql);
        EXPECT_TRUE(q.next()) << q.error();
        return q.integer(0);
    }

    db::Database db_ = db::Database::openInMemory();
    Catalog catalog_;
    AirlineConfig config_;
    Rates rates_;
    std::optional<int> slots_ = 10;  // the lease tests aren't about slots; the slot tests clear it
};

}  // namespace

TEST_F(Leasing, LeaseFromReadingWritesEverything) {
    const auto id = lease(b738("N738SX", "1_1", d(2026, 11, 22)));
    const std::vector<AirplaneView> airplanes = listAirplanes(db_);
    ASSERT_EQ(airplanes.size(), 1u);
    const AirplaneView& a = airplanes[0];
    EXPECT_EQ(a.registration, "N738SX");
    EXPECT_EQ(a.tailNumber, "");
    EXPECT_EQ(a.typeCode, "737-800");
    EXPECT_EQ(a.airframeName, "737-800 Two-class");
    EXPECT_EQ(a.locationIcao, "KBFI");
    EXPECT_EQ(a.fuelOnBoardLb, 0);
    EXPECT_TRUE(a.awaitingDelivery);
    EXPECT_EQ(count("SELECT simbrief_airframe_id = '1_1' FROM airplane"), 1);
    EXPECT_EQ(count("SELECT seats FROM airplane_cabin WHERE cabin_class = 'business'"), 16);
    EXPECT_EQ(count("SELECT seats FROM airplane_cabin WHERE cabin_class = 'economy'"), 144);
    EXPECT_EQ(count("SELECT max_lb FROM airplane_hold WHERE hold = 'forward'"), 7941);
    EXPECT_EQ(count("SELECT max_lb FROM airplane_hold WHERE hold = 'aft'"), 10059);
    EXPECT_EQ(count("SELECT ratio FROM airplane_hold WHERE hold = 'aft'"), 5);
    const auto limits = airplaneLimits(db_, id);
    ASSERT_TRUE(limits.has_value());
    EXPECT_EQ(limits->minRunwayFt, 7000);
    EXPECT_EQ(limits->mlwLb, 146300);
    EXPECT_EQ(limits->passengerWeightLb, 190);
    EXPECT_EQ(limits->bagWeightLb, 55);
    EXPECT_EQ(limits->maxPayloadLb, 47000);  // MZFW 138,300 - empty 91,300
    EXPECT_EQ(count("SELECT mtow_lb FROM airplane_simbrief"), 174700);
    EXPECT_EQ(count("SELECT max_cargo_lb FROM airplane_simbrief"), 18000);
    EXPECT_EQ(count("SELECT max_passengers FROM airplane_simbrief"), 160);
    EXPECT_EQ(count("SELECT monthly_lease_cents FROM airplane"), 23'500'000);
    EXPECT_EQ(cashCents(db_), 476'500'000);
    const std::vector<LedgerView> ledger = listLedger(db_);
    ASSERT_EQ(ledger.size(), 2u);
    EXPECT_EQ(ledger[0].text, "Lease N738SX, 22 Nov – 21 Dec");
    EXPECT_EQ(ledger[0].postedAt, "2026-11-22T00:00:00Z");
}

TEST_F(Leasing, FinNumberIsTheTailAndFlightNumberIsKept) {
    LeaseRequest r = bbj("N814SX", "1_1", d(2026, 11, 22));
    r.flightNumber = "8";
    lease(r);
    EXPECT_EQ(listAirplanes(db_)[0].tailNumber, "1001");
    EXPECT_EQ(count("SELECT CAST(free_flight_number AS INTEGER) FROM airplane"), 8);
    EXPECT_EQ(count("SELECT max_lb FROM airplane_hold WHERE hold = 'forward'"), 7842);
    EXPECT_EQ(count("SELECT max_lb FROM airplane_hold WHERE hold = 'aft'"), 9803);
}

TEST_F(Leasing, InstantDeliveryPostsItsLines) {
    LeaseRequest r = b738("N738SX", "1_1", d(2026, 11, 22));
    r.instantDeliveryIcao = "KSAN";
    lease(r);
    const AirplaneView a = listAirplanes(db_)[0];
    EXPECT_EQ(a.locationIcao, "KSAN");
    EXPECT_FALSE(a.awaitingDelivery);
    EXPECT_EQ(a.fuelOnBoardLb, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM instant_delivery"), 1);

    const auto distance = airportDistanceNm(db_, "KBFI", "KSAN");
    ASSERT_TRUE(distance.has_value());
    const Preview price = instantDeliveryPrice(*catalog_.type("737-800"), *distance, 146300, rates_);
    EXPECT_EQ(count("SELECT COUNT(*) FROM instant_delivery_line"), static_cast<std::int64_t>(price.costs.size()));
    EXPECT_EQ(count("SELECT SUM(l.amount_cents) FROM ledger_line l JOIN instant_delivery_line USING (ledger_line_id)"),
              price.totalCents);
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line WHERE category = 'delivery_fee'"), 1);
    EXPECT_EQ(cashCents(db_), 500'000'000 - 23'500'000 + price.totalCents);
    bool feeLine = false;
    for (const LedgerView& line : listLedger(db_)) {
        feeLine = feeLine || line.text == "N738SX instant delivery KBFI–KSAN · Delivery fee";
    }
    EXPECT_TRUE(feeLine);
}

// Review Focus 4: delivering to the factory itself costs nothing and the plane is delivered there.
TEST_F(Leasing, InstantDeliveryToFactoryIsFree) {
    LeaseRequest r = b738("N738SX", "1_1", d(2026, 11, 22));
    r.instantDeliveryIcao = "KBFI";
    lease(r);
    const AirplaneView a = listAirplanes(db_)[0];
    EXPECT_EQ(a.locationIcao, "KBFI");
    EXPECT_FALSE(a.awaitingDelivery);
    EXPECT_EQ(count("SELECT COUNT(*) FROM instant_delivery"), 0);
    EXPECT_EQ(cashCents(db_), 476'500'000);
}

TEST_F(Leasing, RefusalsWriteNothing) {
    lease(b738("N738SX", "1_1", d(2026, 11, 22)));
    const auto airplanes = count("SELECT COUNT(*) FROM airplane");
    const auto lines = count("SELECT COUNT(*) FROM ledger_line");
    const auto refused = [&](const LeaseRequest& r, std::optional<int> slots, const std::string& expected) {
        EXPECT_EQ(leaseRefusal(db_, r, rates_, slots), expected);
        const LeaseResult result = leaseAirplane(db_, r, rates_, slots);
        EXPECT_FALSE(result.airplaneId.has_value()) << expected;
        EXPECT_EQ(result.refusal, expected);
        EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), airplanes);
        EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line"), lines);
    };
    refused(b738("N738TT", "1_2", d(2026, 11, 22)), 1,
            "All 1 slots are in use. Slot 2 comes at 0 hours flown (you have 0.0).");
    LeaseRequest seats = b738("N738TT", "1_2", d(2026, 11, 22));
    seats.reading.maxPassengers = 184;
    refused(seats, 10, "SimBrief's Max Passengers is 184, but 737-800 Two-class has 160 seats.");
    refused(b738("N738SX", "1_2", d(2026, 11, 22)), 10, "N738SX is already in the fleet.");
    refused(b738("N738TT", "1_1", d(2026, 11, 22)), 10, "SimBrief airframe 1_1 already belongs to N738SX.");
    LeaseRequest missing = b738("N738TT", "1_2", d(2026, 11, 22));
    missing.reading.mlwLb.reset();
    refused(missing, 10, "Not read yet: Max Landing Weight.");
    LeaseRequest base = b738("N738TT", "1_2", d(2026, 11, 22));
    base.reading.baseType = "B77W";
    refused(base, 10, "SimBrief's base type B77W isn't a 737-800.");
    LeaseRequest unknown = b738("N738TT", "1_2", d(2026, 11, 22));
    unknown.instantDeliveryIcao = "KXXX";
    refused(unknown, 10, "KXXX isn't in the airport data.");
    LeaseRequest shortField = b738("N738TT", "1_2", d(2026, 11, 22));
    shortField.instantDeliveryIcao = "KSHT";
    refused(shortField, 10, "KSHT's longest runway is 5,000 ft; N738TT needs 7,000 ft.");
    EXPECT_EQ(leaseRefusal(db_, b738("N738TT", "1_2", d(2026, 11, 22)), rates_, 10), "");  // and a good one passes
}

// Review Focus 5: the cash is checked again inside the lease's own transaction.
TEST_F(Leasing, CashOneCentShortIsRefused) {
    ASSERT_TRUE(db_.exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                         " VALUES ('lease', '2026-11-21T00:00:00Z', -476500500)"));  // $234,995 left
    const LeaseResult result = leaseAirplane(db_, b738("N738SX", "1_1", d(2026, 11, 22)), rates_, slots_);
    EXPECT_FALSE(result.airplaneId.has_value());
    EXPECT_EQ(result.refusal, "Not enough cash: $235,000 needed, $234,995 in cash.");
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 0);
}

TEST_F(Leasing, DuplicateFlightNumberIsRefused) {
    LeaseRequest first = bbj("N814SX", "1_1", d(2026, 11, 22));
    first.flightNumber = "8";
    lease(first);
    LeaseRequest second = b738("N738SX", "1_2", d(2026, 11, 22));
    second.flightNumber = "8";
    EXPECT_EQ(leaseAirplane(db_, second, rates_, slots_).refusal, "Flight number 8 belongs to N814SX.");
}

TEST_F(Leasing, ReturnedRegistrationCanBeLeasedAgain) {
    lease(b738("N738SX", "1_1", d(2026, 11, 22)));
    ASSERT_TRUE(db_.exec("UPDATE airplane SET returned_on = '2026-11-25', return_reason = 'returned'"));
    lease(b738("N738SX", "1_1", d(2026, 11, 26)));
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane WHERE registration = 'N738SX'"), 2);
}

TEST_F(Leasing, SettlingTwiceChangesNothing) {
    lease(bbj("N738BT", "1_1", d(2026, 11, 22)));
    settle(d(2026, 11, 22));
    const LeaseOutcome again = settle(d(2026, 11, 22));
    EXPECT_EQ(again.monthsCharged, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line"), 2);
}

TEST_F(Leasing, NextMonthIsChargedOnItsDay) {
    lease(bbj("N738BT", "1_1", d(2026, 11, 22)));
    EXPECT_EQ(settle(d(2026, 12, 21)).monthsCharged, 0);
    EXPECT_EQ(settle(d(2026, 12, 22)).monthsCharged, 1);
    EXPECT_EQ(cashCents(db_), 470'000'000);
    EXPECT_EQ(listLedger(db_)[0].text, "Lease N738BT, 22 Dec – 21 Jan");
}

TEST_F(Leasing, MonthsMissedWhileClosedAreChargedInDateOrder) {
    // Cash $350,000: A leased 22 Nov ($150,000), B leased 5 Dec ($150,000), leaving $50,000.
    ASSERT_TRUE(db_.exec("UPDATE ledger_line SET amount_cents = 35000000 WHERE category = 'opening_cash'"));
    lease(bbj("N738AA", "1_1", d(2026, 11, 22)));
    lease(bbj("N738BB", "1_2", d(2026, 12, 5)));
    const LeaseOutcome later = settle(d(2027, 1, 10));
    EXPECT_EQ(later.returned, 2);  // A on 22 Dec, B on 5 Jan
    EXPECT_EQ(later.monthsCharged, 0);
    ASSERT_EQ(later.notices.size(), 2u);
    EXPECT_EQ(later.notices[0], "N738AA returned on 22 Dec 2026: the lease month couldn't be paid.");
    EXPECT_EQ(later.notices[1], "N738BB returned on 5 Jan 2027: the lease month couldn't be paid.");
    EXPECT_EQ(cashCents(db_), 5'000'000);
    EXPECT_EQ(settle(d(2027, 3, 1)).monthsCharged, 0);  // a returned airplane is never charged again
}

// A cash read that fails isn't $0: returning an airplane can't be undone, so the run stops instead (review 1.4).
TEST_F(Leasing, ReadErrorReturnsNothing) {
    lease(bbj("N738BT", "1_1", d(2026, 11, 22)));
    ASSERT_TRUE(db_.exec("ALTER TABLE ledger_line RENAME TO ledger_line_hidden"));  // every cash read now fails
    const LeaseOutcome outcome = settleLeases(db_, d(2026, 12, 22));
    EXPECT_NE(outcome.error, "");
    EXPECT_EQ(outcome.returned, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane WHERE returned_on IS NOT NULL"), 0);
}

TEST_F(Leasing, TransactionThatCantStartWritesNothing) {
    ASSERT_TRUE(db_.exec("BEGIN"));  // an open transaction makes the lease's own BEGIN fail
    const LeaseResult result = leaseAirplane(db_, bbj("N738BT", "1_1", d(2026, 11, 22)), rates_, slots_);
    ASSERT_TRUE(db_.exec("COMMIT"));
    EXPECT_NE(result.refusal, "");
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 0);
}

TEST_F(Leasing, TypesRefreshFromCatalog) {
    lease(bbj("N738BT", "1_1", d(2026, 11, 22)));
    for (CatalogType& t : catalog_.types) {
        if (t.code == "737-800 BBJ2") {
            t.turnaroundMin = 40;
            t.monthlyLeaseCents = 20'000'000;
        }
    }
    std::string error;
    ASSERT_TRUE(syncCatalogTypes(db_, catalog_, &error)) << error;
    EXPECT_EQ(count("SELECT turnaround_min FROM aircraft_type WHERE code = '737-800 BBJ2'"), 40);
    EXPECT_EQ(count("SELECT COUNT(*) FROM aircraft_type"), 7);
    EXPECT_EQ(count("SELECT monthly_lease_cents FROM airplane"), 15'000'000);  // the leased rate stays (D-082)
}

TEST_F(Leasing, OpeningCashPostedOnce) {
    std::string error;
    ASSERT_TRUE(postOpeningCash(db_, config_, &error)) << error;
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line WHERE category = 'opening_cash'"), 1);
    EXPECT_EQ(count("SELECT posted_at = '2026-11-22T00:00:00Z' FROM ledger_line"), 1);
}

TEST_F(Leasing, TypeSyncWritesSimFolders) {
    db::Statement q(db_, "SELECT sim_folder FROM aircraft_type WHERE code = '737-800 BBJ2'");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.text(0), "PMDG 737-800BBJ2");
}

TEST_F(Leasing, SlotsEarnedFromHoursAndReturnedPlanesDontCount) {
    slots_.reset();  // earned from hours, as in the app
    lease(bbj("N738AA", "1_1", d(2026, 11, 22)));
    lease(bbj("N738BB", "1_2", d(2026, 11, 22)));
    EXPECT_EQ(leaseAirplane(db_, bbj("N738CC", "1_3", d(2026, 11, 22)), rates_, slots_).refusal,
              "All 2 slots are in use. Slot 3 comes at 10 hours flown (you have 0.0).");

    // Ten hours of flights earn slot 3.
    ASSERT_TRUE(db_.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 'a', airplane_id, '1', 'vip', 1, 2, 2, 0, 4100, '2026-11-23T00:00:00Z',"
        " '2026-11-23T01:00:00Z', '2026-11-23T11:00:00Z', 0, 20000, 5000 FROM airplane WHERE registration = 'N738AA'"));
    lease(bbj("N738CC", "1_3", d(2026, 11, 23)));

    ASSERT_TRUE(db_.exec("UPDATE airplane SET returned_on = '2026-11-23', return_reason = 'returned'"
                         " WHERE registration = 'N738BB'"));
    lease(bbj("N738DD", "1_4", d(2026, 11, 23)));
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane WHERE returned_on IS NULL"), 3);
}

TEST_F(Leasing, AirplaneViewCountsFlightsAndHours) {
    lease(bbj("N738BT", "1_1", d(2026, 11, 22)));
    ASSERT_TRUE(db_.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 'a', airplane_id, '1', 'vip', 1, 2, 2, 0, 4100, '2026-11-23T00:00:00Z',"
        " '2026-11-23T01:00:00Z', '2026-11-23T03:30:00Z', 0, 20000, 5000 FROM airplane"));
    const auto planes = listAirplanes(db_);
    ASSERT_EQ(planes.size(), 1u);
    EXPECT_EQ(planes[0].flights, 1);
    EXPECT_DOUBLE_EQ(planes[0].blockHours, 2.5);
}
