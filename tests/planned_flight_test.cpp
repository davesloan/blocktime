// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "planned_flight.h"
#include "simbrief.h"
#include "test_airline.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

SimbriefPlan fixturePlan() {
    std::string error;
    auto p = parseOfp(fixture("ofp_v2_min.json"), &error);
    EXPECT_TRUE(p.has_value()) << error;
    return p.value_or(SimbriefPlan{});
}

NewPlannedFlight vipToKsan(db::Database& db) {
    const auto a = loadFlightAirplane(db, "N814SX");
    EXPECT_TRUE(a.has_value());
    NewPlannedFlight f;
    f.airplaneId = a ? a->airplaneId : 0;
    f.originIcao = "KBFI";
    f.destinationIcao = "KSAN";
    f.flightNumber = "101";
    f.kind = FlightKind::Vip;
    f.cabins = {{"economy", 46}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = "2026-11-23T14:00:00Z";
    f.originRunway = "14R";
    return f;
}

std::int64_t count(db::Database& db, const char* sql) {
    db::Statement q(db, sql);
    EXPECT_TRUE(q.next()) << q.error();
    return q.integer(0);
}

}  // namespace

TEST(Planned, KindsByRole) {
    db::Database db = test::testAirline();
    EXPECT_EQ(allowedKinds(*loadFlightAirplane(db, "N814SX")), (std::vector<FlightKind>{FlightKind::Delivery}));
    ASSERT_TRUE(db.exec("UPDATE airplane SET awaiting_delivery = 0 WHERE registration = 'N814SX'"));
    EXPECT_EQ(allowedKinds(*loadFlightAirplane(db, "N814SX")),
              (std::vector<FlightKind>{FlightKind::Vip, FlightKind::Charter}));
    EXPECT_EQ(allowedKinds(*loadFlightAirplane(db, "N738BT")), (std::vector<FlightKind>{FlightKind::Passenger}));
    EXPECT_FALSE(loadFlightAirplane(db, "N999ZZ").has_value());
}

TEST(Planned, LoadsTheAirplane) {
    db::Database db = test::testAirline();
    const auto a = loadFlightAirplane(db, "N814SX");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->typeCode, "737-800 BBJ2");
    EXPECT_EQ(a->role, "executive");
    EXPECT_EQ(a->originIcao, "KBFI");
    EXPECT_EQ(a->originTimeZone, "America/Los_Angeles");
    EXPECT_EQ(a->freeFlightNumber, "8");
    EXPECT_EQ(a->simbriefAirframeId, "730097_1790536204614");
    ASSERT_EQ(a->cabins.size(), 1u);
    EXPECT_EQ(a->cabins[0].seats, 48);
    EXPECT_EQ(a->holds.size(), 2u);
    EXPECT_EQ(a->limits.mlwLb, 146300);
}

TEST(Planned, DestinationChecks) {
    db::Database db = test::testAirline();
    const FlightAirplane a = *loadFlightAirplane(db, "N814SX");
    EXPECT_EQ(checkDestination(db, a, "").message, "Type a 4-letter ICAO code.");
    EXPECT_EQ(checkDestination(db, a, "KS").message, "Type a 4-letter ICAO code.");
    EXPECT_EQ(checkDestination(db, a, "KXYZ").message, "KXYZ isn't in the airport data.");
    EXPECT_EQ(checkDestination(db, a, "kbfi").message, "N814SX is already at KBFI.");
    EXPECT_EQ(checkDestination(db, a, "KSHT").message, "KSHT's longest runway is 5,000 ft; N814SX needs 7,000 ft.");
    const DestinationCheck ok = checkDestination(db, a, " ksan ");
    EXPECT_TRUE(ok.ok);
    EXPECT_EQ(ok.message, "");
    EXPECT_EQ(ok.icao, "KSAN");
    EXPECT_EQ(ok.name, "San Diego International");
    EXPECT_NEAR(ok.distanceNm, 918, 1);
    EXPECT_FALSE(checkDestination(db, a, "KSHT").ok);
}

