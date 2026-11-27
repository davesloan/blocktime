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

// Migration 3: milestone 5's limits and planned flight (docs/database.md, D-100, D-103).
constexpr const char* kMigration3 = R"sql(
CREATE TABLE airplane_limits (
  airplane_id INTEGER PRIMARY KEY REFERENCES airplane ON DELETE CASCADE,
  min_runway_ft INTEGER NOT NULL CHECK (min_runway_ft > 0),
  mlw_lb INTEGER NOT NULL CHECK (mlw_lb > 0),
  passenger_weight_lb INTEGER NOT NULL CHECK (passenger_weight_lb > 0),
  bag_weight_lb INTEGER NOT NULL CHECK (bag_weight_lb >= 0),
  max_payload_lb INTEGER NOT NULL CHECK (max_payload_lb > 0)
) STRICT;

ALTER TABLE airplane ADD COLUMN free_flight_number TEXT
  CHECK (free_flight_number IS NULL OR
         (length(free_flight_number) BETWEEN 1 AND 4 AND free_flight_number NOT GLOB '*[^0-9]*'));
CREATE UNIQUE INDEX airplane_free_flight_number ON airplane (free_flight_number);

CREATE TABLE planned_flight (
  planned_flight_id INTEGER PRIMARY KEY,
  airplane_id INTEGER NOT NULL UNIQUE REFERENCES airplane ON DELETE RESTRICT,
  origin_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  destination_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  flight_number TEXT NOT NULL CHECK (length(flight_number) BETWEEN 1 AND 4 AND flight_number NOT GLOB '*[^0-9]*'),
  kind TEXT NOT NULL CHECK (kind IN ('passenger', 'vip', 'charter', 'cargo', 'delivery')),
  cargo_lb INTEGER NOT NULL CHECK (cargo_lb >= 0),
  fuel_price_cents_per_100lb INTEGER NOT NULL CHECK (fuel_price_cents_per_100lb > 0),
  planned_at TEXT NOT NULL CHECK (planned_at GLOB )sql" BT_INSTANT R"sql(),
  origin_runway TEXT CHECK (origin_runway IS NULL OR origin_runway <> ''),
  destination_runway TEXT CHECK (destination_runway IS NULL OR destination_runway <> ''),
  CHECK (destination_airport_id <> origin_airport_id),
  CHECK (cargo_lb = 0 OR kind IN ('passenger', 'cargo'))
) STRICT;
CREATE INDEX planned_flight_origin ON planned_flight (origin_airport_id);
CREATE INDEX planned_flight_destination ON planned_flight (destination_airport_id);

CREATE TABLE planned_flight_cabin (
  planned_flight_id INTEGER NOT NULL REFERENCES planned_flight ON DELETE CASCADE,
  cabin_class TEXT NOT NULL CHECK (cabin_class IN ('economy', 'business', 'first')),
  passengers INTEGER NOT NULL CHECK (passengers >= 0),
  PRIMARY KEY (planned_flight_id, cabin_class)
) STRICT;

CREATE TABLE simbrief_plan (
  planned_flight_id INTEGER PRIMARY KEY REFERENCES planned_flight ON DELETE CASCADE,
  request_id TEXT NOT NULL CHECK (request_id <> ''),
  generated_at TEXT NOT NULL CHECK (generated_at GLOB )sql" BT_INSTANT R"sql(),
  imported_at TEXT NOT NULL CHECK (imported_at GLOB )sql" BT_INSTANT R"sql(),
  route TEXT NOT NULL,
  origin_runway TEXT NOT NULL,
  destination_runway TEXT NOT NULL,
  cruise_altitude_ft INTEGER NOT NULL CHECK (cruise_altitude_ft > 0),
  est_block_s INTEGER NOT NULL CHECK (est_block_s > 0),
  ramp_fuel_lb INTEGER NOT NULL CHECK (ramp_fuel_lb > 0),
  enroute_burn_lb INTEGER NOT NULL CHECK (enroute_burn_lb > 0),
  est_landing_weight_lb INTEGER NOT NULL CHECK (est_landing_weight_lb > 0),
  origin_transition_altitude_ft INTEGER NOT NULL CHECK (origin_transition_altitude_ft > 0),
  destination_transition_level_ft INTEGER NOT NULL CHECK (destination_transition_level_ft > 0),
  ofp_json TEXT NOT NULL CHECK (ofp_json <> '')
) STRICT;
)sql";

