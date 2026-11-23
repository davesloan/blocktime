// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airline_db.h"

#include <array>
#include <format>
#include <system_error>

#include "app_settings.h"

namespace blocktime {

namespace {

// Shapes enforced on every time column (D-081).
#define BT_INSTANT "'[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9]Z'"
#define BT_DATE "'[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]'"

// Migration 2: the airline schema of milestone 4, exactly as docs/database.md describes it. Number 1 was the Qt
// test's schema and is never applied (D-086).
constexpr const char* kMigration2 = R"sql(
CREATE TABLE fleet (
  fleet_id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE CHECK (name <> '')
) STRICT;

CREATE TABLE aircraft_type (
  aircraft_type_id INTEGER PRIMARY KEY,
  code TEXT NOT NULL UNIQUE CHECK (code <> ''),
  family TEXT NOT NULL CHECK (family IN ('737', '777')),
  role TEXT NOT NULL CHECK (role IN ('airliner', 'executive', 'freighter')),
  turnaround_min INTEGER NOT NULL CHECK (turnaround_min > 0),
  cruise_speed_kt INTEGER NOT NULL CHECK (cruise_speed_kt > 0)
) STRICT;

CREATE TABLE airport (
  airport_id INTEGER PRIMARY KEY,
  icao_code TEXT NOT NULL UNIQUE CHECK (length(icao_code) = 4 AND icao_code NOT GLOB '*[^A-Z0-9]*'),
  name TEXT NOT NULL,
  latitude REAL NOT NULL CHECK (latitude BETWEEN -90 AND 90),
  longitude REAL NOT NULL CHECK (longitude BETWEEN -180 AND 180),
  elevation_ft INTEGER NOT NULL,
  country_code TEXT NOT NULL CHECK (length(country_code) = 2),
  time_zone TEXT NOT NULL CHECK (time_zone <> ''),
  longest_runway_ft INTEGER NOT NULL CHECK (longest_runway_ft > 0)
) STRICT;

CREATE TABLE airport_import (
  airport_import_id INTEGER PRIMARY KEY,
  imported_at TEXT NOT NULL CHECK (imported_at GLOB )sql" BT_INSTANT R"sql(),
  airport_count INTEGER NOT NULL CHECK (airport_count >= 0)
) STRICT;

CREATE TABLE airplane (
  airplane_id INTEGER PRIMARY KEY,
  registration TEXT NOT NULL UNIQUE CHECK (registration <> '' AND registration NOT GLOB '*[^A-Z0-9-]*'),
  tail_number TEXT CHECK (tail_number IS NULL OR tail_number <> ''),
  aircraft_type_id INTEGER NOT NULL REFERENCES aircraft_type ON DELETE RESTRICT,
  fleet_id INTEGER NOT NULL REFERENCES fleet ON DELETE RESTRICT,
  airframe_name TEXT NOT NULL CHECK (airframe_name <> ''),
  simbrief_airframe_id TEXT NOT NULL UNIQUE CHECK (simbrief_airframe_id <> ''),
  leased_on TEXT NOT NULL CHECK (leased_on GLOB )sql" BT_DATE R"sql(),
  monthly_lease_cents INTEGER NOT NULL CHECK (monthly_lease_cents > 0 AND monthly_lease_cents % 500 = 0),
  location_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  fuel_on_board_lb INTEGER NOT NULL CHECK (fuel_on_board_lb >= 0),
  awaiting_delivery INTEGER NOT NULL CHECK (awaiting_delivery IN (0, 1)),
  returned_on TEXT CHECK (returned_on IS NULL OR returned_on GLOB )sql" BT_DATE R"sql(),
  return_reason TEXT CHECK (return_reason IS NULL OR return_reason IN ('unpaid')),
  CHECK ((returned_on IS NULL) = (return_reason IS NULL))
) STRICT;
CREATE INDEX airplane_aircraft_type ON airplane (aircraft_type_id);
CREATE INDEX airplane_fleet ON airplane (fleet_id);
CREATE INDEX airplane_location ON airplane (location_airport_id);

CREATE TABLE airplane_cabin (
  airplane_id INTEGER NOT NULL REFERENCES airplane ON DELETE CASCADE,
  cabin_class TEXT NOT NULL CHECK (cabin_class IN ('economy', 'business', 'first')),
  seats INTEGER NOT NULL CHECK (seats > 0),
  PRIMARY KEY (airplane_id, cabin_class)
) STRICT;

CREATE TABLE airplane_hold (
  airplane_id INTEGER NOT NULL REFERENCES airplane ON DELETE CASCADE,
  hold TEXT NOT NULL CHECK (hold IN ('forward', 'aft', 'bulk', 'main_deck')),
  max_lb INTEGER NOT NULL CHECK (max_lb > 0),
  ratio INTEGER NOT NULL CHECK (ratio > 0),
  PRIMARY KEY (airplane_id, hold)
) STRICT;

CREATE TABLE ledger_category (
  code TEXT NOT NULL PRIMARY KEY CHECK (code <> ''),
  label TEXT NOT NULL CHECK (label <> '')
) STRICT;
INSERT INTO ledger_category (code, label) VALUES ('opening_cash', 'Opening cash'), ('lease', 'Lease');

CREATE TABLE ledger_line (
  ledger_line_id INTEGER PRIMARY KEY,
  category TEXT NOT NULL REFERENCES ledger_category ON DELETE RESTRICT,
  posted_at TEXT NOT NULL CHECK (posted_at GLOB )sql" BT_INSTANT R"sql(),
  amount_cents INTEGER NOT NULL CHECK (amount_cents <> 0 AND amount_cents % 500 = 0)
) STRICT;
CREATE INDEX ledger_line_category ON ledger_line (category);
CREATE INDEX ledger_line_posted_at ON ledger_line (posted_at);
CREATE UNIQUE INDEX ledger_line_one_opening_cash ON ledger_line (category) WHERE category = 'opening_cash';

CREATE TABLE lease_charge (
  ledger_line_id INTEGER PRIMARY KEY REFERENCES ledger_line ON DELETE RESTRICT,
  airplane_id INTEGER NOT NULL REFERENCES airplane ON DELETE RESTRICT,
  period_start TEXT NOT NULL CHECK (period_start GLOB )sql" BT_DATE R"sql(),
  UNIQUE (airplane_id, period_start)
) STRICT;
)sql";

#undef BT_INSTANT
#undef BT_DATE

struct Migration {
    int version;
    const char* sql;
};

constexpr std::array kMigrations{Migration{2, kMigration2}};

constexpr int kTestSchemaVersion = 1;  // the Qt test's database (D-061)

}  // namespace

