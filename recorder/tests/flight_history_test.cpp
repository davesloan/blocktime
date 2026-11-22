// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "event_log.h"
#include "flight_history.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

// Log lines in the real format (D-020), as EventLog writes them. A block_out's flight_id is its time (D-031).
std::string blockOut(const std::string& id, double lat, double lon, double fuelLb = 15315.6,
                     const std::string& atc = "N818SX") {
    return std::format(
        R"({{"ts":"{}","type":"block_out","data":{{"atc_id":"{}","flight_id":"{}","fuel_lb":{},"lat":{},"lon":{},"time":"{}","title":"PMDG 737-800 Soxxly"}}}})",
        id, atc, id, fuelLb, lat, lon, id);
}
std::string tel(const std::string& ts, double lat, double lon, double altFt = 2000) {
    return std::format(R"({{"ts":"{}","type":"telemetry","data":{{"alt_ft":{},"lat":{},"lon":{},"on_ground":false}}}})",
                       ts, altFt, lat, lon);
}
std::string blockIn(const std::string& id, const std::string& ts, double lat, double lon, double fuelLb = 12141.0,
                    int blockS = 1612) {
    return std::format(
        R"({{"ts":"{}","type":"block_in","data":{{"block_time_s":{},"flight_id":"{}","fuel_lb":{},"lat":{},"lon":{},"time":"{}"}}}})",
        ts, blockS, id, fuelLb, lat, lon, ts);
}
std::string interrupted(const std::string& id, const std::string& ts, const std::string& reason) {
    return std::format(R"({{"ts":"{}","type":"flight_interrupted","data":{{"flight_id":"{}","reason":"{}"}}}})", ts,
                       id, reason);
}

// Degrees of latitude per metre on the sphere geo.h uses (radius 6,371,000 m).
constexpr double kDegPerMeter = 180.0 / (std::numbers::pi * 6371000.0);
constexpr double kMetersPerNm = 1852.0;

class FlightHistory : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    void file(const std::string& name, const std::vector<std::string>& lines) {
        std::ofstream out(dir_ / name, std::ios::binary);
        for (const auto& l : lines) out << l << '\n';
    }

    fs::path dir_;
};

constexpr const char* kDay = "events-2026-11-22.jsonl";
const std::string kA = "2026-11-22T21:28:23.001Z";
const std::string kB = "2026-11-22T22:40:00.000Z";

}  // namespace

TEST_F(FlightHistory, DailyLogFilesOldestFirstOnlyDailyNames) {
    file("events-2026-11-23.jsonl", {});
    file("events-2026-11-22.jsonl", {});
    file("events-2026-11-2x.jsonl", {});
    file("notes.txt", {});
    const auto files = dailyLogFiles(dir_);
    ASSERT_EQ(files.size(), 2u);
    EXPECT_EQ(files[0].filename(), "events-2026-11-22.jsonl");
    EXPECT_EQ(files[1].filename(), "events-2026-11-23.jsonl");
}