// Migration 4: milestone 6's flights as history, the matching check's sim folder (docs/database.md, D-112, D-115).
constexpr const char* kMigration4 = R"sql(
ALTER TABLE aircraft_type ADD COLUMN sim_folder TEXT NOT NULL DEFAULT '';

CREATE TABLE flight (
  flight_id INTEGER PRIMARY KEY,
  log_flight_id TEXT NOT NULL UNIQUE CHECK (log_flight_id <> ''),
  airplane_id INTEGER NOT NULL REFERENCES airplane ON DELETE RESTRICT,
  flight_number TEXT NOT NULL CHECK (length(flight_number) BETWEEN 1 AND 4 AND flight_number NOT GLOB '*[^0-9]*'),
  kind TEXT NOT NULL CHECK (kind IN ('passenger', 'vip', 'charter', 'cargo', 'delivery')),
  origin_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  planned_destination_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  arrival_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  cargo_lb INTEGER NOT NULL CHECK (cargo_lb >= 0),
  fuel_price_cents_per_100lb INTEGER NOT NULL CHECK (fuel_price_cents_per_100lb > 0),
  planned_at TEXT NOT NULL CHECK (planned_at GLOB )sql" BT_INSTANT R"sql(),
  block_out_at TEXT NOT NULL CHECK (block_out_at GLOB )sql" BT_INSTANT R"sql(),
  block_in_at TEXT NOT NULL CHECK (block_in_at GLOB )sql" BT_INSTANT R"sql(),
  fuel_on_record_lb INTEGER NOT NULL CHECK (fuel_on_record_lb >= 0),
  fuel_out_lb INTEGER NOT NULL CHECK (fuel_out_lb >= 0),
  fuel_in_lb INTEGER NOT NULL CHECK (fuel_in_lb >= 0),
  origin_runway TEXT CHECK (origin_runway IS NULL OR origin_runway <> ''),
  destination_runway TEXT CHECK (destination_runway IS NULL OR destination_runway <> ''),
  CHECK (block_in_at > block_out_at),
  CHECK (cargo_lb = 0 OR kind IN ('passenger', 'cargo'))
) STRICT;
CREATE INDEX flight_airplane ON flight (airplane_id);
CREATE INDEX flight_origin ON flight (origin_airport_id);
CREATE INDEX flight_planned_destination ON flight (planned_destination_airport_id);
CREATE INDEX flight_arrival ON flight (arrival_airport_id);
CREATE INDEX flight_block_in ON flight (block_in_at);

CREATE TABLE flight_cabin (
  flight_id INTEGER NOT NULL REFERENCES flight ON DELETE CASCADE,
  cabin_class TEXT NOT NULL CHECK (cabin_class IN ('economy', 'business', 'first')),
  passengers INTEGER NOT NULL CHECK (passengers >= 0),
  PRIMARY KEY (flight_id, cabin_class)
) STRICT;

CREATE TABLE flight_plan (
  flight_id INTEGER PRIMARY KEY REFERENCES flight ON DELETE RESTRICT,
  request_id TEXT NOT NULL CHECK (request_id <> ''),
  generated_at TEXT NOT NULL CHECK (generated_at GLOB )sql" BT_INSTANT R"sql(),
  imported_at TEXT NOT NULL CHECK (imported_at GLOB )sql" BT_INSTANT R"sql(),
  route TEXT NOT NULL,
  origin_runway TEXT NOT NULL,
  destination_runway TEXT NOT NULL,
  cruise_altitude_ft INTEGER NOT NULL CHECK (cruise_altitude_ft > 0),
  est_block_s INTEGER NOT NULL CHECK (est_block_s > 0),
  ramp_fuel_lb INTEGER NOT NULL CHECK (ramp_fuel_lb > 0),
  enroute_burn_lb INTEGER NOT NULL CHECK (enroute_burn_lb > 0),
  est_landing_weight_lb INTEGER NOT NULL CHECK (est_landing_weight_lb > 0),
  origin_transition_altitude_ft INTEGER NOT NULL CHECK (origin_transition_altitude_ft > 0),
  destination_transition_level_ft INTEGER NOT NULL CHECK (destination_transition_level_ft > 0),
  ofp_json TEXT NOT NULL CHECK (ofp_json <> '')
) STRICT;