TEST(Planned, FlightNumbers) {
    EXPECT_TRUE(validFlightNumber("8"));
    EXPECT_TRUE(validFlightNumber("1414"));
    EXPECT_FALSE(validFlightNumber(""));
    EXPECT_FALSE(validFlightNumber("10A"));
    EXPECT_FALSE(validFlightNumber("12345"));
}

TEST(Planned, CreateLoadCancel) {
    db::Database db = test::testAirline();
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, vipToKsan(db), &error).has_value()) << error;
    const auto f = currentPlannedFlight(db);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->registration, "N814SX");
    EXPECT_EQ(f->origin, "KBFI");
    EXPECT_EQ(f->originName, "Boeing Field");
    EXPECT_EQ(f->destination, "KSAN");
    EXPECT_EQ(f->flightNumber, "101");
    EXPECT_EQ(f->kind, FlightKind::Vip);
    ASSERT_EQ(f->cabins.size(), 1u);
    EXPECT_EQ(f->cabins[0].passengers, 46);
    EXPECT_EQ(f->originRunway, "14R");
    EXPECT_EQ(f->destinationRunway, "");
    EXPECT_EQ(f->plannedAt, "2026-11-23T14:00:00Z");
    EXPECT_EQ(f->seatsInstalled, 48);
    EXPECT_NEAR(f->distanceNm, 918, 1);
    EXPECT_FALSE(f->plan.has_value());

    ASSERT_TRUE(cancelPlannedFlight(db, &error)) << error;
    EXPECT_FALSE(currentPlannedFlight(db).has_value());
    EXPECT_EQ(count(db, "SELECT COUNT(*) FROM planned_flight_cabin"), 0);
}

TEST(Planned, OnlyOneAtATime) {
    db::Database db = test::testAirline();
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, vipToKsan(db), &error).has_value()) << error;
    NewPlannedFlight other = vipToKsan(db);
    other.airplaneId = loadFlightAirplane(db, "N738BT")->airplaneId;
    other.kind = FlightKind::Passenger;
    EXPECT_FALSE(createPlannedFlight(db, other, &error).has_value());
    EXPECT_EQ(error, "Flight 101 KBFI–KSAN is already planned.");
}

TEST(Planned, StorePlanReplaces) {
    db::Database db = test::testAirline();
    std::string error;
    const auto id = createPlannedFlight(db, vipToKsan(db), &error);
    ASSERT_TRUE(id.has_value()) << error;
    SimbriefPlan p = fixturePlan();
    ASSERT_TRUE(storeSimbriefPlan(db, *id, p, fixture("ofp_v2_min.json"), "2026-11-23T14:05:00Z", &error)) << error;
    p.requestId = "999";
    ASSERT_TRUE(storeSimbriefPlan(db, *id, p, fixture("ofp_v2_min.json"), "2026-11-23T14:06:00Z", &error)) << error;
    EXPECT_EQ(count(db, "SELECT COUNT(*) FROM simbrief_plan"), 1);
    const auto f = currentPlannedFlight(db);
    ASSERT_TRUE(f->plan.has_value());
    EXPECT_EQ(f->plan->requestId, "999");
    EXPECT_EQ(f->plan->route, p.route);
    EXPECT_EQ(f->plan->estBlockS, 9060);
}

TEST(Planned, SurvivesReopening) {
    const fs::path dir = fs::temp_directory_path() / "blocktime-test-SurvivesReopening";
    fs::remove_all(dir);
    fs::create_directories(dir);
    const fs::path file = dir / "blocktime.db";
    {
        AirlineDbOpen opened = openAirlineDatabase(file);
        ASSERT_TRUE(opened.db.has_value()) << opened.error;
        test::setUpTestAirline(*opened.db);
        std::string error;
        ASSERT_TRUE(createPlannedFlight(*opened.db, vipToKsan(*opened.db), &error).has_value()) << error;
    }
    {
        AirlineDbOpen again = openAirlineDatabase(file);
        ASSERT_TRUE(again.db.has_value()) << again.error;
        const auto f = currentPlannedFlight(*again.db);
        ASSERT_TRUE(f.has_value());
        EXPECT_EQ(f->destination, "KSAN");
        EXPECT_EQ(f->flightNumber, "101");
        EXPECT_EQ(f->cabins[0].passengers, 46);
        EXPECT_EQ(f->originRunway, "14R");
    }
    fs::remove_all(dir);
}

