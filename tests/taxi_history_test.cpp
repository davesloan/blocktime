// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dave's own taxi times for the predictions (D-212): the median of the last 5 completed flights at the airport.

#include <chrono>
#include <cstdint>
#include <format>
#include <string>

#include <gtest/gtest.h>

#include "taxi_history.h"
#include "test_airline.h"

using namespace blocktime;
using namespace std::chrono;

namespace {

// 2026-11-01T00:00:00Z plus `s` seconds, as the database stores instants.
std::string at(long long s) {
    const sys_seconds base = sys_days(year(2026) / November / 1);
    return std::format("{:%FT%TZ}", base + seconds(s));
}

// A completed flight of N814SX from `origin` to `arrival`, `day` days in: block out at 10:00, then the phases given
// as (phase, seconds after block out), block in `blockS` after block out. Returns its flight_id.
std::int64_t flight(db::Database& db, const std::string& origin, const std::string& arrival, int day,
                    std::initializer_list<std::pair<const char*, int>> phases, int blockS = 4000) {
    static int n = 0;
    const long long out = day * 86400LL + 36000;
    const std::string logId = "log-" + std::to_string(++n);
    EXPECT_TRUE(db.exec(
        "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
        " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb, planned_at,"
        " block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb)"
        " SELECT '" + logId + "', a.airplane_id, '8', 'vip', o.airport_id, d.airport_id, d.airport_id, 0, 4100, '" +
        at(out - 600) + "', '" + at(out) + "', '" + at(out + blockS) + "', 0, 9000, 8000"
        " FROM airplane a, airport o, airport d WHERE a.registration = 'N814SX' AND o.icao_code = '" + origin +
        "' AND d.icao_code = '" + arrival + "'"));
    db::Statement id(db, "SELECT flight_id FROM flight WHERE log_flight_id = ?");
    id.bind(1, std::string_view(logId));
    EXPECT_TRUE(id.next());
    const std::int64_t flightId = id.integer(0);
    int seq = 0;
    for (const auto& [phase, s] : phases) {
        EXPECT_TRUE(db.exec("INSERT INTO flight_phase (flight_id, seq, phase, started_at) VALUES (" +
                            std::to_string(flightId) + ", " + std::to_string(seq++) + ", '" + phase + "', '" +
                            at(out + s) + "')"));
    }
    return flightId;
}

}  // namespace

TEST(TaxiHistory, MedianOfLastFive) {
    db::Database db = test::testAirline();
    int day = 0;
    for (const int taxi : {60, 540, 600, 660, 720, 3000}) flight(db, "KSAN", "KBFI", ++day, {{"takeoff", taxi}});
    EXPECT_EQ(taxiHistory(db, "KSAN", "KSHT", "").outS, std::optional<int>(660));  // the 60 s flight is the oldest
}

TEST(TaxiHistory, MedianEvenCount) {
    db::Database db = test::testAirline();
    flight(db, "KSAN", "KBFI", 1, {{"takeoff", 120}});
    flight(db, "KSAN", "KBFI", 2, {{"takeoff", 180}});
    EXPECT_EQ(taxiHistory(db, "KSAN", "KSHT", "").outS, std::optional<int>(150));
    EXPECT_EQ(medianOf({5, 1, 3}), 3);
    EXPECT_EQ(medianOf({1, 2}), 1);  // the mean of the middle two, rounded down
}

// Dave's first flights have no flight_phase rows (Review Focus 3).
TEST(TaxiHistory, SkipsFlightsWithoutPhases) {
    db::Database db = test::testAirline();
    flight(db, "KSAN", "KBFI", 1, {});
    const TaxiHistory h = taxiHistory(db, "KSAN", "KBFI", "");
    EXPECT_FALSE(h.outS.has_value());
    EXPECT_FALSE(h.inS.has_value());
}

// A go-around: taxi in runs from the last touchdown (Review Focus 3).
TEST(TaxiHistory, GoAroundUsesTheLastLanding) {
    db::Database db = test::testAirline();
    flight(db, "KSAN", "KBFI", 1, {{"takeoff", 600}, {"landing", 3000}, {"go_around", 3010}, {"landing", 3600}},
           3780);
    EXPECT_EQ(taxiHistory(db, "KSHT", "KBFI", "").inS, std::optional<int>(180));
}

// A replay counts only the flights that blocked in before its block out (Review Focus 4).
TEST(TaxiHistory, OnlyFlightsBefore) {
    db::Database db = test::testAirline();
    flight(db, "KSAN", "KBFI", 1, {{"takeoff", 300}});
    flight(db, "KSAN", "KBFI", 2, {{"takeoff", 900}});
    EXPECT_EQ(taxiHistory(db, "KSAN", "KBFI", at(2 * 86400LL)).outS, std::optional<int>(300));
    EXPECT_EQ(taxiHistory(db, "KSAN", "KBFI", "").outS, std::optional<int>(600));
}

TEST(TaxiHistory, InIsByArrivalAirport) {
    db::Database db = test::testAirline();
    flight(db, "KSAN", "KBFI", 1, {{"takeoff", 300}, {"landing", 3000}}, 3240);
    // Planned for KSHT, landed at KBFI: the taxi in belongs to KBFI.
    ASSERT_TRUE(db.exec("UPDATE flight SET planned_destination_airport_id ="
                        " (SELECT airport_id FROM airport WHERE icao_code = 'KSHT')"));
    EXPECT_FALSE(taxiHistory(db, "KSAN", "KSHT", "").inS.has_value());
    EXPECT_EQ(taxiHistory(db, "KSHT", "KBFI", "").inS, std::optional<int>(240));
    EXPECT_FALSE(taxiHistory(db, "KSHT", "KSAN", "").inS.has_value());
    EXPECT_FALSE(taxiHistory(db, "KBFI", "KSAN", "").outS.has_value());  // out is by origin
}