CREATE TABLE flight_money_line (
  ledger_line_id INTEGER PRIMARY KEY REFERENCES ledger_line ON DELETE RESTRICT,
  flight_id INTEGER NOT NULL REFERENCES flight ON DELETE RESTRICT
) STRICT;
CREATE INDEX flight_money_line_flight ON flight_money_line (flight_id);

INSERT INTO ledger_category (code, label) VALUES
  ('economy_fares', 'Economy fares'), ('business_fares', 'Business fares'), ('first_fares', 'First fares'),
  ('vip_fares', 'VIP fares'), ('charter', 'Charter'), ('cargo', 'Cargo'), ('fuel', 'Fuel'),
  ('landing_fee', 'Landing fee'), ('gate', 'Departure / gate'), ('handling', 'Ground handling'),
  ('ga_services', 'GA services'), ('atc', 'ATC / navigation'), ('catering', 'Catering'),
  ('cargo_handling', 'Cargo handling'), ('pilots', 'Pilots'), ('flight_attendants', 'Flight attendants'),
  ('maintenance', 'Maintenance');
)sql";

// Milestone 8 (D-137): each settled flight's phase starts and its completed GSX services, for the leg templates
// (D-075). Written once each; deboarding comes after block in, so its row is added when it ends. CASCADE as on
// flight_cabin: only tests delete flights. Both keys lead with flight_id, so the foreign key is indexed.
constexpr const char* kMigration5 = R"sql(
CREATE TABLE flight_phase (
  flight_id INTEGER NOT NULL REFERENCES flight ON DELETE CASCADE,
  seq INTEGER NOT NULL CHECK (seq >= 0),
  phase TEXT NOT NULL CHECK (phase IN ('pushback', 'taxi_out', 'takeoff', 'climb', 'cruise', 'descent', 'approach',
                                       'go_around', 'landing', 'taxi_in', 'arrived')),
  started_at TEXT NOT NULL CHECK (started_at GLOB )sql" BT_INSTANT R"sql(),
  PRIMARY KEY (flight_id, seq)
) STRICT;

CREATE TABLE flight_service (
  flight_service_id INTEGER PRIMARY KEY,
  flight_id INTEGER NOT NULL REFERENCES flight ON DELETE CASCADE,
  service TEXT NOT NULL CHECK (service IN ('catering', 'refuelling', 'boarding', 'deboarding')),
  started_at TEXT NOT NULL CHECK (started_at GLOB )sql" BT_INSTANT R"sql(),
  ended_at TEXT NOT NULL CHECK (ended_at GLOB )sql" BT_INSTANT R"sql(),
  CHECK (ended_at >= started_at),
  UNIQUE (flight_id, service, started_at)
) STRICT;
)sql";

// Milestone 9 (D-148): the parking spot at block out and block in, as the recorder named it ("Gate D14"). NULL when
// no spot was within 40 m, the airport's spots weren't loaded, or the flight is from before 0.10.0.
constexpr const char* kMigration6 = R"sql(
ALTER TABLE flight ADD COLUMN origin_parking TEXT CHECK (origin_parking IS NULL OR origin_parking <> '');
ALTER TABLE flight ADD COLUMN arrival_parking TEXT CHECK (arrival_parking IS NULL OR arrival_parking <> '');
)sql";

// Milestone 10 (D-160, docs/database.md): what each procedure check measured, or why it couldn't be, the landing's
// facts and Claude's review. Written once each; the scores are computed from them when shown. CASCADE as on
// flight_phase: only tests delete flights. The composite keys lead with flight_id, so those foreign keys are indexed.
constexpr const char* kMigration7 = R"sql(
CREATE TABLE check_part (code TEXT PRIMARY KEY CHECK (code <> '')) STRICT;
INSERT INTO check_part (code) VALUES
  ('beacon_late_s'), ('belts_off_departure_s'), ('taxi_max_kt'), ('origin_baro_off_tenths'), ('takeoff_strobes'),
  ('takeoff_landing_lights'), ('over_250_s'), ('std_late_s'), ('belts_off_descent_s'), ('destination_baro_off_tenths'),
  ('landing_lights_off_s'), ('stable_gear_down'), ('stable_landing_flaps'), ('stable_sink_fpm'),
  ('stable_vref_offset_kt'), ('stable_lined_up'), ('strobes_on_after_landing_s');

CREATE TABLE check_skip_reason (code TEXT PRIMARY KEY CHECK (code <> '')) STRICT;
INSERT INTO check_skip_reason (code) VALUES ('no_pmdg_data'), ('no_vref'), ('no_atis'), ('no_marker'), ('diverted');