TEST(Planned, PriceInputUsesThePlanOnceImported) {
    db::Database db = test::testAirline();
    ASSERT_TRUE(db.exec("UPDATE airplane SET fuel_on_board_lb = 3300 WHERE registration = 'N814SX'"));
    std::string error;
    const auto id = createPlannedFlight(db, vipToKsan(db), &error);
    ASSERT_TRUE(id.has_value()) << error;
    PriceInput in = priceInput(*currentPlannedFlight(db));
    EXPECT_NEAR(blockHoursFor(in), 2.54, 0.01);
    EXPECT_FALSE(in.planBlockS.has_value());
    EXPECT_FALSE(in.planRampFuelLb.has_value());
    EXPECT_EQ(in.fuelOnRecordLb, 3300);
    EXPECT_EQ(in.mlwLb, 146300);
    EXPECT_EQ(in.role, "executive");
    EXPECT_EQ(in.kind, FlightKind::Vip);

    const SimbriefPlan plan = fixturePlan();
    ASSERT_TRUE(storeSimbriefPlan(db, *id, plan, fixture("ofp_v2_min.json"), "2026-11-23T14:05:00Z", &error));
    in = priceInput(*currentPlannedFlight(db));
    EXPECT_NEAR(blockHoursFor(in), 2.5167, 0.0001);
    EXPECT_EQ(in.planBlockS, plan.estBlockS);
    EXPECT_EQ(in.planRampFuelLb, plan.rampFuelLb);
    EXPECT_EQ(in.fuelOnRecordLb, 3300);
}

TEST(Planned, NearMlw) {
    PlannedFlightView v;
    v.limits.mlwLb = 146300;
    EXPECT_FALSE(nearMlw(v));  // no plan yet
    v.plan = SimbriefPlan{};
    v.plan->estLandingWeightLb = 139000;
    EXPECT_TRUE(nearMlw(v));
    v.plan->estLandingWeightLb = 138900;
    EXPECT_FALSE(nearMlw(v));
}

TEST(Planned, TargetCarriesThePlan) {
    db::Database db = test::testAirline();
    std::string error;
    const auto id = createPlannedFlight(db, vipToKsan(db), &error);
    ASSERT_TRUE(id.has_value()) << error;
    auto t = flightTarget(db, *currentPlannedFlight(db));
    ASSERT_TRUE(t.has_value());
    EXPECT_NEAR(t->destinationLat, 32.7336, 1e-9);  // KSAN, known before any plan
    EXPECT_NEAR(t->destinationLon, -117.19, 1e-9);
    EXPECT_EQ(t->cruiseAltitudeFt, 0);
    EXPECT_EQ(t->transitionAltitudeFt, 0);
    EXPECT_EQ(t->transitionLevelFt, 0);

    ASSERT_TRUE(storeSimbriefPlan(db, *id, fixturePlan(), fixture("ofp_v2_min.json"), "2026-11-23T14:05:00Z", &error));
    t = flightTarget(db, *currentPlannedFlight(db));
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(t->cruiseAltitudeFt, 37000);
    EXPECT_EQ(t->transitionAltitudeFt, 18000);
    EXPECT_EQ(t->transitionLevelFt, 18000);
}

namespace {

// A settled flight of N814SX, arriving at `arrival` and parking at `parking` ("" = NULL).
void flownTo(db::Database& db, const std::string& logId, const std::string& arrival, const std::string& in,
             const std::string& parking) {
    const std::string spot = parking.empty() ? "NULL" : "'" + parking + "'";
    ASSERT_TRUE(db.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb, arrival_parking)"
        " SELECT '" + logId + "', a.airplane_id, '8', 'vip', o.airport_id, d.airport_id, d.airport_id, 0, 4100,"
        " '2026-11-20T10:00:00Z', '2026-11-20T11:00:00Z', '" + in + "', 0, 9000, 8000, " + spot +
        " FROM airplane a, airport o, airport d WHERE a.registration = 'N814SX' AND o.icao_code = 'KSAN'"
        " AND d.icao_code = '" + arrival + "'"));
}

}  // namespace

