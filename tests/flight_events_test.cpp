// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One flight's lines read back from the event log, for the debrief (milestone 10).

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

#include "event_log.h"
#include "flight_events.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const std::filesystem::path kFixtureDir = BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan";
constexpr const char* kFlight = "2026-11-24T23:53:16.837Z";

std::filesystem::path tempLogDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / "blocktime-flight-events" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void writeLines(const std::filesystem::path& file, std::initializer_list<const char*> lines) {
    std::ofstream out(file);
    for (const char* l : lines) out << l << "\n";
}

}  // namespace

TEST(EventLog, ParseUtcRoundTrips) {
    const auto t = parseUtc("2026-11-24T23:53:16.837Z");
    ASSERT_TRUE(t);
    EXPECT_EQ(formatUtc(*t), "2026-11-24T23:53:16.837Z");
    const auto whole = parseUtc("2026-11-24T23:53:16Z");
    ASSERT_TRUE(whole);
    EXPECT_EQ(*t - *whole, 837ms);
    EXPECT_FALSE(parseUtc("yesterday"));
}

TEST(FlightEvents, LoadsTheFixtureFlight) {
    const auto lines = loadFlightEvents(kFixtureDir, kFlight);
    ASSERT_GT(lines.size(), 3000u);
    const auto blockOut = std::find_if(lines.begin(), lines.end(), [](const LogLine& l) { return l.type == "block_out"; });
    ASSERT_NE(blockOut, lines.end());
    EXPECT_LE(blockOut->ts - lines.front().ts, minutes(30));
    EXPECT_GE(blockOut->ts - lines.front().ts, minutes(29));
    EXPECT_EQ(lines.back().type, "block_in");
    EXPECT_EQ(lines.back().data["flight_id"], kFlight);
}

TEST(FlightEvents, DropsOtherFlightsLines) {
    const auto dir = tempLogDir("others");
    writeLines(dir / "events-2026-11-24.jsonl",
               {R"({"ts":"2026-11-24T10:00:00.000Z","type":"block_in","data":{"flight_id":"A"}})",
                R"({"ts":"2026-11-24T10:05:00.000Z","type":"telemetry","data":{"on_ground":true}})",
                R"({"ts":"2026-11-24T10:10:00.000Z","type":"block_out","data":{"flight_id":"B"}})",
                R"({"ts":"2026-11-24T10:11:00.000Z","type":"gsx_service","data":{"flight_id":"A"}})",
                R"(not json)",
                R"({"ts":"2026-11-24T11:00:00.000Z","type":"block_in","data":{"flight_id":"B"}})"});
    const auto lines = loadFlightEvents(dir, "B");
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0].type, "telemetry");
    EXPECT_EQ(lines[1].type, "block_out");
    EXPECT_EQ(lines[2].type, "block_in");
}

TEST(FlightEvents, ReadsAcrossDailyFiles) {
    const auto dir = tempLogDir("days");
    writeLines(dir / "events-2026-11-24.jsonl",
               {R"({"ts":"2026-11-24T23:50:00.000Z","type":"block_out","data":{"flight_id":"C"}})"});
    writeLines(dir / "events-2026-11-25.jsonl",
               {R"({"ts":"2026-11-25T00:50:00.000Z","type":"block_in","data":{"flight_id":"C"}})"});
    EXPECT_EQ(loadFlightEvents(dir, "C").size(), 2u);
}

TEST(FlightEvents, OpenFlightIsEmpty) {
    const auto dir = tempLogDir("open");
    writeLines(dir / "events-2026-11-24.jsonl",
               {R"({"ts":"2026-11-24T10:10:00.000Z","type":"block_out","data":{"flight_id":"D"}})",
                R"({"ts":"2026-11-24T10:20:00.000Z","type":"flight_interrupted","data":{"flight_id":"D"}})"});
    EXPECT_TRUE(loadFlightEvents(dir, "D").empty());
    EXPECT_TRUE(loadFlightEvents(dir, "nope").empty());
}

TEST(FlightEvents, BlockInInstantOnlyWhenAsked) {
    const auto dir = tempLogDir("instant");
    const auto file = dir / "events-2026-11-24.jsonl";
    writeLines(file,
               {R"({"ts":"2026-11-24T10:10:00.000Z","type":"block_out","data":{"flight_id":"B"}})",
                R"({"ts":"2026-11-24T11:00:00.000Z","type":"block_in","data":{"flight_id":"B"}})",
                R"({"ts":"2026-11-24T11:00:00.000Z","type":"phase_changed","data":{"flight_id":"B","phase":"arrived"}})",
                R"({"ts":"2026-11-24T11:00:01.000Z","type":"telemetry","data":{"on_ground":true}})"});
    EXPECT_EQ(loadFlightEvents(dir, "B").size(), 2u);  // the debrief and settling: up to block in
    const auto replay = loadFlightEvents({file}, "B", true, true);
    ASSERT_EQ(replay.size(), 3u);
    EXPECT_EQ(replay[2].type, "phase_changed");
}