CREATE TABLE flight_debrief (
  flight_id INTEGER PRIMARY KEY REFERENCES flight ON DELETE CASCADE,
  family TEXT NOT NULL CHECK (family IN ('737', '777'))
) STRICT;

CREATE TABLE flight_check_value (
  flight_id INTEGER NOT NULL REFERENCES flight_debrief ON DELETE CASCADE,
  part TEXT NOT NULL REFERENCES check_part ON DELETE RESTRICT,
  value INTEGER NOT NULL,
  PRIMARY KEY (flight_id, part)
) STRICT;
CREATE INDEX flight_check_value_part ON flight_check_value (part);

CREATE TABLE flight_check_skip (
  flight_id INTEGER NOT NULL REFERENCES flight_debrief ON DELETE CASCADE,
  part TEXT NOT NULL REFERENCES check_part ON DELETE RESTRICT,
  reason TEXT NOT NULL REFERENCES check_skip_reason ON DELETE RESTRICT,
  PRIMARY KEY (flight_id, part)
) STRICT;
CREATE INDEX flight_check_skip_part ON flight_check_skip (part);
CREATE INDEX flight_check_skip_reason ON flight_check_skip (reason);

CREATE TABLE flight_landing (
  flight_id INTEGER PRIMARY KEY REFERENCES flight_debrief ON DELETE CASCADE,
  vs_fpm INTEGER NOT NULL CHECK (vs_fpm >= 0),
  g_hundredths INTEGER NOT NULL CHECK (g_hundredths >= 0),
  bounces INTEGER NOT NULL CHECK (bounces >= 0),
  ias_kt INTEGER NOT NULL CHECK (ias_kt >= 0),
  heading_true_deg INTEGER NOT NULL CHECK (heading_true_deg BETWEEN 0 AND 359),
  wind_dir_deg INTEGER NOT NULL CHECK (wind_dir_deg BETWEEN 0 AND 359),
  wind_kt INTEGER NOT NULL CHECK (wind_kt >= 0),
  sim_pitch_tenths INTEGER,
  bank_tenths INTEGER,
  vref_kt INTEGER CHECK (vref_kt IS NULL OR vref_kt > 0),
  off_centreline_dm INTEGER,
  off_heading_tenths INTEGER
) STRICT;

CREATE TABLE flight_review (
  flight_id INTEGER PRIMARY KEY REFERENCES flight_debrief ON DELETE CASCADE,
  summary TEXT NOT NULL,
  landing TEXT NOT NULL,
  atc TEXT NOT NULL,
  model TEXT NOT NULL CHECK (model <> ''),
  written_at TEXT NOT NULL CHECK (written_at GLOB )sql" BT_INSTANT R"sql()
) STRICT;

CREATE TABLE flight_review_item (
  flight_id INTEGER NOT NULL REFERENCES flight_review ON DELETE CASCADE,
  seq INTEGER NOT NULL CHECK (seq >= 0),
  text TEXT NOT NULL CHECK (text <> ''),
  PRIMARY KEY (flight_id, seq)
) STRICT;
)sql";

// Two APU checks (D-172, D-173): the APU off by the transition altitude, and switched on by engine shutdown at block in.
// Flights measured before them have neither part and show both as not checked.
constexpr const char* kMigration8 = R"sql(
INSERT INTO check_part (code) VALUES ('apu_on_after_transition_s'), ('apu_on_at_shutdown');
)sql";

// D-181: the dashboard's "San Diego to Las Vegas". Filled by blocktime-import from OurAirports' municipality.
constexpr const char* kMigration9 = R"sql(
ALTER TABLE airport ADD COLUMN city TEXT NOT NULL DEFAULT '';
)sql";

// Migration 10 (D-238, review 2.1): the sets that will grow become lookup tables. Flight kinds will gain positioning
// legs and background flights; return reasons gain 'returned' with the settings screen. A rebuild by SQLite's 12-step
// recipe (it runs with foreign keys off, D-238): each table is recreated with its columns in the same order and every
// other rule as it was, the rows copied, the old table dropped and its indexes made again. The cargo rule names two
// kinds, so it stays a CHECK.
constexpr const char* kMigration10 = R"sql(
CREATE TABLE flight_kind (code TEXT PRIMARY KEY CHECK (code <> '')) STRICT;
INSERT INTO flight_kind (code) VALUES ('passenger'), ('vip'), ('charter'), ('cargo'), ('delivery');

