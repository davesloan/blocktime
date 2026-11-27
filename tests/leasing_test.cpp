// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "airline_config.h"
#include "airline_db.h"
#include "airline_queries.h"
#include "catalog.h"
#include "leasing.h"
#include "sqlite_db.h"

using namespace blocktime;
using namespace std::chrono;

namespace {

constexpr Date d(int y, unsigned m, unsigned day) { return Date{year{y}, month{m}, std::chrono::day{day}}; }

ConfigAirplane airplane(const std::string& registration, Date leasedOn, const std::string& simbrief) {
    ConfigAirplane a;
    a.registration = registration;
    a.airframe = "737-800 BBJ2 VIP";
    a.fleet = "Executive";
    a.simbriefAirframeId = simbrief;
    a.leasedOn = leasedOn;
    a.locationIcao = "KBFI";
    a.awaitingDelivery = true;
    return a;
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
            " ('KSAN', 'San Diego', 32.73, -117.19, 17, 'US', 'America/Los_Angeles', 9401)"));
        auto catalog = loadCatalog(BLOCKTIME_SOURCE_DIR "/config/catalog.yaml", &error);
        ASSERT_TRUE(catalog.has_value()) << error;
        catalog_ = *catalog;
        config_.identity = {"BlockTime Air", "BTA", "BLOCKTIME"};
        config_.foundedOn = d(2026, 11, 22);
        config_.startingCashCents = 500'000'000;
        config_.fleets = {"Executive"};
        config_.airplanes = {airplane("N738BT", d(2026, 11, 22), "1_1")};
    }

    // Everything AirlineSession does before settling.
    void sync() {
        std::string error;
        ASSERT_TRUE(syncCatalogTypes(db_, catalog_, &error)) << error;
        ASSERT_TRUE(syncFleets(db_, config_, &error)) << error;
        ASSERT_TRUE(postOpeningCash(db_, config_, &error)) << error;
    }

    LeaseOutcome settle(Date today) {
        sync();
        LeaseOutcome outcome = settleLeases(db_, catalog_, config_, today, slots_);
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
    std::optional<int> slots_ = 10;  // the lease tests aren't about slots; the slot tests clear it
};

}  // namespace

TEST_F(Leasing, FirstMonthChargedAtLease) {
    const LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 1);
    EXPECT_EQ(outcome.monthsCharged, 1);
    EXPECT_TRUE(outcome.notices.empty());
    EXPECT_EQ(cashCents(db_), 485'000'000);

    const std::vector<LedgerView> ledger = listLedger(db_);
    ASSERT_EQ(ledger.size(), 2u);
    EXPECT_EQ(ledger[0].text, "Lease N738BT, 22 Nov – 21 Dec");
    EXPECT_EQ(ledger[0].amountCents, -15'000'000);
    EXPECT_EQ(ledger[0].postedAt, "2026-11-22T00:00:00Z");
    EXPECT_EQ(ledger[1].text, "Opening cash");
    EXPECT_EQ(ledger[1].amountCents, 500'000'000);
}

TEST_F(Leasing, AirplaneCopiesItsAirframe) {
    config_.airplanes[0].tailNumber = "1001";
    settle(d(2026, 11, 22));
    const std::vector<AirplaneView> airplanes = listAirplanes(db_);
    ASSERT_EQ(airplanes.size(), 1u);
    const AirplaneView& a = airplanes[0];
    EXPECT_EQ(a.registration, "N738BT");
    EXPECT_EQ(a.tailNumber, "1001");
    EXPECT_EQ(a.typeCode, "737-800 BBJ2");
    EXPECT_EQ(a.airframeName, "737-800 BBJ2 VIP");
    EXPECT_EQ(a.fleet, "Executive");
    EXPECT_EQ(a.locationIcao, "KBFI");
    EXPECT_EQ(a.fuelOnBoardLb, 0);
    EXPECT_TRUE(a.awaitingDelivery);
    EXPECT_FALSE(a.returnedOn.has_value());
    EXPECT_EQ(count("SELECT seats FROM airplane_cabin WHERE cabin_class = 'economy'"), 48);
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane_hold"), 2);
    EXPECT_EQ(count("SELECT monthly_lease_cents FROM airplane"), 15'000'000);
}

TEST_F(Leasing, SettlingTwiceChangesNothing) {
    settle(d(2026, 11, 22));
    const LeaseOutcome again = settle(d(2026, 11, 22));
    EXPECT_EQ(again.leased, 0);
    EXPECT_EQ(again.monthsCharged, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line"), 2);
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 1);
}

