// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "airline_queries.h"
#include "flight_settlement.h"
#include "planned_flight.h"
#include "test_airline.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

constexpr const char* kPlannedAt = "2026-11-23T10:00:00Z";
constexpr const char* kOut = "2026-11-23T10:30:05.120Z";
constexpr const char* kIn = "2026-11-23T13:20:41.900Z";
constexpr double kSanLat = 32.7336, kSanLon = -117.19;

// One line of the event log, as EventLog writes it.
std::string line(const std::string& type, const std::string& data) {
    return R"({"ts":"2026-11-23T00:00:00.000Z","type":")" + type + R"(","data":)" + data + "}";
}

std::string blockOut(const std::string& id, std::int64_t planned, double fuel) {
    return line("block_out", R"({"flight_id":")" + id + R"(","time":")" + id + R"(","lat":47.53,"lon":-122.302,)" +
                                 R"("fuel_lb":)" + std::to_string(fuel) + R"(,"planned_flight_id":)" +
                                 std::to_string(planned) + R"(,"aircraft_folder":"pmdg 737-800bbj2"})");
}

std::string blockIn(const std::string& id, const std::string& time, double lat, double lon, double shutdownFuel) {
    return line("block_in", R"({"flight_id":")" + id + R"(","time":")" + time + R"(","lat":)" + std::to_string(lat) +
                                R"(,"lon":)" + std::to_string(lon) + R"(,"fuel_lb":9500,"shutdown_fuel_lb":)" +
                                std::to_string(shutdownFuel) + R"(,"block_time_s":10236})");
}

// A line with its own ts: the sim_* lines carry no data time.
std::string lineAt(const std::string& ts, const std::string& type, const std::string& data) {
    return R"({"ts":")" + ts + R"(","type":")" + type + R"(","data":)" + data + "}";
}

std::string gsxLine(const std::string& service, const std::string& state, const std::string& time) {
    return line("gsx_service", R"({"service":")" + service + R"(","state":")" + state + R"(","time":")" + time +
                                   R"("})");
}

std::string phaseLine(const std::string& id, const std::string& phase, const std::string& time) {
    return line("phase_changed", R"({"flight_id":")" + id + R"(","phase":")" + phase + R"(","time":")" + time +
                                     R"(","alt_ft":25,"flight_level":false})");
}

std::string interrupted(const std::string& id) {
    return line("flight_interrupted", R"({"flight_id":")" + id + R"(","reason":"sim_stopped"})");
}

class Settlement : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-settle-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    void writeLog(const std::vector<std::string>& lines, const std::string& day = "2026-11-23") {
        std::ofstream out(dir_ / ("events-" + day + ".jsonl"), std::ios::binary | std::ios::app);
        for (const std::string& l : lines) out << l << "\n";
    }

    // N814SX's delivery flight, KBFI to KSAN. Returns its planned flight id.
    std::int64_t planDelivery() {
        NewPlannedFlight f;
        f.airplaneId = loadFlightAirplane(db_, "N814SX")->airplaneId;
        f.originIcao = "KBFI";
        f.destinationIcao = "KSAN";
        f.flightNumber = "8";
        f.kind = FlightKind::Delivery;
        f.fuelPriceCentsPer100Lb = 4100;
        f.plannedAt = kPlannedAt;
        f.originRunway = "14R";
        std::string error;
        const auto id = createPlannedFlight(db_, f, &error);
        EXPECT_TRUE(id.has_value()) << error;
        return id.value_or(0);
    }

    std::int64_t count(const std::string& table) {
        db::Statement q(db_, "SELECT COUNT(*) FROM " + table);
        return q.next() ? q.integer(0) : -1;
    }

    std::string text(const std::string& sql) {
        db::Statement q(db_, sql);
        return q.next() ? q.text(0) : std::string("(none)");
    }

    std::int64_t integer(const std::string& sql) {
        db::Statement q(db_, sql);
        return q.next() ? q.integer(0) : -1;
    }

    fs::path dir_;
    db::Database db_ = test::testAirline();
};

std::vector<std::string> lines(const std::vector<MoneyLine>& money) {
    std::vector<std::string> out;
    for (const MoneyLine& m : money) out.push_back(m.category + " " + std::to_string(m.cents / 100));
    return out;
}