CREATE TABLE return_reason (code TEXT PRIMARY KEY CHECK (code <> '')) STRICT;
INSERT INTO return_reason (code) VALUES ('unpaid');

CREATE TABLE airplane_new (
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
  return_reason TEXT REFERENCES return_reason ON DELETE RESTRICT,
  free_flight_number TEXT
    CHECK (free_flight_number IS NULL OR
           (length(free_flight_number) BETWEEN 1 AND 4 AND free_flight_number NOT GLOB '*[^0-9]*')),
  CHECK ((returned_on IS NULL) = (return_reason IS NULL))
) STRICT;
INSERT INTO airplane_new (airplane_id, registration, tail_number, aircraft_type_id, fleet_id, airframe_name,
                          simbrief_airframe_id, leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb,
                          awaiting_delivery, returned_on, return_reason, free_flight_number)
  SELECT airplane_id, registration, tail_number, aircraft_type_id, fleet_id, airframe_name, simbrief_airframe_id,
         leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb, awaiting_delivery, returned_on,
         return_reason, free_flight_number FROM airplane;
DROP TABLE airplane;
ALTER TABLE airplane_new RENAME TO airplane;
CREATE INDEX airplane_aircraft_type ON airplane (aircraft_type_id);
CREATE INDEX airplane_fleet ON airplane (fleet_id);
CREATE INDEX airplane_location ON airplane (location_airport_id);
CREATE UNIQUE INDEX airplane_free_flight_number ON airplane (free_flight_number);
CREATE INDEX airplane_return_reason ON airplane (return_reason);

CREATE TABLE planned_flight_new (
  planned_flight_id INTEGER PRIMARY KEY,
  airplane_id INTEGER NOT NULL UNIQUE REFERENCES airplane ON DELETE RESTRICT,
  origin_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  destination_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  flight_number TEXT NOT NULL CHECK (length(flight_number) BETWEEN 1 AND 4 AND flight_number NOT GLOB '*[^0-9]*'),
  kind TEXT NOT NULL REFERENCES flight_kind ON DELETE RESTRICT,
  cargo_lb INTEGER NOT NULL CHECK (cargo_lb >= 0),
  fuel_price_cents_per_100lb INTEGER NOT NULL CHECK (fuel_price_cents_per_100lb > 0),
  planned_at TEXT NOT NULL CHECK (planned_at GLOB )sql" BT_INSTANT R"sql(),
  origin_runway TEXT CHECK (origin_runway IS NULL OR origin_runway <> ''),
  destination_runway TEXT CHECK (destination_runway IS NULL OR destination_runway <> ''),
  CHECK (destination_airport_id <> origin_airport_id),
  CHECK (cargo_lb = 0 OR kind IN ('passenger', 'cargo'))
) STRICT;
INSERT INTO planned_flight_new (planned_flight_id, airplane_id, origin_airport_id, destination_airport_id,
                                flight_number, kind, cargo_lb, fuel_price_cents_per_100lb, planned_at, origin_runway,
                                destination_runway)
  SELECT planned_flight_id, airplane_id, origin_airport_id, destination_airport_id, flight_number, kind, cargo_lb,
         fuel_price_cents_per_100lb, planned_at, origin_runway, destination_runway FROM planned_flight;
DROP TABLE planned_flight;
ALTER TABLE planned_flight_new RENAME TO planned_flight;
CREATE INDEX planned_flight_origin ON planned_flight (origin_airport_id);
CREATE INDEX planned_flight_destination ON planned_flight (destination_airport_id);
CREATE INDEX planned_flight_kind ON planned_flight (kind);

