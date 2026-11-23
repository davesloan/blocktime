// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "airline_db.h"
#include "sqlite_db.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

db::Database migrated() {
    db::Database d = db::Database::openInMemory();
    std::string error;
    EXPECT_TRUE(migrateAirlineDatabase(d, &error)) << error;
    return d;
}

// One airport, type, fleet and airplane, so rows that point at them can be inserted.
void addParents(db::Database& d) {
    ASSERT_TRUE(d.exec(
        "INSERT INTO airport (airport_id, icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
        "  longest_runway_ft) VALUES (1, 'KBFI', 'Boeing Field', 47.53, -122.30, 21, 'US', 'America/Los_Angeles', 10007);"
        "INSERT INTO aircraft_type (aircraft_type_id, code, family, role, turnaround_min, cruise_speed_kt)"
        "  VALUES (1, '737-800 BBJ2', '737', 'executive', 35, 450);"
        "INSERT INTO fleet (fleet_id, name) VALUES (1, 'Executive');"
        "INSERT INTO airplane (airplane_id, registration, aircraft_type_id, fleet_id, airframe_name,"
        "  simbrief_airframe_id, leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb,"
        "  awaiting_delivery) VALUES (1, 'N814SX', 1, 1, '737-800 BBJ2 VIP', '730097_1', '2026-11-22', 90000000, 1, 0, 1);"));
}

bool hasTable(db::Database& d, const char* name) {
    db::Statement q(d, "SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = ?");
    q.bind(1, std::string_view(name));
    return q.next();
}

std::string queryPlan(db::Database& d, const std::string& sql) {
    db::Statement q(d, "EXPLAIN QUERY PLAN " + sql);
    std::string plan;
    while (q.next()) plan += q.text(3) + "\n";
    EXPECT_EQ(q.error(), "");
    return plan;
}

class AirlineDbFiles : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }
    fs::path dir_;
};

}  // namespace

TEST(AirlineDb, EmptyDatabaseMigratesToVersion4) {
    db::Database d = db::Database::openInMemory();
    std::string error;
    EXPECT_TRUE(migrateAirlineDatabase(d, &error)) << error;
    EXPECT_EQ(d.userVersion(), 4);
    EXPECT_EQ(kAirlineSchemaVersion, 4);
}

TEST(AirlineDb, ForeignKeysAreOn) {
    db::Database d = migrated();
    db::Statement q(d, "PRAGMA foreign_keys");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 1);
}

TEST(AirlineDb, MigratingTwiceIsANoOp) {
    db::Database d = migrated();
    std::string error;
    EXPECT_TRUE(migrateAirlineDatabase(d, &error)) << error;
    EXPECT_EQ(d.userVersion(), 4);
}

TEST(AirlineDb, NewerSchemaIsRefused) {  // one above kAirlineSchemaVersion
    db::Database d = db::Database::openInMemory();
    ASSERT_TRUE(d.exec("PRAGMA user_version = " + std::to_string(kAirlineSchemaVersion + 1)));
    std::string error;
    EXPECT_FALSE(migrateAirlineDatabase(d, &error));
    EXPECT_NE(error.find("newer BlockTime"), std::string::npos) << error;
}

TEST_F(AirlineDbFiles, TestSchemaIsSetAside) {
    const fs::path file = dir_ / "blocktime.db";
    {
        std::string error;
        auto old = db::Database::open(file, &error);
        ASSERT_TRUE(old.has_value()) << error;
        ASSERT_TRUE(old->exec("CREATE TABLE airframes (registration TEXT PRIMARY KEY); PRAGMA user_version = 1;"));
    }
    AirlineDbOpen opened = openAirlineDatabase(file);
    ASSERT_TRUE(opened.db.has_value()) << opened.error;
    EXPECT_TRUE(opened.testSchemaSetAside);
    EXPECT_EQ(opened.db->userVersion(), 4);

    const fs::path aside = dir_ / "blocktime.test-schema.db";
    ASSERT_TRUE(fs::exists(aside));
    std::string error;
    auto old = db::Database::open(aside, &error);
    ASSERT_TRUE(old.has_value()) << error;
    db::Statement q(*old, "SELECT COUNT(*) FROM sqlite_schema WHERE name = 'airframes'");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 1);
}

TEST_F(AirlineDbFiles, NewFileIsNotSetAside) {
    AirlineDbOpen opened = openAirlineDatabase(dir_ / "blocktime.db");
    ASSERT_TRUE(opened.db.has_value()) << opened.error;
    EXPECT_FALSE(opened.testSchemaSetAside);
    EXPECT_EQ(opened.db->userVersion(), 4);
}

