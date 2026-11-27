// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "block_detector.h"
#include "event_log.h"
#include "flight_events.h"
#include "flight_settlement.h"
#include "open_flight.h"

using namespace std::chrono;
using namespace blocktime;

TEST(FormatUtc, MillisecondsAndZ) {
    const auto t = sys_days{2026y / 11 / 22} + 14h + 3min + 7s + 123ms;
    EXPECT_EQ(formatUtc(t), "2026-11-22T14:03:07.123Z");
}

TEST(FormatUtc, WholeSecondHasZeroMillis) {
    const auto t = sys_days{2026y / 11 / 22} + 14h;
    EXPECT_EQ(formatUtc(t), "2026-11-22T14:00:00.000Z");
}

TEST(FormatUtc, SubMillisecondIsTruncated) {
    const auto t = sys_days{2026y / 11 / 22} + 14h + 999us;
    EXPECT_EQ(formatUtc(t), "2026-11-22T14:00:00.000Z");
}

// --- The writer and its readers together (review 1.10): every reader of a flight depends on the writer's text. ---

namespace {

class EventLogFiles : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = std::filesystem::temp_directory_path() /
               ("blocktime-test-EventLogFiles-" +
                std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        std::filesystem::remove_all(dir_);
    }
    void TearDown() override { std::filesystem::remove_all(dir_); }

    std::filesystem::path todayFile() const { return dir_ / ("events-" + utcTimestamp().substr(0, 10) + ".jsonl"); }

    std::vector<std::string> fileLines() const {
        std::ifstream in(todayFile(), std::ios::binary);
        std::vector<std::string> out;
        for (std::string l; std::getline(in, l);) out.push_back(l);
        return out;
    }

    std::filesystem::path dir_;
};

}  // namespace

TEST_F(EventLogFiles, WrittenFlightReadsBack) {
    EventLog log(dir_);
    const auto now = floor<milliseconds>(system_clock::now());
    BlockOut out;
    out.flightId = formatUtc(now);
    out.time = now;
    out.latDeg = 47.53;
    out.lonDeg = -122.302;
    out.fuelLb = 20000.4;
    out.plannedFlightId = 7;
    out.aircraftFolder = "pmdg 737-800bbj2";
    out.parking = "Gate A1";
    ASSERT_TRUE(log.write("block_out", toJson(BlockEvent{out})));
    EXPECT_EQ(findOpenFlight(dir_), out.flightId);

    ASSERT_TRUE(log.write("phase_changed",
                          {{"flight_id", out.flightId}, {"phase", "taxi_out"}, {"time", formatUtc(now + 1s)}}));
    BlockIn in;
    in.flightId = out.flightId;
    in.time = now + 2h;
    in.shutdownTime = now + 2h;
    in.latDeg = 32.7336;
    in.lonDeg = -117.19;
    in.fuelLb = 9400.6;
    in.shutdownFuelLb = 9400.6;
    in.blockTimeS = 7200;
    in.parking = "Gate 5";
    ASSERT_TRUE(log.write("block_in", toJson(BlockEvent{in})));
    EXPECT_FALSE(findOpenFlight(dir_).has_value());

    const auto f = findFlownFlight(dir_, 7, "2000-01-01T00:00:00Z", {});
    ASSERT_TRUE(f.has_value());
    EXPECT_EQ(f->logFlightId, out.flightId);
    EXPECT_EQ(f->blockOutAt, out.flightId.substr(0, 19) + "Z");
    EXPECT_EQ(f->fuelOutLb, 20000);
    EXPECT_EQ(f->fuelInLb, 9401);
    EXPECT_EQ(f->blockS, 7200);
    EXPECT_DOUBLE_EQ(f->inLat, 32.7336);
    EXPECT_EQ(f->originParking, "Gate A1");
    EXPECT_EQ(f->arrivalParking, "Gate 5");
    ASSERT_GE(f->phases.size(), 1u);
    EXPECT_EQ(f->phases.front().phase, "taxi_out");

    EXPECT_EQ(loadFlightEvents(dir_, out.flightId).size(), 3u);

    // loadFlightEvents' fast path finds the block_out by text and skips lines by a leading ts (D-230). If the writer's
    // form changed, the reader would still be right but slow again; this pins the form.
    bool blockOutSeen = false;
    for (const auto& file : dailyLogFiles(dir_)) {
        std::ifstream lines(file, std::ios::binary);
        std::string text;
        while (std::getline(lines, text)) {
            EXPECT_TRUE(text.starts_with(R"({"ts":")")) << text;
            if (text.find(R"("type":"block_out")") != std::string::npos &&
                text.find(R"("flight_id":")" + out.flightId + '"') != std::string::npos) {
                blockOutSeen = true;
            }
        }
    }
    EXPECT_TRUE(blockOutSeen);
}

TEST_F(EventLogFiles, TornLastLineIsRepaired) {
    std::filesystem::create_directories(dir_);
    std::ofstream(todayFile(), std::ios::binary) << R"({"ts":"2026-11-27T10:00:00.000Z","type":"block_o)";
    EventLog log(dir_);
    ASSERT_TRUE(log.write("recorder_started", {{"version", "test"}}));
    const auto lines = fileLines();
    ASSERT_EQ(lines.size(), 2u);
    const auto j = nlohmann::json::parse(lines[1], nullptr, false);
    ASSERT_FALSE(j.is_discarded());
    EXPECT_EQ(j["type"], "recorder_started");
}

TEST_F(EventLogFiles, KeysAreInTheOrderTheReadersExpect) {
    EventLog log(dir_);
    ASSERT_TRUE(log.write("block_in", {{"flight_id", "x"}}));
    const auto lines = fileLines();
    ASSERT_EQ(lines.size(), 1u);
    // The readers' pre-filters match this text before parsing (open_flight, flight_settlement, flight_history).
    EXPECT_EQ(lines[0].substr(0, 7), R"({"ts":")");
    EXPECT_NE(lines[0].find(R"(","type":"block_in","data":{)"), std::string::npos) << lines[0];
}

TEST_F(EventLogFiles, BadUtf8IsReplacedNotThrown) {
    EventLog log(dir_);
    ASSERT_TRUE(log.write("telemetry", {{"title", std::string("PMDG \xff 737")}}));
    const auto lines = fileLines();
    ASSERT_EQ(lines.size(), 1u);
    const auto j = nlohmann::json::parse(lines[0], nullptr, false);
    ASSERT_FALSE(j.is_discarded());
    EXPECT_NE(j["data"]["title"].get<std::string>().find("\xEF\xBF\xBD"), std::string::npos);
}