CREATE TABLE flight_new (
  flight_id INTEGER PRIMARY KEY,
  log_flight_id TEXT NOT NULL UNIQUE CHECK (log_flight_id <> ''),
  airplane_id INTEGER NOT NULL REFERENCES airplane ON DELETE RESTRICT,
  flight_number TEXT NOT NULL CHECK (length(flight_number) BETWEEN 1 AND 4 AND flight_number NOT GLOB '*[^0-9]*'),
  kind TEXT NOT NULL REFERENCES flight_kind ON DELETE RESTRICT,
  origin_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  planned_destination_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  arrival_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  cargo_lb INTEGER NOT NULL CHECK (cargo_lb >= 0),
  fuel_price_cents_per_100lb INTEGER NOT NULL CHECK (fuel_price_cents_per_100lb > 0),
  planned_at TEXT NOT NULL CHECK (planned_at GLOB )sql" BT_INSTANT R"sql(),
  block_out_at TEXT NOT NULL CHECK (block_out_at GLOB )sql" BT_INSTANT R"sql(),
  block_in_at TEXT NOT NULL CHECK (block_in_at GLOB )sql" BT_INSTANT R"sql(),
  fuel_on_record_lb INTEGER NOT NULL CHECK (fuel_on_record_lb >= 0),
  fuel_out_lb INTEGER NOT NULL CHECK (fuel_out_lb >= 0),
  fuel_in_lb INTEGER NOT NULL CHECK (fuel_in_lb >= 0),
  origin_runway TEXT CHECK (origin_runway IS NULL OR origin_runway <> ''),
  destination_runway TEXT CHECK (destination_runway IS NULL OR destination_runway <> ''),
  origin_parking TEXT CHECK (origin_parking IS NULL OR origin_parking <> ''),
  arrival_parking TEXT CHECK (arrival_parking IS NULL OR arrival_parking <> ''),
  CHECK (block_in_at > block_out_at),
  CHECK (cargo_lb = 0 OR kind IN ('passenger', 'cargo'))
) STRICT;
INSERT INTO flight_new (flight_id, log_flight_id, airplane_id, flight_number, kind, origin_airport_id,
                        planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb,
                        planned_at, block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb,
                        origin_runway, destination_runway, origin_parking, arrival_parking)
  SELECT flight_id, log_flight_id, airplane_id, flight_number, kind, origin_airport_id, planned_destination_airport_id,
         arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at, block_out_at, block_in_at,
         fuel_on_record_lb, fuel_out_lb, fuel_in_lb, origin_runway, destination_runway, origin_parking,
         arrival_parking FROM flight;
DROP TABLE flight;
ALTER TABLE flight_new RENAME TO flight;
CREATE INDEX flight_airplane ON flight (airplane_id);
CREATE INDEX flight_origin ON flight (origin_airport_id);
CREATE INDEX flight_planned_destination ON flight (planned_destination_airport_id);
CREATE INDEX flight_arrival ON flight (arrival_airport_id);
CREATE INDEX flight_block_in ON flight (block_in_at);
CREATE INDEX flight_kind_code ON flight (kind);
)sql";

// Migration 11 (milestone 15b, docs/fleet.md): airplanes are leased in the app from a SimBrief screenshot (D-242) and
// fleets are gone (D-244). airplane is rebuilt without fleet_id; its registration, SimBrief airframe and flight number
// are unique among airplanes in the fleet only, so a returned plane can be leased again (D-246). The screenshot's
// other values, instant delivery and the lease refund get their tables.
constexpr const char* kMigration11 = R"sql(
CREATE TABLE airplane_new (
  airplane_id INTEGER PRIMARY KEY,
  registration TEXT NOT NULL CHECK (registration <> '' AND registration NOT GLOB '*[^A-Z0-9-]*'),
  tail_number TEXT CHECK (tail_number IS NULL OR tail_number <> ''),
  aircraft_type_id INTEGER NOT NULL REFERENCES aircraft_type ON DELETE RESTRICT,
  airframe_name TEXT NOT NULL CHECK (airframe_name <> ''),
  simbrief_airframe_id TEXT NOT NULL CHECK (simbrief_airframe_id <> ''),
  leased_on TEXT NOT NULL CHECK (leased_on GLOB )sql" BT_DATE R"sql(),
  monthly_lease_cents INTEGER NOT NULL CHECK (monthly_lease_cents > 0 AND monthly_lease_cents % 500 = 0),
  location_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  fuel_on_board_lb INTEGER NOT NULL CHECK (fuel_on_board_lb >= 0),
  awaiting_delivery INTEGER NOT NULL CHECK (awaiting_delivery IN (0, 1)),
  returned_on TEXT CHECK (returned_on IS NULL OR returned_on GLOB )sql" BT_DATE R"sql(),
  return_reason TEXT REFERENCES return_reason ON DELETE RESTRICT,
  free_flight_number TEXT
    CHECK (free_flight_number IS NULL OR
           (length(free_flight_number) BETWEEN 1 AND 4 AND free_flight_number NOT GLOB '*[^0-9]*')),
  CHECK ((returned_on IS NULL) = (return_reason IS NULL))
) STRICT;
INSERT INTO airplane_new (airplane_id, registration, tail_number, aircraft_type_id, airframe_name,
                          simbrief_airframe_id, leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb,
                          awaiting_delivery, returned_on, return_reason, free_flight_number)
  SELECT airplane_id, registration, tail_number, aircraft_type_id, airframe_name, simbrief_airframe_id, leased_on,
         monthly_lease_cents, location_airport_id, fuel_on_board_lb, awaiting_delivery, returned_on, return_reason,
         free_flight_number FROM airplane;