TEST_F(Leasing, NextMonthIsChargedOnItsDay) {
    settle(d(2026, 11, 22));
    EXPECT_EQ(settle(d(2026, 12, 21)).monthsCharged, 0);
    EXPECT_EQ(settle(d(2026, 12, 22)).monthsCharged, 1);
    EXPECT_EQ(cashCents(db_), 470'000'000);
    EXPECT_EQ(listLedger(db_)[0].text, "Lease N738BT, 22 Dec – 21 Jan");
}

TEST_F(Leasing, MonthsMissedWhileClosedAreChargedInDateOrder) {
    config_.startingCashCents = 35'000'000;
    config_.airplanes = {airplane("N738AA", d(2026, 11, 22), "1_1"), airplane("N738BB", d(2026, 12, 5), "1_2")};
    settle(d(2026, 11, 22));  // A leased at $150,000; cash $200k

    const LeaseOutcome later = settle(d(2027, 1, 10));
    EXPECT_EQ(later.leased, 1);    // B on 5 Dec, cash $50k after
    EXPECT_EQ(later.returned, 2);  // A on 22 Dec, B on 5 Jan
    EXPECT_EQ(later.monthsCharged, 1);
    EXPECT_EQ(later.notices.size(), 2u);
    EXPECT_EQ(cashCents(db_), 5'000'000);

    const std::vector<AirplaneView> airplanes = listAirplanes(db_);
    ASSERT_EQ(airplanes.size(), 2u);
    EXPECT_EQ(airplanes[0].returnedOn, d(2026, 12, 22));
    EXPECT_EQ(airplanes[0].returnReason, "unpaid");
    EXPECT_EQ(airplanes[1].returnedOn, d(2027, 1, 5));
    EXPECT_EQ(later.notices[0], "N738AA returned on 22 Dec 2026: the lease month couldn't be paid.");
    EXPECT_EQ(later.notices[1], "N738BB returned on 5 Jan 2027: the lease month couldn't be paid.");

    // B's first month is dated when it fell due, before A's unpaid month.
    const std::vector<LedgerView> ledger = listLedger(db_);
    ASSERT_EQ(ledger.size(), 3u);
    EXPECT_EQ(ledger[0].postedAt, "2026-12-05T00:00:00Z");
    EXPECT_EQ(ledger[0].text, "Lease N738BB, 5 Dec – 4 Jan");

    // A returned airplane is never charged again.
    EXPECT_EQ(settle(d(2027, 3, 1)).monthsCharged, 0);
}

// A cash read that fails isn't $0: returning an airplane can't be undone, so the run stops instead (review 1.4).
TEST_F(Leasing, ReadErrorReturnsNothing) {
    settle(d(2026, 11, 22));
    ASSERT_TRUE(db_.exec("ALTER TABLE ledger_line RENAME TO ledger_line_hidden"));  // every cash read now fails
    const LeaseOutcome outcome = settleLeases(db_, catalog_, config_, d(2026, 12, 22), slots_);
    EXPECT_NE(outcome.error, "");
    EXPECT_EQ(outcome.returned, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane WHERE returned_on IS NOT NULL"), 0);
}

TEST_F(Leasing, CantAffordFirstMonthLeasesNothing) {
    config_.startingCashCents = 10'000'000;
    const LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 0);
    ASSERT_EQ(outcome.notices.size(), 1u);
    EXPECT_EQ(outcome.notices[0], "Can't afford N738BT's first month: $150,000 needed, $100,000 in cash on 22 Nov 2026. "
                                  "Set a later leased_on to try again.");
}

TEST_F(Leasing, FutureLeaseWaitsForItsDay) {
    config_.airplanes[0].leasedOn = d(2026, 12, 1);
    EXPECT_EQ(settle(d(2026, 11, 22)).leased, 0);
    EXPECT_EQ(settle(d(2026, 12, 1)).leased, 1);
    EXPECT_EQ(listLedger(db_)[0].postedAt, "2026-12-01T00:00:00Z");
}

TEST_F(Leasing, UnknownAirportOrAirframeIsANotice) {
    config_.airplanes = {airplane("N738AA", d(2026, 11, 22), "1_1"), airplane("N738BB", d(2026, 11, 22), "1_2")};
    config_.airplanes[0].locationIcao = "KXXX";
    config_.airplanes[1].airframe = "737-800 BBJ";
    const LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 0);
    ASSERT_EQ(outcome.notices.size(), 2u);
    EXPECT_EQ(outcome.notices[0], "N738AA: KXXX isn't in the airport data. Run blocktime-import (README).");
    EXPECT_EQ(outcome.notices[1], "N738BB: no airframe \"737-800 BBJ\" in catalog.yaml.");
}