using Lines = std::vector<std::string>;

}  // namespace

TEST_F(Settlement, FindsTheMatchingPair) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    const auto f = findFlownFlight(dir_, id, kPlannedAt, {});
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->logFlightId, kOut);
    EXPECT_EQ(f->blockOutAt, "2026-11-23T10:30:05Z");
    EXPECT_EQ(f->blockInAt, "2026-11-23T13:20:41Z");
    EXPECT_EQ(f->fuelOutLb, 20000);
    EXPECT_EQ(f->fuelInLb, 9400);
    EXPECT_EQ(f->blockS, 10236);
    EXPECT_DOUBLE_EQ(f->inLat, kSanLat);
}

TEST_F(Settlement, InterruptedThenFlownAgain) {
    const auto id = planDelivery();
    const std::string second = "2026-11-23T11:05:00.000Z";
    writeLog({blockOut(kOut, id, 20000), interrupted(kOut), blockOut(second, id, 19000),
              blockIn(second, kIn, kSanLat, kSanLon, 9400)});
    const auto f = findFlownFlight(dir_, id, kPlannedAt, {});
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->logFlightId, second);
    EXPECT_EQ(f->fuelOutLb, 19000);
}

TEST_F(Settlement, IgnoresAnOlderFlightWithTheSameId) {
    const auto id = planDelivery();
    const std::string older = "2026-11-23T08:00:00.000Z";
    writeLog({blockOut(older, id, 20000), blockIn(older, "2026-11-23T09:00:00.000Z", kSanLat, kSanLon, 9400)});
    EXPECT_FALSE(findFlownFlight(dir_, id, kPlannedAt, {}).has_value());

    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    EXPECT_FALSE(findFlownFlight(dir_, id, kPlannedAt, {kOut}).has_value());
    EXPECT_TRUE(findFlownFlight(dir_, id, kPlannedAt, {}).has_value());
}

TEST_F(Settlement, OpenFlightIsNotFlown) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000)});
    EXPECT_FALSE(findFlownFlight(dir_, id, kPlannedAt, {}).has_value());
}

TEST_F(Settlement, AnotherPlannedFlightsPairIsNotThisOne) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id + 1, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    EXPECT_FALSE(findFlownFlight(dir_, id, kPlannedAt, {}).has_value());
}

TEST_F(Settlement, FlightAcrossUtcMidnight) {
    const auto id = planDelivery();
    writeLog({blockOut("2026-11-23T22:30:00.000Z", id, 20000)});
    writeLog({blockIn("2026-11-23T22:30:00.000Z", "2026-11-24T01:10:00.000Z", kSanLat, kSanLon, 9400)}, "2026-11-24");
    const auto f = findFlownFlight(dir_, id, kPlannedAt, {});
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->blockInAt, "2026-11-24T01:10:00Z");
}

TEST_F(Settlement, DeliveryFlight) {
    const auto id = planDelivery();
    const Cents cashBefore = cashCents(db_);
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    const auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    EXPECT_EQ(s->logFlightId, kOut);
    EXPECT_EQ(s->arrivalIcao, "KSAN");
    EXPECT_FALSE(s->diverted);
    EXPECT_TRUE(s->money.income.empty());
    const Lines costs = lines(s->money.costs);
    ASSERT_EQ(costs.size(), 5u) << ::testing::PrintToString(costs);
    EXPECT_EQ(costs[0], "fuel -8200");
    EXPECT_EQ(costs[1], "landing_fee -660");
    EXPECT_EQ(costs[2].rfind("atc -", 0), 0u);
    EXPECT_EQ(costs[3], "pilots -1420");       // 2 x $250 (VIP crew) x 2.843 h
    EXPECT_EQ(costs[4], "maintenance -855");   // $300 x 2.843 h

    EXPECT_EQ(count("flight"), 1);
    EXPECT_EQ(count("planned_flight"), 0);
    EXPECT_EQ(text("SELECT p.icao_code FROM airplane a JOIN airport p ON p.airport_id = a.location_airport_id"
                   " WHERE registration = 'N814SX'"),
              "KSAN");
    EXPECT_EQ(integer("SELECT fuel_on_board_lb FROM airplane WHERE registration = 'N814SX'"), 9400);
    EXPECT_EQ(integer("SELECT awaiting_delivery FROM airplane WHERE registration = 'N814SX'"), 0);
    EXPECT_EQ(cashCents(db_) - cashBefore, s->money.totalCents);
    EXPECT_EQ(count("flight_money_line"), 5);
    EXPECT_EQ(integer("SELECT COUNT(*) FROM ledger_line WHERE posted_at = '2026-11-23T13:20:41Z'"), 5);
    EXPECT_EQ(text("SELECT block_out_at || ' ' || block_in_at || ' ' || fuel_on_record_lb || ' ' || fuel_out_lb ||"
                   " ' ' || fuel_in_lb || ' ' || COALESCE(origin_runway, '-') FROM flight"),
              "2026-11-23T10:30:05Z 2026-11-23T13:20:41Z 0 20000 9400 14R");
}