DROP TABLE airplane;
ALTER TABLE airplane_new RENAME TO airplane;
CREATE INDEX airplane_aircraft_type ON airplane (aircraft_type_id);
CREATE INDEX airplane_location ON airplane (location_airport_id);
CREATE INDEX airplane_return_reason ON airplane (return_reason);
CREATE UNIQUE INDEX airplane_registration_in_fleet ON airplane (registration) WHERE returned_on IS NULL;
CREATE UNIQUE INDEX airplane_simbrief_in_fleet ON airplane (simbrief_airframe_id) WHERE returned_on IS NULL;
CREATE UNIQUE INDEX airplane_free_flight_number ON airplane (free_flight_number) WHERE returned_on IS NULL;
DROP TABLE fleet;

INSERT INTO return_reason (code) VALUES ('returned');
INSERT INTO ledger_category (code, label) VALUES ('delivery_fee', 'Delivery fee'), ('lease_refund', 'Lease refund');

CREATE TABLE airplane_simbrief (
  airplane_id INTEGER PRIMARY KEY REFERENCES airplane ON DELETE CASCADE,
  read_at TEXT NOT NULL CHECK (read_at GLOB )sql" BT_INSTANT R"sql(),
  base_type TEXT NOT NULL CHECK (base_type <> ''),
  max_passengers INTEGER NOT NULL CHECK (max_passengers >= 0),
  empty_weight_lb INTEGER NOT NULL CHECK (empty_weight_lb > 0),
  mzfw_lb INTEGER NOT NULL CHECK (mzfw_lb > empty_weight_lb),
  mtow_lb INTEGER NOT NULL CHECK (mtow_lb > 0),
  max_fuel_lb INTEGER NOT NULL CHECK (max_fuel_lb > 0),
  max_cargo_lb INTEGER NOT NULL CHECK (max_cargo_lb > 0)
) STRICT;

CREATE TABLE instant_delivery (
  instant_delivery_id INTEGER PRIMARY KEY,
  airplane_id INTEGER NOT NULL UNIQUE REFERENCES airplane ON DELETE RESTRICT,
  from_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  to_airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT,
  CHECK (to_airport_id <> from_airport_id)
) STRICT;
CREATE INDEX instant_delivery_from ON instant_delivery (from_airport_id);
CREATE INDEX instant_delivery_to ON instant_delivery (to_airport_id);

CREATE TABLE instant_delivery_line (
  ledger_line_id INTEGER PRIMARY KEY REFERENCES ledger_line ON DELETE RESTRICT,
  instant_delivery_id INTEGER NOT NULL REFERENCES instant_delivery ON DELETE RESTRICT
) STRICT;
CREATE INDEX instant_delivery_line_delivery ON instant_delivery_line (instant_delivery_id);

CREATE TABLE lease_refund (
  ledger_line_id INTEGER PRIMARY KEY REFERENCES ledger_line ON DELETE RESTRICT,
  airplane_id INTEGER NOT NULL UNIQUE REFERENCES airplane ON DELETE RESTRICT
) STRICT;
)sql";

#undef BT_INSTANT
#undef BT_DATE

using detail::Migration;

constexpr std::array kMigrations{Migration{2, kMigration2}, Migration{3, kMigration3}, Migration{4, kMigration4},
                                 Migration{5, kMigration5}, Migration{6, kMigration6}, Migration{7, kMigration7},
                                 Migration{8, kMigration8}, Migration{9, kMigration9},
                                 Migration{10, kMigration10, true}, Migration{11, kMigration11, true}};

// The first row PRAGMA foreign_key_check finds pointing at nothing, as "flight row 3 → airplane"; "" if none.
std::string firstDanglingRow(db::Database& db) {
    db::Statement q(db, "PRAGMA foreign_key_check");
    if (!q.next()) return q.error();
    return std::format("{} row {} → {}", q.text(0), q.isNull(1) ? std::string("?") : q.text(1), q.text(2));
}