TEST(Planned, LoadInParkingFromLastFlight) {
    db::Database db = test::testAirline();
    flownTo(db, "older", "KBFI", "2026-11-20T12:00:00Z", "Gate A1");
    flownTo(db, "latest", "KBFI", "2026-11-21T12:00:00Z", "Parking 70");
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, vipToKsan(db), &error).has_value()) << error;  // from KBFI
    EXPECT_EQ(currentPlannedFlight(db)->loadInParking, "Parking 70");
}

TEST(Planned, NoLoadInParkingAfterAMove) {
    db::Database db = test::testAirline();
    flownTo(db, "older", "KBFI", "2026-11-20T12:00:00Z", "Parking 70");
    flownTo(db, "latest", "KSAN", "2026-11-21T12:00:00Z", "Gate 20");  // the airplane moved since
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, vipToKsan(db), &error).has_value()) << error;
    EXPECT_EQ(currentPlannedFlight(db)->loadInParking, "");
    flownTo(db, "parked-nowhere", "KBFI", "2026-11-22T12:00:00Z", "");
    EXPECT_EQ(currentPlannedFlight(db)->loadInParking, "");
}

TEST(Planned, ViewHasCities) {
    db::Database db = test::testAirline();
    ASSERT_TRUE(db.exec("UPDATE airport SET city = 'San Diego' WHERE icao_code = 'KSAN'"));
    std::string error;
    ASSERT_TRUE(createPlannedFlight(db, vipToKsan(db), &error).has_value()) << error;
    const auto f = currentPlannedFlight(db);
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->originCity, "Boeing Field");  // no city: the airport's name
    EXPECT_EQ(f->destinationCity, "San Diego");
}

TEST(Planned, ViewHasBriefAndHolds) {
    db::Database db = test::testAirline();
    std::string error;
    const auto id = createPlannedFlight(db, vipToKsan(db), &error);
    ASSERT_TRUE(id.has_value()) << error;
    auto f = currentPlannedFlight(db);
    EXPECT_FALSE(f->brief.has_value());  // no plan yet
    ASSERT_EQ(f->holds.size(), 2u);      // N814SX's: forward then aft
    EXPECT_EQ(f->holds[0].hold, "forward");
    EXPECT_EQ(f->holds[0].ratio, 4);
    EXPECT_EQ(f->holds[1].hold, "aft");

    ASSERT_TRUE(storeSimbriefPlan(db, *id, fixturePlan(), fixture("ofp-ksan-ksjc-brief.json"), "2026-11-23T14:05:00Z",
                                  &error))
        << error;
    f = currentPlannedFlight(db);
    ASSERT_TRUE(f->brief.has_value());
    EXPECT_EQ(f->brief->sid, "PADRZ2");
    EXPECT_EQ(f->brief->zfwLb, 112967);
}

TEST(Planned, ViewHasRoute) {
    db::Database db = test::testAirline();
    std::string error;
    const auto id = createPlannedFlight(db, vipToKsan(db), &error);
    ASSERT_TRUE(id.has_value()) << error;
    auto f = currentPlannedFlight(db);
    ASSERT_EQ(f->route.size(), 2u);  // no plan: the two airports
    EXPECT_NEAR(f->route[0].lat, 47.53, 1e-6);
    EXPECT_NEAR(f->route[1].lat, 32.7336, 1e-6);

    const std::string ofp = R"({"navlog":[{"pos_lat":"45.0","pos_long":"-121.0"},{"pos_lat":"40.0","pos_long":"-119.0"}]})";
    ASSERT_TRUE(storeSimbriefPlan(db, *id, fixturePlan(), ofp, "2026-11-23T14:05:00Z", &error)) << error;
    f = currentPlannedFlight(db);
    ASSERT_EQ(f->route.size(), 4u);  // origin, two fixes, destination
    EXPECT_NEAR(f->route[1].lat, 45.0, 1e-6);
    EXPECT_NEAR(f->route[2].lon, -119.0, 1e-6);
    EXPECT_NEAR(f->route[3].lon, -117.19, 1e-6);
}