TEST(AirlineDb, ChecksRejectBadRows) {
    db::Database d = migrated();
    addParents(d);
    // Each statement breaks exactly one rule.
    EXPECT_FALSE(d.exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                        " VALUES ('lease', '2026-11-22T00:00:00Z', 250)"));
    EXPECT_FALSE(d.exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                        " VALUES ('lease', '2026-11-22 10:00:00', -500)"));
    EXPECT_FALSE(d.exec("INSERT INTO aircraft_type (code, family, role, turnaround_min, cruise_speed_kt)"
                        " VALUES ('747-400', '747', 'airliner', 60, 490)"));
    EXPECT_FALSE(d.exec("UPDATE airplane SET return_reason = 'unpaid' WHERE airplane_id = 1"));
    ASSERT_TRUE(d.exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                       " VALUES ('opening_cash', '2026-11-22T00:00:00Z', 500000000)"));
    EXPECT_FALSE(d.exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                        " VALUES ('opening_cash', '2026-11-23T00:00:00Z', 500000000)"));
    // And a valid row of each kind goes in, so the failures above are the rules, not a broken table.
    EXPECT_TRUE(d.exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                       " VALUES ('lease', '2026-11-22T00:00:00Z', -90000000)"));
    EXPECT_TRUE(d.exec("UPDATE airplane SET returned_on = '2026-12-22', return_reason = 'unpaid' WHERE airplane_id = 1"));
}

TEST(AirlineDb, LeaseMonthCannotBeChargedTwice) {
    db::Database d = migrated();
    addParents(d);
    ASSERT_TRUE(d.exec("INSERT INTO ledger_line (ledger_line_id, category, posted_at, amount_cents) VALUES"
                       " (1, 'lease', '2026-11-22T00:00:00Z', -90000000),"
                       " (2, 'lease', '2026-11-22T00:00:00Z', -90000000);"
                       "INSERT INTO lease_charge (ledger_line_id, airplane_id, period_start) VALUES (1, 1, '2026-11-22');"));
    EXPECT_FALSE(d.exec("INSERT INTO lease_charge (ledger_line_id, airplane_id, period_start) VALUES (2, 1, '2026-11-22')"));
}

TEST(AirlineDb, QueryPlansUseIndexes) {
    db::Database d = migrated();
    for (const std::string sql : {
             "SELECT airplane_id FROM airplane WHERE location_airport_id = 1",
             "SELECT ledger_line_id FROM ledger_line ORDER BY posted_at DESC",
             "SELECT SUM(amount_cents) FROM ledger_line WHERE posted_at <= '2026-11-22T00:00:00Z'",
             "SELECT 1 FROM lease_charge WHERE airplane_id = 1 AND period_start = '2026-11-22'",
             "SELECT planned_flight_id FROM planned_flight WHERE destination_airport_id = 1",
             "SELECT planned_flight_id FROM planned_flight WHERE origin_airport_id = 1",
             "SELECT flight_id FROM flight WHERE airplane_id = 1",
             "SELECT flight_id FROM flight WHERE origin_airport_id = 1",
             "SELECT flight_id FROM flight WHERE planned_destination_airport_id = 1",
             "SELECT flight_id FROM flight WHERE arrival_airport_id = 1",
             "SELECT flight_id FROM flight ORDER BY block_in_at DESC",
             "SELECT ledger_line_id FROM flight_money_line WHERE flight_id = 1",
             "SELECT passengers FROM flight_cabin WHERE flight_id = 1",
         }) {
        const std::string plan = queryPlan(d, sql);
        EXPECT_NE(plan.find("USING"), std::string::npos) << sql << "\n" << plan;
        EXPECT_NE(plan.find("INDEX"), std::string::npos) << sql << "\n" << plan;
        EXPECT_EQ(plan.find("TEMP B-TREE"), std::string::npos) << sql << "\n" << plan;
    }
}

TEST_F(AirlineDbFiles, BusyTimeoutIsSet) {
    std::string error;
    auto d = db::Database::open(dir_ / "x.db", &error);
    ASSERT_TRUE(d.has_value()) << error;
    db::Statement q(*d, "PRAGMA busy_timeout");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 5000);
}