TEST_F(Settlement, FuelAddedNotBurned) {
    const auto id = planDelivery();
    ASSERT_TRUE(db_.exec("UPDATE airplane SET fuel_on_board_lb = 12000 WHERE registration = 'N814SX'"));
    writeLog({blockOut(kOut, id, 18000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    EXPECT_EQ(lines(s->money.costs)[0], "fuel -2460");  // 6,000 lb added
    EXPECT_EQ(integer("SELECT fuel_on_record_lb FROM flight"), 12000);
}

TEST_F(Settlement, NoFuelLineWhenNoneWasAdded) {
    const auto id = planDelivery();
    ASSERT_TRUE(db_.exec("UPDATE airplane SET fuel_on_board_lb = 12000 WHERE registration = 'N814SX'"));
    writeLog({blockOut(kOut, id, 11800), blockIn(kOut, kIn, kSanLat, kSanLon, 3000)});
    std::string error;
    auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    for (const MoneyLine& m : s->money.costs) EXPECT_NE(m.category, "fuel");
}

TEST_F(Settlement, DiversionMovesToWhereItParked) {
    // A VIP flight, so there are fares to compare with the preview.
    NewPlannedFlight f;
    f.airplaneId = loadFlightAirplane(db_, "N738BT")->airplaneId;
    f.originIcao = "KBFI";
    f.destinationIcao = "KSAN";
    f.flightNumber = "101";
    f.kind = FlightKind::Passenger;
    f.cabins = {{"economy", 120}, {"business", 10}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = kPlannedAt;
    std::string error;
    const auto id = createPlannedFlight(db_, f, &error);
    ASSERT_TRUE(id.has_value()) << error;
    const Preview preview = pricePreview(priceInput(*currentPlannedFlight(db_)), Rates{});

    writeLog({blockOut(kOut, *id, 20000), blockIn(kOut, kIn, 47.001, -120.001, 9400)});  // at KSHT
    const auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    EXPECT_EQ(s->arrivalIcao, "KSHT");
    EXPECT_TRUE(s->diverted);
    EXPECT_EQ(lines(s->money.income), lines(preview.income));
    EXPECT_EQ(text("SELECT p.icao_code FROM airplane a JOIN airport p ON p.airport_id = a.location_airport_id"
                   " WHERE registration = 'N738BT'"),
              "KSHT");
    EXPECT_EQ(text("SELECT p.icao_code FROM flight f JOIN airport p ON p.airport_id = f.planned_destination_airport_id"),
              "KSAN");
}

TEST_F(Settlement, RunsOnceFromTheLog) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    const auto flights = count("flight"), lines = count("ledger_line");
    EXPECT_FALSE(settleIfFlown(db_, dir_, Rates{}, &error).has_value());
    EXPECT_EQ(error, "");
    EXPECT_EQ(count("flight"), flights);
    EXPECT_EQ(count("ledger_line"), lines);

    // A new planned flight that reuses the id doesn't take the settled pair either.
    const auto again = planDelivery();
    EXPECT_FALSE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << "planned id " << again;
    EXPECT_EQ(count("flight"), flights);
}

// --- Phase and GSX service times (milestone 8, D-137) ---------------------------------------------------------

TEST_F(Settlement, SettlingStoresPhases) {
    const auto id = planDelivery();
    const char* phases[] = {"pushback", "taxi_out", "takeoff", "climb",   "cruise",  "descent",
                            "approach", "go_around", "approach", "landing", "taxi_in", "arrived"};
    std::vector<std::string> log{blockOut(kOut, id, 20000)};
    for (int i = 0; i < 11; ++i) {
        const std::string t = "2026-11-23T1" + std::to_string(i / 6) + ":" + std::to_string(i % 6) + "0:05.500Z";
        log.push_back(phaseLine(kOut, phases[i], t));
    }
    log.push_back(phaseLine("2026-11-22T09:00:00.000Z", "climb", "2026-11-23T11:00:00.000Z"));  // another flight's
    // As the recorder writes them: block_in first, then the arrived phase (FlightTracker::onBlockIn).
    log.push_back(blockIn(kOut, kIn, kSanLat, kSanLon, 9400));
    log.push_back(phaseLine(kOut, "arrived", kIn));
    writeLog(log);
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight_phase"), 12);
    db::Statement q(db_, "SELECT seq, phase, started_at FROM flight_phase ORDER BY seq");
    for (int i = 0; i < 12; ++i) {
        ASSERT_TRUE(q.next());
        EXPECT_EQ(q.integer(0), i);
        EXPECT_EQ(q.text(1), phases[i]);
        if (i < 11) {
            EXPECT_EQ(q.text(2), "2026-11-23T1" + std::to_string(i / 6) + ":" + std::to_string(i % 6) + "0:05Z");
        } else {
            EXPECT_EQ(q.text(2), "2026-11-23T13:20:41Z");  // arrived is the block in
        }
    }
}

TEST_F(Settlement, SettlingStoresPreDepartureServices) {
    const auto id = planDelivery();
    writeLog({gsxLine("refuelling", "performing", "2026-11-23T10:01:00.300Z"),
              gsxLine("refuelling", "completed", "2026-11-23T10:06:00.300Z"),
              gsxLine("boarding", "performing", "2026-11-23T10:05:00.000Z"),
              gsxLine("boarding", "available", "2026-11-23T10:25:00.000Z"),
              blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight_service"), 2);
    EXPECT_EQ(text("SELECT started_at || ' ' || ended_at FROM flight_service WHERE service = 'refuelling'"),
              "2026-11-23T10:01:00Z 2026-11-23T10:06:00Z");
    EXPECT_EQ(text("SELECT started_at || ' ' || ended_at FROM flight_service WHERE service = 'boarding'"),
              "2026-11-23T10:05:00Z 2026-11-23T10:25:00Z");
}

TEST_F(Settlement, AbandonedServiceNotStored) {
    const auto id = planDelivery();
    writeLog({gsxLine("catering", "performing", "2026-11-23T10:01:00.000Z"),
              gsxLine("catering", "bypassed", "2026-11-23T10:02:00.000Z"),
              blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight_service"), 0);
}

TEST_F(Settlement, EarlierLegServicesNotStored) {
    const auto id = planDelivery();
    const std::string earlier = "2026-11-23T06:00:00.000Z";
    writeLog({gsxLine("refuelling", "performing", "2026-11-23T05:30:00.000Z"),
              gsxLine("refuelling", "completed", "2026-11-23T05:35:00.000Z"),
              line("block_out", R"({"flight_id":")" + earlier + R"(","time":")" + earlier + R"("})"),
              blockIn(earlier, "2026-11-23T08:00:00.000Z", kSanLat, kSanLon, 9000),
              gsxLine("boarding", "performing", "2026-11-23T10:05:00.000Z"),
              gsxLine("boarding", "available", "2026-11-23T10:25:00.000Z"),
              blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight_service"), 1);
    EXPECT_EQ(text("SELECT service FROM flight_service"), "boarding");
}

TEST_F(Settlement, ServicesBeforeSimStoppedNotStored) {
    const auto id = planDelivery();
    writeLog({gsxLine("refuelling", "performing", "2026-11-23T10:01:00.000Z"),
              gsxLine("refuelling", "completed", "2026-11-23T10:06:00.000Z"),
              lineAt("2026-11-23T10:10:00.000Z", "sim_stopped", "{}"),
              lineAt("2026-11-23T10:12:00.000Z", "sim_running", "{}"),
              blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight_service"), 0);
}

TEST_F(Settlement, TwoRefuelsBothStored) {
    const auto id = planDelivery();
    writeLog({gsxLine("refuelling", "performing", "2026-11-23T10:01:00.000Z"),
              gsxLine("refuelling", "completed", "2026-11-23T10:06:00.000Z"),
              gsxLine("refuelling", "available", "2026-11-23T10:06:10.000Z"),
              gsxLine("refuelling", "performing", "2026-11-23T10:15:00.000Z"),
              gsxLine("refuelling", "completed", "2026-11-23T10:18:00.000Z"),
              blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight_service"), 2);
}

std::vector<std::string> deboarding(const std::string& from, const std::string& to) {
    return {gsxLine("deboarding", "requested", from), gsxLine("deboarding", "performing", from),
            gsxLine("deboarding", "completed", to)};
}

TEST_F(Settlement, DeboardingAfterBlockInRecorded) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    writeLog(deboarding("2026-11-23T13:22:00.400Z", "2026-11-23T13:28:00.400Z"));
    EXPECT_EQ(recordDeboardings(db_, dir_), 1);
    EXPECT_EQ(text("SELECT service || ' ' || started_at || ' ' || ended_at FROM flight_service"),
              "deboarding 2026-11-23T13:22:00Z 2026-11-23T13:28:00Z");
}

TEST_F(Settlement, DeboardingRecordedOnce) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    writeLog(deboarding("2026-11-23T13:22:00.000Z", "2026-11-23T13:28:00.000Z"));
    EXPECT_EQ(recordDeboardings(db_, dir_), 1);
    EXPECT_EQ(recordDeboardings(db_, dir_), 0);
    EXPECT_EQ(count("flight_service"), 1);
}

TEST_F(Settlement, DeboardingForUnsettledFlightSkipped) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    writeLog(deboarding("2026-11-23T13:22:00.000Z", "2026-11-23T13:28:00.000Z"));
    EXPECT_EQ(recordDeboardings(db_, dir_), 0);  // not settled
    EXPECT_EQ(count("flight_service"), 0);
}

TEST_F(Settlement, DeboardingStillRunningNotRecorded) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    writeLog({gsxLine("deboarding", "performing", "2026-11-23T13:22:00.000Z")});
    EXPECT_EQ(recordDeboardings(db_, dir_), 0);
}

TEST_F(Settlement, DeboardingAfterNewPlanStillRecorded) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    planDelivery();  // Dave plans the next leg while the passengers walk off
    writeLog(deboarding("2026-11-23T13:22:00.000Z", "2026-11-23T13:28:00.000Z"));
    EXPECT_EQ(recordDeboardings(db_, dir_), 1);
    EXPECT_EQ(text("SELECT f.log_flight_id FROM flight_service s JOIN flight f USING (flight_id)"), kOut);
}

TEST_F(Settlement, KeepsThePlanAndCabins) {
    NewPlannedFlight f;
    f.airplaneId = loadFlightAirplane(db_, "N814SX")->airplaneId;
    f.originIcao = "KBFI";
    f.destinationIcao = "KSAN";
    f.flightNumber = "8";
    f.kind = FlightKind::Vip;
    f.cabins = {{"economy", 40}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = kPlannedAt;
    std::string error;
    const auto id = createPlannedFlight(db_, f, &error);
    ASSERT_TRUE(id.has_value()) << error;
    SimbriefPlan p;
    p.requestId = "r1";
    p.generatedAt = "2026-11-23T10:05:00Z";
    p.route = "SEA J1 SAN";
    p.originRunway = "14R";
    p.destinationRunway = "27";
    p.cruiseAltitudeFt = 41000;
    p.estBlockS = 10320;
    p.rampFuelLb = 21000;
    p.enrouteBurnLb = 10493;
    p.estLandingWeightLb = 120000;
    p.originTransitionAltitudeFt = 18000;
    p.destinationTransitionLevelFt = 18000;
    ASSERT_TRUE(storeSimbriefPlan(db_, *id, p, R"({"ofp":1})", "2026-11-23T10:06:00Z", &error)) << error;

    writeLog({blockOut(kOut, *id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(text("SELECT route || ' ' || ofp_json FROM flight_plan"), R"(SEA J1 SAN {"ofp":1})");
    EXPECT_EQ(text("SELECT cabin_class || ' ' || passengers FROM flight_cabin"), "economy 40");
    EXPECT_EQ(count("simbrief_plan"), 0);
}

TEST_F(Settlement, FailureWritesNothing) {
    const auto id = planDelivery();
    const auto planned = currentPlannedFlight(db_);
    FlownFlight bad;
    bad.logFlightId = kOut;
    bad.blockOutAt = "2026-11-23T13:20:41Z";
    bad.blockInAt = "2026-11-23T10:30:05Z";  // before block out: the flight row is refused
    bad.fuelOutLb = 20000;
    bad.fuelInLb = 9400;
    bad.inLat = kSanLat;
    bad.inLon = kSanLon;
    bad.blockS = 10236;
    const auto linesBefore = count("ledger_line");
    std::string error;
    EXPECT_FALSE(settleFlight(db_, *planned, bad, Rates{}, &error).has_value());
    EXPECT_NE(error, "");
    EXPECT_EQ(count("flight"), 0);
    EXPECT_EQ(count("ledger_line"), linesBefore);
    EXPECT_EQ(count("planned_flight"), 1);
    EXPECT_EQ(integer("SELECT awaiting_delivery FROM airplane WHERE registration = 'N814SX'"), 1);
    (void)id;
}

TEST_F(Settlement, NearestAirportAndHours) {
    const auto near = nearestAirport(db_, kSanLat + 0.009, kSanLon);  // about 1 km north
    ASSERT_TRUE(near.has_value());
    EXPECT_EQ(near->icao, "KSAN");
    EXPECT_NEAR(near->distanceM, 1000, 10);

    EXPECT_DOUBLE_EQ(hoursFlown(db_), 0.0);
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, "2026-11-23T12:00:05.120Z", kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_DOUBLE_EQ(hoursFlown(db_), 1.5);
    EXPECT_EQ(settledLogFlightIds(db_), (std::set<std::string>{kOut}));
}

TEST_F(Settlement, LedgerTextForFlightLines) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    const auto ledger = listLedger(db_);
    ASSERT_FALSE(ledger.empty());
    EXPECT_EQ(ledger[0].postedAt, "2026-11-23T13:20:41Z");
    bool sawFuel = false;
    for (const LedgerView& l : ledger) sawFuel |= l.text == "N814SX KBFI–KSAN · Fuel";
    EXPECT_TRUE(sawFuel);
}

TEST_F(Settlement, ListFlightsReadsLinesBack) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    const auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    const auto flights = listFlights(db_);
    ASSERT_EQ(flights.size(), 1u);
    const FlightView& f = flights[0];
    EXPECT_EQ(f.logFlightId, kOut);
    EXPECT_EQ(f.flightNumber, "8");
    EXPECT_EQ(f.registration, "N814SX");
    EXPECT_EQ(f.typeCode, "737-800 BBJ2");
    EXPECT_EQ(f.kind, "delivery");
    EXPECT_EQ(f.origin, "KBFI");
    EXPECT_EQ(f.plannedDestination, "KSAN");
    EXPECT_EQ(f.arrival, "KSAN");
    EXPECT_DOUBLE_EQ(f.originLat, 47.53);
    EXPECT_DOUBLE_EQ(f.arrivalLat, kSanLat);
    EXPECT_EQ(f.blockOutAt, "2026-11-23T10:30:05Z");
    EXPECT_EQ(f.blockInAt, "2026-11-23T13:20:41Z");
    EXPECT_EQ(f.blockS, 10236);
    EXPECT_EQ(f.fuelOutLb, 20000);
    EXPECT_EQ(f.fuelInLb, 9400);
    EXPECT_EQ(lines(f.income), lines(s->money.income));
    EXPECT_EQ(lines(f.costs), lines(s->money.costs));
    EXPECT_EQ(f.costs[0].label, "Fuel");
    EXPECT_EQ(f.totalCents, s->money.totalCents);
}
