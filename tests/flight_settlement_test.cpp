// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "airline_queries.h"
#include "block_detector.h"
#include "event_log.h"
#include "debrief_measure.h"
#include "debrief_store.h"
#include "flight_events.h"
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

// The block lines are built by the recorder's own toJson (review 1.10), so they are what EventLog really writes.
std::string blockOut(const std::string& id, std::int64_t planned, double fuel) {
    BlockOut e;
    e.flightId = id;
    e.time = *parseUtc(id);
    e.latDeg = 47.53;
    e.lonDeg = -122.302;
    e.fuelLb = fuel;
    e.plannedFlightId = planned;
    e.aircraftFolder = "pmdg 737-800bbj2";
    return line("block_out", toJson(BlockEvent{e}).dump());
}

std::string blockIn(const std::string& id, const std::string& time, double lat, double lon, double shutdownFuel) {
    BlockIn e;
    e.flightId = id;
    e.time = *parseUtc(time);
    e.shutdownTime = e.time;
    e.latDeg = lat;
    e.lonDeg = lon;
    e.fuelLb = 9500;
    e.shutdownFuelLb = shutdownFuel;
    e.blockTimeS = 10236;
    return line("block_in", toJson(BlockEvent{e}).dump());
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

// Review 1.2: the lookback isn't the last 7 files but every file from the planned day on, so a flight left unsettled
// for more than a week of logs still settles.
TEST_F(Settlement, FoundAfterMoreThanAWeekOfLogs) {
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    for (int day = 24; day <= 30; ++day) writeLog({line("recorder_started", "{}")}, "2026-11-" + std::to_string(day));
    for (int day = 1; day <= 3; ++day) writeLog({line("recorder_started", "{}")}, "2026-12-0" + std::to_string(day));
    const auto f = findFlownFlight(dir_, id, kPlannedAt, {});
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->logFlightId, kOut);
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

// A $0 fuel line is shown but never posted: the ledger holds no $0 lines.
TEST_F(Settlement, NoFuelPostedWhenNoneWasAdded) {
    const auto id = planDelivery();
    ASSERT_TRUE(db_.exec("UPDATE airplane SET fuel_on_board_lb = 12000 WHERE registration = 'N814SX'"));
    writeLog({blockOut(kOut, id, 11800), blockIn(kOut, kIn, kSanLat, kSanLon, 3000)});
    std::string error;
    auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    for (const MoneyLine& m : s->money.costs) {
        if (m.category == "fuel") EXPECT_EQ(m.cents, 0);
    }
    db::Statement q(db_, "SELECT count(*) FROM ledger_line WHERE category = 'fuel'");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 0);
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

// The replay shows a settled flight as it was planned (D-175).
TEST_F(Settlement, FlownFlightViewMatchesThePlan) {
    NewPlannedFlight f;
    f.airplaneId = loadFlightAirplane(db_, "N814SX")->airplaneId;
    f.originIcao = "KBFI";
    f.destinationIcao = "KSAN";
    f.flightNumber = "8";
    f.kind = FlightKind::Vip;
    f.cabins = {{"economy", 40}};
    f.fuelPriceCentsPer100Lb = 4100;
    f.plannedAt = kPlannedAt;
    f.originRunway = "14R";
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
    const PlannedFlightView planned = *currentPlannedFlight(db_);

    writeLog({blockOut(kOut, *id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_FALSE(flownFlightView(db_, "no such flight"));
    const auto flown = flownFlightView(db_, kOut);
    ASSERT_TRUE(flown.has_value());
    EXPECT_EQ(flown->registration, planned.registration);
    EXPECT_EQ(flown->typeCode, planned.typeCode);
    EXPECT_EQ(flown->airframeId, planned.airframeId);
    EXPECT_EQ(flown->origin, "KBFI");
    EXPECT_EQ(flown->originName, planned.originName);
    EXPECT_EQ(flown->destination, "KSAN");
    EXPECT_EQ(flown->destinationName, planned.destinationName);
    EXPECT_EQ(flown->originCity, planned.originCity);
    EXPECT_EQ(flown->destinationCity, planned.destinationCity);
    EXPECT_FALSE(flown->destinationCity.empty());
    EXPECT_EQ(flown->flightNumber, "8");
    EXPECT_EQ(flown->kind, FlightKind::Vip);
    EXPECT_EQ(flown->seatsInstalled, planned.seatsInstalled);
    EXPECT_EQ(flown->cruiseSpeedKt, planned.cruiseSpeedKt);
    EXPECT_EQ(flown->limits.mlwLb, planned.limits.mlwLb);
    ASSERT_EQ(flown->cabins.size(), 1u);
    EXPECT_EQ(flown->cabins[0].passengers, 40);
    EXPECT_EQ(flown->cargoLb, 0);
    EXPECT_DOUBLE_EQ(flown->distanceNm, planned.distanceNm);
    EXPECT_TRUE(flown->brief.has_value());  // the Pre-Flight panel on a replay (D-188)
    EXPECT_EQ(flown->holds.size(), planned.holds.size());
    EXPECT_FALSE(flown->holds.empty());
    EXPECT_EQ(flown->fuelPriceCentsPer100Lb, 4100);
    EXPECT_EQ(flown->plannedAt, kPlannedAt);
    EXPECT_EQ(flown->originRunway, "14R");
    EXPECT_EQ(flown->destinationRunway, "");
    ASSERT_TRUE(flown->plan.has_value());
    EXPECT_EQ(flown->plan->route, "SEA J1 SAN");
    EXPECT_EQ(flown->plan->cruiseAltitudeFt, 41000);
    EXPECT_EQ(flown->plan->estBlockS, 10320);
    EXPECT_EQ(flown->plan->enrouteBurnLb, 10493);
    EXPECT_EQ(flown->plan->estLandingWeightLb, 120000);
    EXPECT_EQ(flown->plan->destinationIcao, "KSAN");
}

TEST_F(Settlement, LatestFlightId) {
    EXPECT_FALSE(latestFlightId(db_));
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(latestFlightId(db_), kOut);
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

    EXPECT_DOUBLE_EQ(hoursFlown(db_).value_or(-1), 0.0);
    const auto id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, "2026-11-23T12:00:05.120Z", kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_DOUBLE_EQ(hoursFlown(db_).value_or(-1), 1.5);
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

namespace {

// The block line with a parking spot, as the recorder writes it from 0.10.0 (D-148).
std::string withParking(std::string line, const std::string& parking) {
    line.insert(line.size() - 2, R"(,"parking":")" + parking + R"(")");
    return line;
}

}  // namespace

TEST_F(Settlement, ParkingStored) {
    const auto id = planDelivery();
    writeLog({withParking(blockOut(kOut, id, 20000), "Gate D14"),
              withParking(blockIn(kOut, kIn, kSanLat, kSanLon, 9400), "Parking 70")});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(text("SELECT origin_parking FROM flight"), "Gate D14");
    EXPECT_EQ(text("SELECT arrival_parking FROM flight"), "Parking 70");
}

TEST_F(Settlement, NoParkingStoredAsNull) {
    const auto id = planDelivery();
    writeLog({withParking(blockOut(kOut, id, 20000), ""), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(integer("SELECT count(*) FROM flight WHERE origin_parking IS NULL AND arrival_parking IS NULL"), 1);
}

namespace {

constexpr const char* kFixtureFlight = "2026-11-24T23:53:16.837Z";

// The KLAS-KSAN fixture (tests/data/README.md) as this test's log, flown as planned flight `id`.
void copyFixture(const fs::path& dir, std::int64_t id) {
    std::ifstream in(BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan/events-2026-11-24.jsonl", std::ios::binary);
    std::ofstream out(dir / "events-2026-11-24.jsonl", std::ios::binary);
    const std::string from = "\"planned_flight_id\":1,";
    std::string l;
    while (std::getline(in, l)) {
        const auto at = l.find(from);
        if (at != std::string::npos) l.replace(at, from.size(), "\"planned_flight_id\":" + std::to_string(id) + ",");
        out << l << "\n";
    }
}

}  // namespace

TEST_F(Settlement, WritesMeasurementsInTheTransaction) {
    copyFixture(dir_, planDelivery());
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    const auto stored = loadMeasurements(db_);
    ASSERT_EQ(stored.size(), 1u);
    const FlightMeasurements& m = stored.at(kFixtureFlight);
    const FlightMeasurements expected = measureFlight(loadFlightEvents(dir_, kFixtureFlight), false);
    EXPECT_EQ(m.family, expected.family);
    ASSERT_EQ(m.values.size(), expected.values.size());
    for (const auto& v : expected.values) {
        const auto it =
            std::find_if(m.values.begin(), m.values.end(), [&](const CheckValue& x) { return x.part == v.part; });
        ASSERT_NE(it, m.values.end()) << v.part;
        EXPECT_EQ(it->value, v.value) << v.part;
    }
    EXPECT_TRUE(m.skipped.empty());
    ASSERT_TRUE(m.landing);
    EXPECT_EQ(m.landing->vsFpm, 591);
    EXPECT_EQ(m.landing->vrefKt, 125);
    EXPECT_FALSE(m.landing->simPitchTenths);
    EXPECT_EQ(m.landing->offCentrelineDm, 27);
}

// The money, fuel and block time on a real log (review 6.1): catches a block event field renamed in the writer.
TEST_F(Settlement, RealLogSettlesTheMoneyFuelAndTimes) {
    copyFixture(dir_, planDelivery());
    std::string error;
    const auto settled = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(settled.has_value()) << error;
    EXPECT_EQ(settled->arrivalIcao, "KSAN");
    EXPECT_EQ(integer("SELECT fuel_out_lb FROM flight"), 10984);
    EXPECT_EQ(integer("SELECT fuel_in_lb FROM flight"), 5906);
    EXPECT_EQ(integer("SELECT fuel_on_board_lb FROM airplane WHERE registration = 'N814SX'"), 5906);
    EXPECT_EQ(text("SELECT block_out_at || ' ' || block_in_at FROM flight"),
              "2026-11-24T23:53:16Z 2026-11-25T00:52:48Z");
    EXPECT_EQ(text("SELECT origin_parking || ' ' || arrival_parking FROM flight"), "Gate E7 Gate 5");
    // Crew and maintenance are charged for the block time the block_in reports (3,571 s), fuel for what was added.
    const auto byCategory = [&](const std::string& category) {
        return integer("SELECT l.amount_cents FROM ledger_line l JOIN flight_money_line USING (ledger_line_id)"
                       " WHERE l.category = '" + category + "'");
    };
    EXPECT_LT(byCategory("fuel"), 0);
    EXPECT_LT(byCategory("pilots"), 0);
    EXPECT_LT(byCategory("maintenance"), 0);
}

// Some errors (a full disk, an I/O error) make SQLite roll back the whole transaction from inside the debrief's
// savepoint. Settlement must then fail, not go on writing money line by line in autocommit (review 1.3).
TEST_F(Settlement, TransactionLostInsideTheDebriefPostsNothing) {
    copyFixture(dir_, planDelivery());
    ASSERT_TRUE(db_.exec("CREATE TEMP TRIGGER lose_tx BEFORE INSERT ON flight_landing BEGIN"
                         " SELECT RAISE(ROLLBACK, 'test'); END;"));
    const auto linesBefore = count("ledger_line");
    std::string error;
    EXPECT_FALSE(settleIfFlown(db_, dir_, Rates{}, &error).has_value());
    EXPECT_NE(error, "");
    EXPECT_EQ(count("ledger_line"), linesBefore);
    EXPECT_EQ(count("flight"), 0);
    EXPECT_EQ(count("planned_flight"), 1);
}

// D-160, D-200: check 2's window moved to the transition altitude for new flights; a stored measurement is never
// measured again, so an old flight keeps the value it was settled with.
TEST_F(Settlement, StoredMeasurementIsNotRemeasured) {
    copyFixture(dir_, planDelivery());
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    // As an older BlockTime would have measured it (to 10,000 ft).
    ASSERT_TRUE(db_.exec("UPDATE flight_check_value SET value = 0 WHERE part = 'belts_off_departure_s'", &error))
        << error;
    EXPECT_FALSE(settleIfFlown(db_, dir_, Rates{}, &error).has_value());  // already settled
    const FlightMeasurements m = loadMeasurements(db_).at(kFixtureFlight);
    const auto it = std::find_if(m.values.begin(), m.values.end(),
                                 [](const CheckValue& v) { return v.part == "belts_off_departure_s"; });
    ASSERT_NE(it, m.values.end());
    EXPECT_EQ(it->value, 0);
}

TEST_F(Settlement, SkipsAreStoredWithTheirReason) {
    const std::int64_t id = planDelivery();
    writeLog({blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    const FlightMeasurements m = loadMeasurements(db_).at(kOut);
    EXPECT_EQ(m.values.size() + m.skipped.size(), kCheckParts.size());
    EXPECT_FALSE(m.skipped.empty());
    EXPECT_FALSE(m.landing);
}

TEST_F(Settlement, DebriefFailureStillSettles) {
    // The debrief is extra; the money isn't: a debrief that can't be written leaves the flight settled without one.
    copyFixture(dir_, planDelivery());
    ASSERT_TRUE(db_.exec("CREATE TRIGGER no_landing BEFORE INSERT ON flight_landing BEGIN"
                         " SELECT RAISE(ABORT, 'test'); END;"));
    std::string error;
    EXPECT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight"), 1);
    EXPECT_GT(count("flight_money_line"), 0);
    EXPECT_EQ(count("flight_debrief"), 0);
    EXPECT_EQ(count("flight_check_value"), 0);
}

TEST_F(Settlement, MalformedFieldStillMeasures) {
    // A field of the wrong type (a flag that isn't a bool) reads as its fallback (review 1.6): the flight settles with
    // its debrief, where it used to throw and settle without one.
    const std::int64_t id = planDelivery();
    writeLog({blockOut(kOut, id, 20000),
              line("cockpit", R"({"flight_id":")" + std::string(kOut) + R"(","pmdg":"yes","seat_belts":2})"),
              blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    EXPECT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    EXPECT_EQ(count("flight"), 1);
    EXPECT_EQ(count("flight_debrief"), 1);
}

TEST_F(Settlement, ReviewStoredOnce) {
    copyFixture(dir_, planDelivery());
    std::string error;
    ASSERT_TRUE(settleIfFlown(db_, dir_, Rates{}, &error).has_value()) << error;
    StoredReview r{"A tidy flight.", "Firm at 591 fpm.", "Levelled at FL360 as cleared.", "claude-opus-5-5",
                   "2026-11-25T00:55:00Z", {"Beacon before the push", "Landing lights at 10,000 ft"}};
    EXPECT_TRUE(storeReview(db_, kFixtureFlight, r, &error)) << error;
    StoredReview other = r;
    other.summary = "Something else.";
    EXPECT_FALSE(storeReview(db_, kFixtureFlight, other, &error));
    EXPECT_FALSE(storeReview(db_, "no-such-flight", r, &error));
    const auto reviews = loadReviews(db_);
    ASSERT_EQ(reviews.size(), 1u);
    const StoredReview& got = reviews.at(kFixtureFlight);
    EXPECT_EQ(got.summary, "A tidy flight.");
    EXPECT_EQ(got.workOn, r.workOn);
    EXPECT_EQ(got.model, "claude-opus-5-5");
    EXPECT_EQ(count("flight_review_item"), 2);
}

// D-236: the flight_accepted marker and its snapshots before block_out don't change what settles.
TEST_F(Settlement, IgnoresAcceptedLines) {
    const auto id = planDelivery();
    const std::string marker = line("flight_accepted", R"({"planned_flight_id":)" + std::to_string(id) +
                                                           R"(,"planned_at":"2026-11-23T10:00:00Z","registration":)"
                                                           R"("N814SX","origin":"KBFI","destination":"KSAN",)"
                                                           R"("at_block_out":false})");
    const std::string cockpit = line("cockpit", R"({"beacon":false,"landing_lights":false})");
    const std::string telemetry = line("telemetry", R"({"lat":47.53,"lon":-122.302,"fuel_lb":20000})");
    writeLog({marker, cockpit, telemetry, blockOut(kOut, id, 20000), blockIn(kOut, kIn, kSanLat, kSanLon, 9400)});
    std::string error;
    const auto s = settleIfFlown(db_, dir_, Rates{}, &error);
    ASSERT_TRUE(s.has_value()) << error;
    EXPECT_EQ(s->logFlightId, kOut);
    const Lines costs = lines(s->money.costs);
    ASSERT_EQ(costs.size(), 5u) << ::testing::PrintToString(costs);
    EXPECT_EQ(costs[0], "fuel -8200");
    EXPECT_EQ(costs[3], "pilots -1420");
}
