// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small real airline for tests (milestone 5): the repo's aircraft files, three airports and two airplanes at KBFI,
// leased through the same code the app uses. Used by blocktime-tests and blocktime-qt-tests.

#pragma once

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "airline_config.h"
#include "airline_db.h"
#include "catalog.h"
#include "fleet_rules.h"
#include "leasing.h"
#include "rates.h"
#include "sqlite_db.h"

namespace blocktime::test {

inline AirlineConfig testAirlineConfig() {
    using namespace std::chrono;
    AirlineConfig c;
    c.identity = {"Soxxly Airlines", "XLY", "SOCKS"};
    c.foundedOn = Date{year{2026}, month{11}, day{22}};
    c.startingCashCents = 500'000'000;
    return c;
}

// N814SX's SimBrief airframe (docs/notes/simbrief-airframes.md).
inline SimbriefReading testBbjReading() {
    SimbriefReading r;
    r.registration = "N814SX";
    r.internalId = "730097_1790536204614";
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

// A 737-800 two-class whose SimBrief values give the old catalog's holds (7,500 / 9,500) and max payload (47,000).
inline SimbriefReading test738Reading() {
    SimbriefReading r = testBbjReading();
    r.registration = "N738BT";
    r.internalId = "test_738";
    r.baseType = "B738";
    r.maxPassengers = 160;
    r.emptyWeightLb = 91300;
    r.mtowLb = 174200;
    r.maxFuelLb = 46063;
    r.maxCargoLb = 17000;
    return r;
}

// Migrates `db` and fills it: KBFI, KSAN and KSHT (a 5,000 ft runway), N814SX (BBJ2, awaiting delivery, flight
// number 8) and N738BT (737-800 two-class, delivered, no flight number), both at KBFI.
inline void setUpTestAirline(db::Database& db) {
    std::string error;
    ASSERT_TRUE(migrateAirlineDatabase(db, &error)) << error;
    ASSERT_TRUE(db.exec(
        "INSERT INTO airport (icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
        " longest_runway_ft) VALUES"
        " ('KBFI', 'Boeing Field', 47.53, -122.302, 21, 'US', 'America/Los_Angeles', 10007),"
        " ('KSAN', 'San Diego International', 32.7336, -117.19, 17, 'US', 'America/Los_Angeles', 9401),"
        " ('KSHT', 'Short Field', 47.0, -120.0, 1000, 'US', 'America/Los_Angeles', 5000)"));
    const Catalog catalog = loadCatalogDir(BLOCKTIME_SOURCE_DIR "/data/aircraft", nullptr);
    ASSERT_EQ(catalog.types.size(), 7u);
    const AirlineConfig config = testAirlineConfig();
    ASSERT_TRUE(syncCatalogTypes(db, catalog, &error)) << error;
    ASSERT_TRUE(postOpeningCash(db, config, &error)) << error;
    const LeaseRequest bbj{testBbjReading(), catalog.type("737-800 BBJ2"), catalog.airframe("737-800 BBJ2 VIP"), "8",
                           std::nullopt, config.foundedOn};
    ASSERT_EQ(leaseAirplane(db, bbj, Rates{}, 2).refusal, "");
    // Instant delivery to its own factory: delivered at KBFI, free.
    const LeaseRequest b738{test738Reading(), catalog.type("737-800"), catalog.airframe("737-800 Two-class"), "",
                            std::string("KBFI"), config.foundedOn};
    ASSERT_EQ(leaseAirplane(db, b738, Rates{}, 2).refusal, "");
}

inline db::Database testAirline() {
    db::Database db = db::Database::openInMemory();
    setUpTestAirline(db);
    return db;
}

}  // namespace blocktime::test