bool migrateAirlineDatabase(db::Database& db, std::string* error) {
    // WAL can't be switched inside a transaction, and an in-memory database just answers "memory".
    if (!db.exec("PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL; PRAGMA synchronous = FULL;", error)) return false;

    const int current = db.userVersion();
    if (current > kAirlineSchemaVersion) {
        if (error) *error = std::format("The database is from a newer BlockTime (schema {}).", current);
        return false;
    }
    if (current == kTestSchemaVersion) {
        if (error) *error = "The database holds the Qt test's schema (1), not the airline's.";
        return false;
    }
    for (const Migration& m : kMigrations) {
        if (m.version <= current) continue;
        db::Transaction tx(db);
        std::string message;
        if (!tx.active() || !db.exec(m.sql, &message) ||
            !db.exec(std::format("PRAGMA user_version = {}", m.version), &message) || !tx.commit(&message)) {
            if (error) *error = std::format("Migration {} failed: {}", m.version, message);
            return false;
        }
    }
    if (error) error->clear();
    return true;
}

AirlineDbOpen openAirlineDatabase(const std::filesystem::path& file) {
    AirlineDbOpen result;
    result.db = db::Database::open(file, &result.error);
    if (!result.db) return result;

    if (result.db->userVersion() == kTestSchemaVersion) {
        result.db.reset();  // close it before renaming
        std::error_code ec;
        std::filesystem::rename(file, file.parent_path() / "blocktime.test-schema.db", ec);
        if (ec) {
            result.error = "Couldn't set the Qt test's database aside: " + ec.message();
            return result;
        }
        result.testSchemaSetAside = true;
        result.db = db::Database::open(file, &result.error);
        if (!result.db) return result;
    }
    if (!migrateAirlineDatabase(*result.db, &result.error)) result.db.reset();
    return result;
}

std::filesystem::path defaultAirlineDatabaseFile() { return defaultSettingsFile().parent_path() / "blocktime.db"; }

}  // namespace blocktime
