// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
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
        LeaseOutcome outcome = settleLeases(db_, catalog_, config_, today);
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
};

}  // namespace

TEST_F(Leasing, FirstMonthChargedAtLease) {
    const LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 1);
    EXPECT_EQ(outcome.monthsCharged, 1);
    EXPECT_TRUE(outcome.notices.empty());
    EXPECT_EQ(cashCents(db_), 410'000'000);

    const std::vector<LedgerView> ledger = listLedger(db_);
    ASSERT_EQ(ledger.size(), 2u);
    EXPECT_EQ(ledger[0].text, "Lease N738BT, 22 Nov – 21 Dec");
    EXPECT_EQ(ledger[0].amountCents, -90'000'000);
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
    EXPECT_EQ(count("SELECT monthly_lease_cents FROM airplane"), 90'000'000);
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
    EXPECT_EQ(cashCents(db_), 320'000'000);
    EXPECT_EQ(listLedger(db_)[0].text, "Lease N738BT, 22 Dec – 21 Jan");
}

TEST_F(Leasing, MonthsMissedWhileClosedAreChargedInDateOrder) {
    config_.startingCashCents = 200'000'000;
    config_.airplanes = {airplane("N738AA", d(2026, 11, 22), "1_1"), airplane("N738BB", d(2026, 12, 5), "1_2")};
    settle(d(2026, 11, 22));  // A leased; cash $1.1M

    const LeaseOutcome later = settle(d(2027, 1, 10));
    EXPECT_EQ(later.leased, 1);    // B on 5 Dec, cash $200k after
    EXPECT_EQ(later.returned, 2);  // A on 22 Dec, B on 5 Jan
    EXPECT_EQ(later.monthsCharged, 1);
    EXPECT_EQ(later.notices.size(), 2u);
    EXPECT_EQ(cashCents(db_), 20'000'000);

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

TEST_F(Leasing, CantAffordFirstMonthLeasesNothing) {
    config_.startingCashCents = 50'000'000;
    const LeaseOutcome outcome = settle(d(2026, 11, 22));
    EXPECT_EQ(outcome.leased, 0);
    EXPECT_EQ(count("SELECT COUNT(*) FROM airplane"), 0);
    ASSERT_EQ(outcome.notices.size(), 1u);
    EXPECT_EQ(outcome.notices[0], "Can't afford N738BT's first month: $900,000 needed, $500,000 in cash on 22 Nov 2026.");
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
            t.monthlyLeaseCents = 95'000'000;
        }
    }
    settle(d(2026, 11, 22));
    EXPECT_EQ(count("SELECT turnaround_min FROM aircraft_type WHERE code = '737-800 BBJ2'"), 40);
    EXPECT_EQ(count("SELECT COUNT(*) FROM aircraft_type"), 7);
    EXPECT_EQ(count("SELECT monthly_lease_cents FROM airplane"), 90'000'000);
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