TEST_F(AirlineDbFiles, ClobberedVersionOfARealAirlineIsRestored) {
    const fs::path file = dir_ / "blocktime.db";
    {
        // Only BlockTime 0.4 clobbered the version, and only schema-2 files existed then.
        std::string error;
        auto opened = db::Database::open(file, &error);
        ASSERT_TRUE(opened.has_value()) << error;
        ASSERT_TRUE(migrateAirlineDatabaseTo(*opened, 2, &error)) << error;
        ASSERT_TRUE(opened->exec("INSERT INTO fleet (name) VALUES ('Executive')"));
        // What BlockTime 0.4 and older did to any blocktime.db it opened (D-061's FleetStore).
        ASSERT_TRUE(opened->exec("CREATE TABLE IF NOT EXISTS airframes (registration TEXT PRIMARY KEY);"
                                    "PRAGMA user_version = 1;"));
    }
    AirlineDbOpen again = openAirlineDatabase(file);
    ASSERT_TRUE(again.db.has_value()) << again.error;
    EXPECT_FALSE(again.testSchemaSetAside);
    EXPECT_TRUE(again.versionRestored);
    EXPECT_EQ(again.db->userVersion(), 4);
    EXPECT_FALSE(fs::exists(dir_ / "blocktime.test-schema.db"));
    db::Statement q(*again.db, "SELECT COUNT(*) FROM fleet");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 1);
}

TEST(AirlineDb, Version2DatabaseMigratesTo3) {
    db::Database d = db::Database::openInMemory();
    std::string error;
    ASSERT_TRUE(migrateAirlineDatabaseTo(d, 2, &error)) << error;
    ASSERT_EQ(d.userVersion(), 2);
    EXPECT_FALSE(hasTable(d, "airplane_limits"));
    ASSERT_TRUE(migrateAirlineDatabaseTo(d, 3, &error)) << error;
    EXPECT_EQ(d.userVersion(), 3);
    EXPECT_TRUE(hasTable(d, "airplane_limits"));
}

TEST_F(AirlineDbFiles, RestoredVersionStillGetsMigrations3And4) {
    const fs::path file = dir_ / "blocktime.db";
    {
        std::string error;
        auto d = db::Database::open(file, &error);
        ASSERT_TRUE(d.has_value()) << error;
        ASSERT_TRUE(migrateAirlineDatabaseTo(*d, 2, &error)) << error;
        // A milestone 4 airline whose version BlockTime 0.4 set back to 1.
        ASSERT_TRUE(d->exec("PRAGMA user_version = 1"));
    }
    AirlineDbOpen opened = openAirlineDatabase(file);
    ASSERT_TRUE(opened.db.has_value()) << opened.error;
    EXPECT_TRUE(opened.versionRestored);
    EXPECT_EQ(opened.db->userVersion(), 4);
    EXPECT_TRUE(hasTable(*opened.db, "planned_flight"));
    EXPECT_TRUE(hasTable(*opened.db, "flight"));
}

TEST(AirlineDb, PlannedFlightChecks) {
    db::Database d = migrated();
    addParents(d);
    ASSERT_TRUE(d.exec(
        "INSERT INTO airport (airport_id, icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
        "  longest_runway_ft) VALUES (2, 'KSAN', 'San Diego', 32.73, -117.19, 17, 'US', 'America/Los_Angeles', 9401)"));
    const auto insert = [&](const std::string& kind, int cargo, int dest, const std::string& number) {
        return d.exec("INSERT INTO planned_flight (airplane_id, origin_airport_id, destination_airport_id, flight_number,"
                      " kind, cargo_lb, fuel_price_cents_per_100lb, planned_at) VALUES (1, 1, " +
                      std::to_string(dest) + ", '" + number + "', '" + kind + "', " + std::to_string(cargo) +
                      ", 4100, '2026-11-23T14:00:00Z')");
    };
    EXPECT_FALSE(insert("vip", 500, 2, "101"));
    EXPECT_FALSE(insert("vip", 0, 1, "101"));
    EXPECT_FALSE(insert("vip", 0, 2, "A1"));
    EXPECT_FALSE(insert("vip", 0, 2, "10101"));
    EXPECT_TRUE(insert("vip", 0, 2, "101"));
    EXPECT_FALSE(insert("vip", 0, 2, "102"));  // a second planned flight for the same airplane
}

TEST(AirlineDb, FreeFlightNumberIsUniqueDigits) {
    db::Database d = migrated();
    addParents(d);
    ASSERT_TRUE(d.exec(
        "INSERT INTO airplane (airplane_id, registration, aircraft_type_id, fleet_id, airframe_name,"
        "  simbrief_airframe_id, leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb,"
        "  awaiting_delivery) VALUES (2, 'N815SX', 1, 1, '737-800 BBJ2 VIP', '730097_2', '2026-11-22', 90000000, 1, 0, 1)"));
    EXPECT_FALSE(d.exec("UPDATE airplane SET free_flight_number = 'A1' WHERE airplane_id = 1"));
    EXPECT_FALSE(d.exec("UPDATE airplane SET free_flight_number = '12345' WHERE airplane_id = 1"));
    EXPECT_TRUE(d.exec("UPDATE airplane SET free_flight_number = '8' WHERE airplane_id = 1"));
    EXPECT_FALSE(d.exec("UPDATE airplane SET free_flight_number = '8' WHERE airplane_id = 2"));
    EXPECT_TRUE(d.exec("UPDATE airplane SET free_flight_number = '9' WHERE airplane_id = 2"));
}