// Turns foreign keys back on when it goes out of scope, after the transaction's own end: SQLite ignores the pragma
// inside a transaction.
struct ForeignKeysBackOn {
    db::Database& db;
    explicit ForeignKeysBackOn(db::Database& d) : db(d) {}
    ForeignKeysBackOn(const ForeignKeysBackOn&) = delete;
    ForeignKeysBackOn& operator=(const ForeignKeysBackOn&) = delete;
    ~ForeignKeysBackOn() { db.exec("PRAGMA foreign_keys = ON"); }
};

constexpr int kFirstRealSchemaVersion = 2;  // what a clobbered milestone 4 airline really is (D-098)

constexpr int kTestSchemaVersion = 1;  // the Qt test's database (D-061)

bool hasTable(db::Database& db, const char* name) {
    db::Statement q(db, "SELECT 1 FROM sqlite_schema WHERE type = 'table' AND name = ?");
    q.bind(1, std::string_view(name));
    return q.next();
}

}  // namespace

bool migrateAirlineDatabase(db::Database& db, std::string* error) {
    return migrateAirlineDatabaseTo(db, kAirlineSchemaVersion, error);
}

bool migrateAirlineDatabaseTo(db::Database& db, int target, std::string* error) {
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
        if (m.version <= current || m.version > target) continue;
        if (!detail::applyMigration(db, m, error)) return false;
    }
    if (error) error->clear();
    return true;
}

bool detail::applyMigration(db::Database& db, const Migration& m, std::string* error) {
    std::string message;
    const auto fail = [&] {
        if (error) *error = std::format("Migration {} failed: {}", m.version, message);
        return false;
    };
    ForeignKeysBackOn backOn{db};  // declared before the transaction, so it runs after its rollback
    if (m.rebuild && !db.exec("PRAGMA foreign_keys = OFF", &message)) return fail();
    db::Transaction tx(db);
    if (!tx.active()) {
        message = tx.error();
        return fail();
    }
    if (!db.exec(m.sql, &message)) return fail();
    if (m.rebuild) {
        const std::string dangling = firstDanglingRow(db);
        if (!dangling.empty()) {
            message = "foreign key check failed: " + dangling;
            return fail();
        }
    }
    if (!db.exec(std::format("PRAGMA user_version = {}", m.version), &message) || !tx.commit(&message)) return fail();
    return true;
}

AirlineDbOpen openAirlineDatabase(const std::filesystem::path& file) {
    AirlineDbOpen result;
    result.db = db::Database::open(file, &result.error);
    if (!result.db) return result;

    if (result.db->userVersion() == kTestSchemaVersion && hasTable(*result.db, "airplane")) {
        // BlockTime 0.4 and older ran "PRAGMA user_version = 1" on any blocktime.db it opened. This is a real
        // airline, not the Qt test's file: put the version back (the stray airframes table does no harm). Only 0.4 did
        // this, before any schema after 2 existed, so it's 2; the migrations after it still run below.
        if (!result.db->exec(std::format("PRAGMA user_version = {}", kFirstRealSchemaVersion), &result.error)) {
            result.db.reset();
            return result;
        }
        result.versionRestored = true;
    }
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
    if (const int version = result.db->userVersion(); version > 0 && version < kAirlineSchemaVersion) {
        // A migration can't be undone, so the airline is copied aside first. A copy left by an earlier try that
        // failed is kept: it's the one from before that migration.
        const auto backup = file.parent_path() / std::format("blocktime.before-schema-{}.db", kAirlineSchemaVersion);
        std::error_code ec;
        if (!std::filesystem::exists(backup, ec)) {
            db::Statement vacuum(*result.db, "VACUUM INTO ?");
            const std::u8string name = backup.u8string();  // sqlite3 takes UTF-8
            vacuum.bind(1, std::string_view(reinterpret_cast<const char*>(name.data()), name.size()));
            if (!vacuum.run()) {
                result.error = "Couldn't back up blocktime.db before updating it, so it wasn't updated: " + vacuum.error();
                result.db.reset();
                return result;
            }
        }
        result.backup = backup;
    }
    if (!migrateAirlineDatabase(*result.db, &result.error)) result.db.reset();
    return result;
}

std::filesystem::path defaultAirlineDatabaseFile() { return defaultSettingsFile().parent_path() / "blocktime.db"; }

}  // namespace blocktime