TEST_F(FlightHistory, CompleteFlight) {
    file(kDay, {blockOut(kA, 32.730, -117.200), tel("2026-11-22T21:29:00.000Z", 32.731, -117.200),
                tel("2026-11-22T21:30:00.000Z", 32.732, -117.200), tel("2026-11-22T21:31:00.000Z", 32.733, -117.200),
                blockIn(kA, "2026-11-22T21:55:15.475Z", 32.734, -117.200)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    const auto& f = flights[0];
    EXPECT_EQ(f.flightId, kA);
    EXPECT_EQ(f.atcId, "N818SX");
    EXPECT_EQ(f.title, "PMDG 737-800 Soxxly");
    EXPECT_EQ(f.outTime, kA);
    EXPECT_DOUBLE_EQ(f.outFuelLb, 15315.6);
    EXPECT_EQ(f.end, FlightEnd::BlockIn);
    EXPECT_EQ(f.endTime, "2026-11-22T21:55:15.475Z");
    EXPECT_EQ(f.endReason, "");
    ASSERT_TRUE(f.blockTimeS.has_value());
    EXPECT_EQ(*f.blockTimeS, 1612);
    ASSERT_TRUE(f.inFuelLb.has_value());
    EXPECT_DOUBLE_EQ(*f.inFuelLb, 12141.0);
    ASSERT_EQ(f.track.size(), 5u);
    EXPECT_DOUBLE_EQ(f.track.front().lat, 32.730);
    EXPECT_DOUBLE_EQ(f.track.back().lat, 32.734);
    EXPECT_DOUBLE_EQ(f.track.back().lon, -117.200);
}

TEST_F(FlightHistory, NewestFirst) {
    file(kDay, {blockOut(kA, 32.73, -117.2), blockIn(kA, "2026-11-22T21:55:00.000Z", 32.74, -117.2),
                blockOut(kB, 32.74, -117.2), blockIn(kB, "2026-11-22T23:10:00.000Z", 32.75, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 2u);
    EXPECT_EQ(flights[0].flightId, kB);
    EXPECT_EQ(flights[1].flightId, kA);
}

TEST_F(FlightHistory, InterruptedFlight) {
    file(kDay, {blockOut(kA, 32.73, -117.2), tel("2026-11-22T21:29:00.000Z", 32.74, -117.2),
                interrupted(kA, "2026-11-22T21:31:50.032Z", "sim_stopped")});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    EXPECT_EQ(flights[0].end, FlightEnd::Interrupted);
    EXPECT_EQ(flights[0].endReason, "sim_stopped");
    EXPECT_EQ(flights[0].endTime, "2026-11-22T21:31:50.032Z");
    EXPECT_FALSE(flights[0].blockTimeS.has_value());
    EXPECT_FALSE(flights[0].inFuelLb.has_value());
}

TEST_F(FlightHistory, OpenFlightAtEnd) {
    file(kDay, {blockOut(kA, 32.73, -117.2), tel("2026-11-22T21:29:00.000Z", 32.74, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    EXPECT_EQ(flights[0].end, FlightEnd::Open);
    EXPECT_EQ(flights[0].endTime, "");
    EXPECT_EQ(flights[0].track.size(), 2u);
}

TEST_F(FlightHistory, UnclosedFlightBeforeNewBlockOut) {
    file(kDay, {blockOut(kA, 32.73, -117.2), tel("2026-11-22T21:29:00.000Z", 32.74, -117.2),
                blockOut(kB, 32.75, -117.2), tel("2026-11-22T22:41:00.000Z", 32.76, -117.2),
                blockIn(kB, "2026-11-22T23:10:00.000Z", 32.77, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 2u);
    EXPECT_EQ(flights[0].flightId, kB);
    EXPECT_EQ(flights[0].end, FlightEnd::BlockIn);
    EXPECT_EQ(flights[0].track.size(), 3u);
    EXPECT_EQ(flights[1].flightId, kA);
    EXPECT_EQ(flights[1].end, FlightEnd::Interrupted);
    EXPECT_EQ(flights[1].endReason, "unclosed");
    EXPECT_EQ(flights[1].endTime, kB);  // when the next block_out showed it was never closed
    EXPECT_EQ(flights[1].track.size(), 2u);
}

TEST_F(FlightHistory, CrossesUtcMidnight) {
    const std::string id = "2026-11-22T23:50:00.000Z";
    file("events-2026-11-22.jsonl", {blockOut(id, 32.73, -117.2), tel("2026-11-22T23:55:00.000Z", 32.74, -117.2),
                                     tel("2026-11-22T23:59:00.000Z", 32.75, -117.2)});
    file("events-2026-11-23.jsonl", {tel("2026-11-23T00:01:00.000Z", 32.76, -117.2),
                                     tel("2026-11-23T00:05:00.000Z", 32.77, -117.2),
                                     blockIn(id, "2026-11-23T00:20:00.000Z", 32.78, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    EXPECT_EQ(flights[0].end, FlightEnd::BlockIn);
    EXPECT_EQ(flights[0].track.size(), 6u);
}

TEST_F(FlightHistory, TornLineInsideFlight) {
    file(kDay, {blockOut(kA, 32.73, -117.2), tel("2026-11-22T21:29:00.000Z", 32.74, -117.2),
                R"({"ts":"2026-11-22T21:3)", tel("2026-11-22T21:31:00.000Z", 32.75, -117.2),
                blockIn(kA, "2026-11-22T21:55:00.000Z", 32.76, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    EXPECT_EQ(flights[0].end, FlightEnd::BlockIn);
    EXPECT_EQ(flights[0].track.size(), 4u);
}

TEST_F(FlightHistory, TelemetryOutsideFlightIgnored) {
    file(kDay, {tel("2026-11-22T21:20:00.000Z", 10.0, 10.0), blockOut(kA, 32.73, -117.2),
                tel("2026-11-22T21:29:00.000Z", 32.74, -117.2),
                blockIn(kA, "2026-11-22T21:55:00.000Z", 32.75, -117.2), tel("2026-11-22T22:00:00.000Z", 20.0, 20.0)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    EXPECT_EQ(flights[0].track.size(), 3u);
}

TEST_F(FlightHistory, DistanceAndMaxAltitude) {
    // Six points in a line along a meridian, each 1 nm apart: 5 nm in total.
    const double step = kMetersPerNm * kDegPerMeter;
    file(kDay, {blockOut(kA, 32.0, -117.2), tel("2026-11-22T21:29:00.000Z", 32.0 + step, -117.2, 1000),
                tel("2026-11-22T21:30:00.000Z", 32.0 + 2 * step, -117.2, 5000),
                tel("2026-11-22T21:31:00.000Z", 32.0 + 3 * step, -117.2, 3000),
                tel("2026-11-22T21:32:00.000Z", 32.0 + 4 * step, -117.2, 800),
                blockIn(kA, "2026-11-22T21:55:00.000Z", 32.0 + 5 * step, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    EXPECT_NEAR(flights[0].distanceNm, 5.0, 0.01);
    EXPECT_DOUBLE_EQ(flights[0].maxAltFt, 5000);
}

TEST_F(FlightHistory, TrackThinnedTo2000) {
    // 5,000 samples 100 m apart. Every one is kept by spacing, so evenly thinning has to bring it down.
    const double step = 100 * kDegPerMeter;
    std::vector<std::string> lines{blockOut(kA, 32.0, -117.2)};
    for (int i = 1; i <= 5000; ++i) lines.push_back(tel("2026-11-22T21:30:00.000Z", 32.0 + i * step, -117.2));
    lines.push_back(blockIn(kA, "2026-11-22T23:55:00.000Z", 32.0 + 5001 * step, -117.2));
    file(kDay, lines);
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    const auto& f = flights[0];
    EXPECT_LE(f.track.size(), kMaxTrackPoints);
    EXPECT_GT(f.track.size(), kMaxTrackPoints / 2);
    EXPECT_DOUBLE_EQ(f.track.front().lat, 32.0);
    EXPECT_DOUBLE_EQ(f.track.back().lat, 32.0 + 5001 * step);
    EXPECT_NEAR(f.distanceNm, 5001 * 100 / kMetersPerNm, 0.01);  // every sample still counts
}

TEST_F(FlightHistory, CloseSamplesSkipped) {
    // Taxi creep: samples 5 m apart. Only those 25 m or more from the last kept point stay.
    const double m = kDegPerMeter;
    file(kDay, {blockOut(kA, 32.0, -117.2), tel("2026-11-22T21:29:00.000Z", 32.0 + 5 * m, -117.2),
                tel("2026-11-22T21:29:01.000Z", 32.0 + 10 * m, -117.2),
                tel("2026-11-22T21:29:02.000Z", 32.0 + 15 * m, -117.2),
                tel("2026-11-22T21:29:03.000Z", 32.0 + 20 * m, -117.2),
                tel("2026-11-22T21:29:04.000Z", 32.0 + 30 * m, -117.2),
                blockIn(kA, "2026-11-22T21:55:00.000Z", 32.0 + 60 * m, -117.2)});
    const auto flights = loadFlightHistory(dir_);
    ASSERT_EQ(flights.size(), 1u);
    ASSERT_EQ(flights[0].track.size(), 3u);  // block out, the +30 m sample, block in
    EXPECT_NEAR(flights[0].track[1].lat, 32.0 + 30 * m, 1e-12);
    EXPECT_NEAR(flights[0].distanceNm, 60 / kMetersPerNm, 1e-6);
}

TEST_F(FlightHistory, OnlyLastNFiles) {
    const std::string a = "2026-11-20T10:00:00.000Z", b = "2026-11-21T10:00:00.000Z", c = "2026-11-22T10:00:00.000Z";
    file("events-2026-11-20.jsonl", {blockOut(a, 32.73, -117.2), blockIn(a, "2026-11-20T11:00:00.000Z", 32.74, -117.2)});
    file("events-2026-11-21.jsonl", {blockOut(b, 32.73, -117.2), blockIn(b, "2026-11-21T11:00:00.000Z", 32.74, -117.2)});
    file("events-2026-11-22.jsonl", {blockOut(c, 32.73, -117.2), blockIn(c, "2026-11-22T11:00:00.000Z", 32.74, -117.2)});
    const auto flights = loadFlightHistory(dir_, 2);
    ASSERT_EQ(flights.size(), 2u);
    EXPECT_EQ(flights[0].flightId, c);
    EXPECT_EQ(flights[1].flightId, b);
}

TEST_F(FlightHistory, EmptyOrMissingDirHasNone) {
    EXPECT_TRUE(loadFlightHistory(dir_).empty());
    EXPECT_TRUE(loadFlightHistory(dir_ / "missing").empty());
}

// Reads Dave's real log and reports how long it took. Skipped unless BLOCKTIME_REAL_LOG_DIR is set; run it in a
// release build (Task 2, Step 5 of the UI test 2 plan).
TEST(FlightHistoryRealLog, RealLogTiming) {
    // _wdupenv_s, as EventLog::defaultDir does: getenv is deprecated under MSVC (C4996).
    wchar_t* env = nullptr;
    std::size_t len = 0;
    fs::path dir;
    if (_wdupenv_s(&env, &len, L"BLOCKTIME_REAL_LOG_DIR") == 0 && env != nullptr) {
        dir = env;
        std::free(env);
    }
    if (dir.empty()) GTEST_SKIP() << "BLOCKTIME_REAL_LOG_DIR not set";
    const auto start = std::chrono::steady_clock::now();
    const auto flights = loadFlightHistory(dir);
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    std::size_t points = 0;
    for (const auto& f : flights) points += f.track.size();
    std::cout << std::format("[ timing ] {} flights, {} track points, {:.1f} ms\n", flights.size(), points, ms);
    for (const auto& f : flights) {
        std::cout << std::format("[ flight ] {} {} end={} reason={} block_s={} dist={:.1f} nm max_alt={:.0f} pts={}\n",
                                 f.flightId, f.atcId, static_cast<int>(f.end), f.endReason, f.blockTimeS.value_or(-1),
                                 f.distanceNm, f.maxAltFt, f.track.size());
    }
    EXPECT_LT(ms, 200.0);
}