TEST_F(Leasing, TypesRefreshFromCatalog) {
    settle(d(2026, 11, 22));
    for (CatalogType& t : catalog_.types) {
        if (t.code == "737-800 BBJ2") {
            t.turnaroundMin = 40;
            t.monthlyLeaseCents = 20'000'000;
        }
    }
    settle(d(2026, 11, 22));
    EXPECT_EQ(count("SELECT turnaround_min FROM aircraft_type WHERE code = '737-800 BBJ2'"), 40);
    EXPECT_EQ(count("SELECT COUNT(*) FROM aircraft_type"), 7);
    EXPECT_EQ(count("SELECT monthly_lease_cents FROM airplane"), 15'000'000);  // the leased rate stays (D-082)
}

TEST_F(Leasing, OpeningCashPostedOnce) {
    sync();
    sync();
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line WHERE category = 'opening_cash'"), 1);
    EXPECT_EQ(count("SELECT posted_at = '2026-11-22T00:00:00Z' FROM ledger_line"), 1);
}

TEST_F(Leasing, DriftWarnings) {
    settle(d(2026, 11, 22));
    EXPECT_TRUE(configDriftWarnings(db_, config_).empty());

    config_.airplanes[0].simbriefAirframeId = "1_9";
    std::vector<std::string> warnings = configDriftWarnings(db_, config_);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "N738BT: airline.yaml changed simbrief_airframe_id, but the database keeps the leased values (D-082).");

    config_.airplanes.clear();
    warnings = configDriftWarnings(db_, config_);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0], "N738BT is no longer in airline.yaml; returning a lease comes with the settings screen (D-092).");
}

TEST_F(Leasing, RenamedRegistrationIsANoticeAndOthersStillCharge) {
    config_.airplanes = {airplane("N738BT", d(2026, 11, 22), "1_1"), airplane("N738CC", d(2026, 11, 22), "1_2")};
    settle(d(2026, 11, 22));
    config_.airplanes[0].registration = "N738TB";  // a typo fixed after leasing, same SimBrief airframe
    const LeaseOutcome outcome = settle(d(2026, 12, 22));
    EXPECT_EQ(outcome.error, "");
    EXPECT_EQ(outcome.leased, 0);
    EXPECT_EQ(outcome.monthsCharged, 2);  // N738BT and N738CC's second months
    ASSERT_EQ(outcome.notices.size(), 1u);
    EXPECT_EQ(outcome.notices[0],
              "N738TB: simbrief_airframe_id 1_1 already belongs to N738BT; a leased airplane can't be renamed in "
              "airline.yaml (D-082).");
}

TEST_F(Leasing, TransactionThatCantStartWritesNothing) {
    sync();
    ASSERT_TRUE(db_.exec("BEGIN"));  // an open transaction makes the settlement's own BEGIN fail
    const LeaseOutcome outcome = settleLeases(db_, catalog_, config_, d(2026, 11, 22));
    ASSERT_TRUE(db_.exec("COMMIT"));
    EXPECT_NE(outcome.error, "");
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line WHERE category = 'lease'"), 0);
}

TEST_F(Leasing, LeaseCopiesLimitsWithOverrides) {
    config_.airplanes[0].limits.mlwLb = 140000;
    settle(d(2026, 11, 22));
    const auto id = count("SELECT airplane_id FROM airplane WHERE registration = 'N738BT'");
    const auto limits = airplaneLimits(db_, id);
    ASSERT_TRUE(limits.has_value());
    EXPECT_EQ(limits->minRunwayFt, 7000);
    EXPECT_EQ(limits->mlwLb, 140000);
    EXPECT_EQ(limits->passengerWeightLb, 190);
    EXPECT_EQ(limits->bagWeightLb, 55);
    EXPECT_EQ(limits->maxPayloadLb, 36113);
}

TEST_F(Leasing, AirplaneLeasedBeforeLimitsGetsThem) {
    settle(d(2026, 11, 22));
    const auto id = count("SELECT airplane_id FROM airplane WHERE registration = 'N738BT'");
    ASSERT_TRUE(db_.exec("DELETE FROM airplane_limits"));
    ASSERT_FALSE(airplaneLimits(db_, id).has_value());
    std::string error;
    ASSERT_TRUE(fillMissingLimits(db_, catalog_, config_, &error)) << error;
    const auto limits = airplaneLimits(db_, id);
    ASSERT_TRUE(limits.has_value());
    EXPECT_EQ(limits->mlwLb, 146300);
    EXPECT_EQ(limits->maxPayloadLb, 36113);
}

