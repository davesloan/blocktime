// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small real airline for tests (milestone 5): the repo's catalog, three airports and two airplanes at KBFI,
// leased through the same code the app uses. Used by blocktime-tests and blocktime-qt-tests.

#pragma once

#include <string>

#include <gtest/gtest.h>

#include "airline_config.h"
#include "airline_db.h"
#include "catalog.h"
#include "leasing.h"
#include "sqlite_db.h"

namespace blocktime::test {

inline AirlineConfig testAirlineConfig() {
    using namespace std::chrono;
    AirlineConfig c;
    c.identity = {"Soxxly Airlines", "XLY", "SOCKS"};
    c.foundedOn = Date{year{2026}, month{11}, day{22}};
    c.startingCashCents = 500'000'000;
    c.fleets = {"Executive", "Mainline"};
    ConfigAirplane bbj;
    bbj.registration = "N814SX";
    bbj.airframe = "737-800 BBJ2 VIP";
    bbj.fleet = "Executive";
    bbj.simbriefAirframeId = "730097_1790536204614";
    bbj.leasedOn = c.foundedOn;
    bbj.locationIcao = "KBFI";
    bbj.awaitingDelivery = true;
    bbj.flightNumber = "8";
    ConfigAirplane b738 = bbj;
    b738.registration = "N738BT";
    b738.airframe = "737-800 Two-class";
    b738.fleet = "Mainline";
    b738.simbriefAirframeId = "test_738";
    b738.awaitingDelivery = false;
    b738.flightNumber = "";
    c.airplanes = {bbj, b738};
    return c;
}

// Migrates `db` and fills it: KBFI, KSAN and KSHT (a 5,000 ft runway), N814SX (BBJ2, awaiting delivery, flight
// number 8) and N738BT (737-800 two-class, no flight number), both at KBFI.
inline void setUpTestAirline(db::Database& db) {
    std::string error;
    ASSERT_TRUE(migrateAirlineDatabase(db, &error)) << error;
    ASSERT_TRUE(db.exec(
        "INSERT INTO airport (icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
        " longest_runway_ft) VALUES"
        " ('KBFI', 'Boeing Field', 47.53, -122.302, 21, 'US', 'America/Los_Angeles', 10007),"
        " ('KSAN', 'San Diego International', 32.7336, -117.19, 17, 'US', 'America/Los_Angeles', 9401),"
        " ('KSHT', 'Short Field', 47.0, -120.0, 1000, 'US', 'America/Los_Angeles', 5000)"));
    const auto catalog = loadCatalog(BLOCKTIME_SOURCE_DIR "/config/catalog.yaml", &error);
    ASSERT_TRUE(catalog.has_value()) << error;
    const AirlineConfig config = testAirlineConfig();
    ASSERT_TRUE(syncCatalogTypes(db, *catalog, &error)) << error;
    ASSERT_TRUE(syncFleets(db, config, &error)) << error;
    ASSERT_TRUE(postOpeningCash(db, config, &error)) << error;
    const LeaseOutcome outcome = settleLeases(db, *catalog, config, config.foundedOn, 2);  // two slots
    ASSERT_EQ(outcome.error, "");
    ASSERT_EQ(outcome.leased, 2);
    ASSERT_TRUE(syncFreeFlightNumbers(db, config, &error)) << error;
}

inline db::Database testAirline() {
    db::Database db = db::Database::openInMemory();
    setUpTestAirline(db);
    return db;
}

}  // namespace blocktime::test