TEST(AirlineDb, Version3DatabaseMigratesTo4) {
    db::Database d = db::Database::openInMemory();
    std::string error;
    ASSERT_TRUE(migrateAirlineDatabaseTo(d, 3, &error)) << error;
    ASSERT_EQ(d.userVersion(), 3);
    EXPECT_FALSE(hasTable(d, "flight"));
    ASSERT_TRUE(migrateAirlineDatabase(d, &error)) << error;
    EXPECT_EQ(d.userVersion(), 4);
    for (const char* table : {"flight", "flight_cabin", "flight_plan", "flight_money_line"}) {
        EXPECT_TRUE(hasTable(d, table)) << table;
    }
    db::Statement q(d, "SELECT sim_folder FROM aircraft_type");
    EXPECT_EQ(q.error(), "");
}

namespace {

// A second airport and one settled flight's worth of parents (KBFI 1, KSAN 2).
void addFlightParents(db::Database& d) {
    addParents(d);
    ASSERT_TRUE(d.exec(
        "INSERT INTO airport (airport_id, icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
        "  longest_runway_ft) VALUES (2, 'KSAN', 'San Diego', 32.73, -117.19, 17, 'US', 'America/Los_Angeles', 9401)"));
}

bool insertFlight(db::Database& d, const std::string& logId, const std::string& kind, const std::string& out,
                  const std::string& in, int fuelOut) {
    return d.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb) VALUES ('" +
        logId + "', 1, '8', '" + kind + "', 1, 2, 2, 0, 4100, '2026-11-23T10:00:00Z', '" + out + "', '" + in +
        "', 0, " + std::to_string(fuelOut) + ", 9400)");
}

}  // namespace

TEST(AirlineDb, FlightChecks) {
    db::Database d = migrated();
    addFlightParents(d);
    const std::string out = "2026-11-23T10:30:05Z", in = "2026-11-23T13:20:41Z";
    EXPECT_FALSE(insertFlight(d, "a", "delivery", in, in, 20000));     // block in not after block out
    EXPECT_FALSE(insertFlight(d, "b", "delivery", in, out, 20000));
    EXPECT_FALSE(insertFlight(d, "c", "delivery", out, in, -1));       // negative fuel
    EXPECT_FALSE(insertFlight(d, "d", "ferry", out, in, 20000));       // not a kind
    EXPECT_FALSE(insertFlight(d, "", "delivery", out, in, 20000));     // no log id
    EXPECT_FALSE(insertFlight(d, "e", "delivery", "2026-11-23 10:30", in, 20000));  // not an instant
    EXPECT_TRUE(insertFlight(d, "2026-11-23T10:30:05.120Z", "delivery", out, in, 20000));
    EXPECT_FALSE(insertFlight(d, "2026-11-23T10:30:05.120Z", "delivery", out, in, 20000));  // the same log flight
    ASSERT_TRUE(d.exec("INSERT INTO ledger_line (ledger_line_id, category, posted_at, amount_cents)"
                       " VALUES (1, 'fuel', '2026-11-23T13:20:41Z', -820000)"));
    EXPECT_FALSE(d.exec("INSERT INTO flight_money_line (ledger_line_id, flight_id) VALUES (1, 99)"));
    EXPECT_TRUE(d.exec("INSERT INTO flight_money_line (ledger_line_id, flight_id) VALUES (1, 1)"));
    EXPECT_FALSE(d.exec("DELETE FROM flight WHERE flight_id = 1"));  // its money points at it
}

TEST(AirlineDb, FlightLedgerCategoriesExist) {
    db::Database d = migrated();
    for (const char* code : {"economy_fares", "business_fares", "first_fares", "vip_fares", "charter", "cargo", "fuel",
                             "landing_fee", "gate", "handling", "ga_services", "atc", "catering", "cargo_handling",
                             "pilots", "flight_attendants", "maintenance"}) {
        db::Statement q(d, "SELECT label FROM ledger_category WHERE code = ?");
        q.bind(1, std::string_view(code));
        EXPECT_TRUE(q.next()) << code;
    }
}