TEST_F(Leasing, FreeFlightNumberFollowsTheConfig) {
    config_.airplanes[0].flightNumber = "8";
    settle(d(2026, 11, 22));
    std::string error;
    ASSERT_TRUE(syncFreeFlightNumbers(db_, config_, &error)) << error;
    EXPECT_EQ(count("SELECT CAST(free_flight_number AS INTEGER) FROM airplane WHERE registration = 'N738BT'"), 8);
    config_.airplanes[0].flightNumber = "9";
    ASSERT_TRUE(syncFreeFlightNumbers(db_, config_, &error)) << error;
    EXPECT_EQ(count("SELECT CAST(free_flight_number AS INTEGER) FROM airplane WHERE registration = 'N738BT'"), 9);
}

TEST_F(Leasing, TypeSyncWritesSimFolders) {
    sync();
    db::Statement q(db_, "SELECT sim_folder FROM aircraft_type WHERE code = '737-800 BBJ2'");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.text(0), "PMDG 737-800BBJ2");
}

TEST(Slots, FromHours) {
    // Two slots to start; the first earned one still comes at 10 h (D-131).
    EXPECT_EQ(slotsEarned(0), 2);
    EXPECT_EQ(slotsEarned(9.9), 2);
    EXPECT_EQ(slotsEarned(10), 3);
    EXPECT_EQ(slotsEarned(29.9), 3);
    EXPECT_EQ(slotsEarned(30), 4);
    EXPECT_EQ(slotsEarned(150), 7);
    EXPECT_EQ(slotsEarned(279.9), 8);
    EXPECT_EQ(slotsEarned(280), 9);
    EXPECT_EQ(slotsEarned(450), 11);
    EXPECT_DOUBLE_EQ(hoursForSlot(2), 0);
    EXPECT_DOUBLE_EQ(hoursForSlot(3), 10);
    EXPECT_DOUBLE_EQ(hoursForSlot(11), 450);
}

TEST_F(Leasing, NoFreeSlotMeansNoLeaseAndANotice) {
    slots_.reset();  // earned from hours, as in the app
    config_.airplanes = {airplane("N738BT", d(2026, 11, 22), "1_1"), airplane("N738CC", d(2026, 11, 22), "1_2"),
                         airplane("N738DD", d(2026, 11, 22), "1_3")};
    LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 2);
    ASSERT_EQ(outcome.notices.size(), 1u);
    EXPECT_EQ(outcome.notices[0],
              "Can't lease N738DD yet: 2 of 2 slots in use. Slot 3 comes at 10 hours flown (you have 0.0).");
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 2);
    EXPECT_EQ(count("SELECT COUNT(*) FROM ledger_line WHERE category = 'lease'"), 2);

    // Ten hours of flights in N738BT earn slot 3.
    ASSERT_TRUE(db_.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 'a', airplane_id, '1', 'passenger', 1, 2, 2, 0, 4100, '2026-11-23T00:00:00Z',"
        " '2026-11-23T01:00:00Z', '2026-11-23T07:00:00Z', 0, 20000, 5000 FROM airplane WHERE registration = 'N738BT';"
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 'b', airplane_id, '1', 'passenger', 2, 1, 1, 0, 4100, '2026-11-23T00:00:00Z',"
        " '2026-11-23T08:00:00Z', '2026-11-23T12:00:00Z', 5000, 20000, 5000 FROM airplane WHERE registration = 'N738BT'"));
    outcome = settle(d(2026, 11, 23));
    EXPECT_EQ(outcome.leased, 1);
    EXPECT_TRUE(outcome.notices.empty());
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 3);
}

TEST_F(Leasing, ReturnedAirplanesDontTakeASlot) {
    slots_.reset();
    config_.airplanes.push_back(airplane("N738CC", d(2026, 11, 22), "1_2"));
    settle(d(2026, 11, 22));  // both starting slots in use
    ASSERT_TRUE(db_.exec("UPDATE airplane SET returned_on = '2026-11-22', return_reason = 'unpaid'"
                         " WHERE registration = 'N738CC'"));
    config_.airplanes.push_back(airplane("N738DD", d(2026, 11, 22), "1_3"));
    const LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 1);
}

TEST_F(Leasing, AirplaneViewCountsFlightsAndHours) {
    settle(d(2026, 11, 22));
    ASSERT_TRUE(db_.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT 'a', airplane_id, '1', 'passenger', 1, 2, 2, 0, 4100, '2026-11-23T00:00:00Z',"
        " '2026-11-23T01:00:00Z', '2026-11-23T03:30:00Z', 0, 20000, 5000 FROM airplane"));
    const auto planes = listAirplanes(db_);
    ASSERT_EQ(planes.size(), 1u);
    EXPECT_EQ(planes[0].flights, 1);
    EXPECT_DOUBLE_EQ(planes[0].blockHours, 2.5);
}