namespace {

// A flown flight of N814SX out of `origin` with `taxiOutS` from block out to the takeoff roll.
void flownWithTaxi(db::Database& db, const std::string& logId, const std::string& origin, const std::string& out,
                   const std::string& takeoff, const std::string& in) {
    ASSERT_TRUE(db.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT '" + logId + "', a.airplane_id, '8', 'vip', o.airport_id, d.airport_id, d.airport_id, 0, 4100,"
        " '2026-11-20T09:00:00Z', '" + out + "', '" + in + "', 0, 9000, 8000"
        " FROM airplane a, airport o, airport d WHERE a.registration = 'N814SX' AND o.icao_code = '" + origin +
        "' AND d.icao_code = 'KSHT'"));
    ASSERT_TRUE(db.exec("INSERT INTO flight_phase (flight_id, seq, phase, started_at) SELECT flight_id, 0, 'takeoff', '" +
                        takeoff + "' FROM flight WHERE log_flight_id = '" + logId + "'"));
}

// The brief's times with the navlog's fixes: the whole of what the predictions read from an OFP.
std::string predictionsOfp() {
    nlohmann::json ofp = nlohmann::json::parse(fixture("ofp-ksan-ksjc-brief.json"));
    ofp["navlog"] = nlohmann::json::parse(fixture("ofp-ksan-ksjc-navlog.json"))["navlog"];
    return ofp.dump();
}

}  // namespace

// D-212: Dave's median where he has flown from the airport, else the plan's.
TEST(Planned, ViewCarriesTaxiTimes) {
    db::Database db = test::testAirline();
    flownWithTaxi(db, "earlier", "KBFI", "2026-11-20T10:00:00Z", "2026-11-20T10:06:40Z", "2026-11-20T11:00:00Z");
    std::string error;
    const auto id = createPlannedFlight(db, vipToKsan(db), &error);  // KBFI to KSAN
    ASSERT_TRUE(id.has_value()) << error;
    EXPECT_EQ(currentPlannedFlight(db)->taxiOutS, 400);  // Dave's, even without a plan
    EXPECT_EQ(currentPlannedFlight(db)->taxiInS, 0);     // neither Dave's nor a plan's

    ASSERT_TRUE(storeSimbriefPlan(db, *id, fixturePlan(), predictionsOfp(), "2026-11-23T14:05:00Z", &error)) << error;
    const auto f = currentPlannedFlight(db);
    EXPECT_EQ(f->taxiOutS, 400);
    EXPECT_EQ(f->taxiInS, 1200);  // no flights into KSAN yet: the plan's
    EXPECT_EQ(f->planFixes.size(), 17u);
    EXPECT_EQ(f->route.size(), 19u);  // the origin, the fixes, the destination
}

// Review Focus 5: a replay's estimate uses the fuel on record when that flight was flown, not today's.
TEST(Planned, FlownViewUsesTheFlightsFuelOnRecord) {
    db::Database db = test::testAirline();
    flownTo(db, "flown", "KBFI", "2026-11-20T12:00:00Z", "");
    ASSERT_TRUE(db.exec("UPDATE flight SET fuel_on_record_lb = 2100 WHERE log_flight_id = 'flown'"));
    ASSERT_TRUE(db.exec("UPDATE airplane SET fuel_on_board_lb = 7777 WHERE registration = 'N814SX'"));
    EXPECT_EQ(flownFlightView(db, "flown")->fuelOnRecordLb, 2100);
}

// A replay doesn't feed on its own taxi time (Review Focus 4).
TEST(Planned, FlownViewTaxiTimesExcludeItself) {
    db::Database db = test::testAirline();
    flownWithTaxi(db, "first", "KSAN", "2026-11-20T10:00:00Z", "2026-11-20T10:08:20Z", "2026-11-20T11:00:00Z");
    flownWithTaxi(db, "second", "KSAN", "2026-11-21T10:00:00Z", "2026-11-21T10:05:00Z", "2026-11-21T11:00:00Z");
    EXPECT_EQ(flownFlightView(db, "second")->taxiOutS, 500);  // the first's 500 s, not its own 300
    EXPECT_EQ(flownFlightView(db, "first")->taxiOutS, 0);     // nothing before it, and no plan
}
