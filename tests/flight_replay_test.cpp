// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The replayer: a flight's log lines as a timeline with the sim's pauses taken out, and a clock (milestone 11,
// docs/replay.md).

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "event_log.h"
#include "flight_replay.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const std::filesystem::path kStrippedFixtureDir = BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan";

std::filesystem::path tempLogDir(const char* name) {
    auto dir = std::filesystem::temp_directory_path() / "blocktime-flight-replay" / name;
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void touch(const std::filesystem::path& file) { std::ofstream(file) << ""; }

const TimePoint t0 = *parseUtc("2026-11-25T18:20:00.000Z");

LogLine line(milliseconds after, std::string type, nlohmann::json data = nlohmann::json::object()) {
    return {t0 + after, std::move(type), std::move(data)};
}

std::vector<std::int64_t> offsets(const ReplayTimeline& t) {
    std::vector<std::int64_t> out;
    for (const ReplayItem& i : t.items()) out.push_back(i.offsetMs);
    return out;
}

}  // namespace

TEST(FlightReplay, LogFilesIncludeTheDayBefore) {
    const auto dir = tempLogDir("days");
    for (const char* d : {"2026-11-23", "2026-11-24", "2026-11-25", "2026-11-26"}) {
        touch(dir / ("events-" + std::string(d) + ".jsonl"));
    }
    const auto files = replayLogFiles(dir, "2026-11-25T00:10:00.000Z");
    ASSERT_EQ(files.size(), 3u);
    EXPECT_EQ(files[0].filename(), "events-2026-11-24.jsonl");
    EXPECT_EQ(files[2].filename(), "events-2026-11-26.jsonl");
}

TEST(FlightReplay, LogAvailable) {
    const auto dir = tempLogDir("available");
    touch(dir / "events-2026-11-25.jsonl");
    EXPECT_TRUE(replayLogAvailable(dir, "2026-11-25T18:43:04.053Z"));
    EXPECT_FALSE(replayLogAvailable(dir, "2026-11-24T23:53:16.837Z"));
}

TEST(FlightReplay, Routes) {
    EXPECT_EQ(replayRoute("telemetry"), ReplayRoute::Sample);
    EXPECT_EQ(replayRoute("block_out"), ReplayRoute::Block);
    EXPECT_EQ(replayRoute("block_in"), ReplayRoute::Block);
    for (const char* t : {"phase_changed", "gsx_service", "atc_exchange", "atis", "arrival_gate", "sim_running",
                          "sim_stopped", "touchdown"}) {
        EXPECT_EQ(replayRoute(t), ReplayRoute::Flight) << t;
    }
    for (const char* t : {"cockpit", "probe_gsx", "pmdg_data", "paused", "unpaused", "sim_disconnected",
                          "airport_runways", "block_out_refused", "telemetry_invalid", "recorder_started", "sim_connected"}) {
        EXPECT_EQ(replayRoute(t), ReplayRoute::Skip) << t;
    }
}

TEST(FlightReplay, OffsetsFromTheFirstLine) {
    const ReplayTimeline t({line(0ms, "telemetry"), line(1s, "telemetry"), line(61s, "phase_changed")});
    EXPECT_EQ(offsets(t), (std::vector<std::int64_t>{0, 1000, 61000}));
    EXPECT_EQ(t.durationMs(), 61000);
    EXPECT_EQ(t.items()[2].route, ReplayRoute::Flight);
    EXPECT_TRUE(t.items()[0].data.is_null());  // samples keep only the parsed sample
}

// The replay starts at the first line it plays, not at a skipped one (a probe or cockpit line), so it opens on
// something.
TEST(FlightReplay, OffsetsFromTheFirstPlayedLine) {
    const ReplayTimeline t({line(0ms, "probe_facility"), line(900ms, "cockpit"), line(2s, "telemetry"),
                            line(3s, "telemetry")});
    EXPECT_EQ(offsets(t), (std::vector<std::int64_t>{0, 1000}));
    EXPECT_EQ(t.timeAt(0), t0 + 2s);
}

TEST(FlightReplay, PausedStretchIsSkipped) {
    const ReplayTimeline t({line(0ms, "telemetry"), line(10s, "paused", {{"flags", 8}}),
                            line(12s, "telemetry"), line(20min, "unpaused"), line(20min + 5s, "telemetry")});
    EXPECT_EQ(offsets(t), (std::vector<std::int64_t>{0, 10000, 15000}));
    EXPECT_EQ(t.timeAt(15000), t0 + 20min + 5s);
    EXPECT_EQ(t.timeAt(14000), t0 + 20min + 4s);
}

TEST(FlightReplay, NestedPausesAreOneStretch) {
    const ReplayTimeline t({line(0ms, "telemetry"), line(10s, "paused", {{"flags", 8}}),
                            line(12s, "paused", {{"flags", 1}}), line(60s, "unpaused"), line(61s, "telemetry")});
    EXPECT_EQ(offsets(t), (std::vector<std::int64_t>{0, 11000}));
}

// After a reconnect the recorder logs no `unpaused` for a pause from before it (sim_connection.cpp resets its flags), so
// a session boundary ends the stretch; otherwise every later line would get the pause's offset.
TEST(FlightReplay, SessionBoundaryEndsAPause) {
    const ReplayTimeline t({line(0ms, "telemetry"), line(10s, "paused", {{"flags", 1}}),
                            line(5min, "sim_connected"), line(5min + 1s, "telemetry"),
                            line(6min, "paused", {{"flags", 8}}), line(7min, "recorder_started"),
                            line(7min + 2s, "telemetry")});
    EXPECT_EQ(offsets(t), (std::vector<std::int64_t>{0, 11000, 72000}));  // 7:02 less 4:50 and 1:00 paused
}

TEST(FlightReplay, OffsetOfPhaseAndTime) {
    const ReplayTimeline t({line(0ms, "telemetry"), line(10min, "block_out", {{"flight_id", "F"}}),
                            line(15min, "phase_changed", {{"phase", "takeoff"}}),
                            line(16min, "phase_changed", {{"phase", "climb"}}), line(50min, "telemetry"),
                            line(70min, "block_in", {{"flight_id", "F"}})});
    EXPECT_EQ(t.offsetOf("climb"), 16 * 60'000);
    EXPECT_EQ(t.offsetOf("block_out"), 10 * 60'000);
    EXPECT_EQ(t.offsetOf("block_in"), 70 * 60'000);
    EXPECT_EQ(t.offsetOf("0:30"), 40 * 60'000);
    EXPECT_FALSE(t.offsetOf("nonsense"));
    EXPECT_FALSE(t.offsetOf("cruise"));
}

// Dave's KLAS → KSAN leg began 23 minutes after the KSFO → KLAS leg blocked in: the replay starts after that block in,
// not on the previous leg's final approach (its telemetry carries no flight id).
TEST(FlightReplay, StartsAfterThePreviousFlightsBlockIn) {
    const auto dir = tempLogDir("previous");
    std::ofstream(dir / "events-2026-11-24.jsonl")
        << R"({"ts":"2026-11-24T23:24:14.000Z","type":"telemetry","data":{"on_ground":true}})" << "\n"
        << R"({"ts":"2026-11-24T23:30:25.944Z","type":"block_in","data":{"flight_id":"A"}})" << "\n"
        << R"({"ts":"2026-11-24T23:31:00.000Z","type":"telemetry","data":{"on_ground":true}})" << "\n"
        << R"({"ts":"2026-11-24T23:53:27.961Z","type":"block_out","data":{"flight_id":"2026-11-24T23:53:16.837Z","time":"2026-11-24T23:53:16.837Z"}})" << "\n"
        << R"({"ts":"2026-11-25T00:52:00.000Z","type":"block_in","data":{"flight_id":"2026-11-24T23:53:16.837Z","time":"2026-11-25T00:52:00.000Z"}})" << "\n";
    const ReplayTimeline t = loadReplay(dir, "2026-11-24T23:53:16.837Z");
    ASSERT_EQ(t.items().size(), 3u);
    EXPECT_EQ(t.items().front().ts, *parseUtc("2026-11-24T23:31:00.000Z"));
}

// Deboarding before block out is the previous flight's (D-137); on Live it showed on that flight's Arrival.
TEST(FlightReplay, DeboardingBeforeBlockOutIsSkipped) {
    const ReplayTimeline t({line(0ms, "gsx_service", {{"service", "deboarding"}, {"state", "performing"}}),
                            line(1min, "gsx_service", {{"service", "boarding"}, {"state", "performing"}}),
                            line(10min, "block_out", {{"flight_id", "F"}}),
                            line(70min, "gsx_service", {{"service", "deboarding"}, {"state", "performing"}})});
    ASSERT_EQ(t.items().size(), 3u);
    EXPECT_EQ(t.items()[0].data.value("service", ""), "boarding");
    EXPECT_EQ(t.items()[2].data.value("service", ""), "deboarding");
}

TEST(FlightReplay, StrippedFixtureBuildsATimeline) {
    const ReplayTimeline t = loadReplay(kStrippedFixtureDir, "2026-11-24T23:53:16.837Z");
    ASSERT_FALSE(t.empty());
    const auto blockOut = t.offsetOf("block_out");
    ASSERT_TRUE(blockOut);
    EXPECT_GT(*blockOut, 0);
    EXPECT_EQ(t.items().back().type, "block_in");
    EXPECT_EQ(t.items().back().route, ReplayRoute::Block);
}

TEST(FlightReplay, MissingLogIsEmpty) {
    const auto dir = tempLogDir("missing");
    EXPECT_TRUE(loadReplay(dir, "2026-11-25T18:43:04.053Z").empty());
}

TEST(ReplayClock, AdvancesBySpeed) {
    ReplayClock c;
    c.setSpeed(10);
    EXPECT_EQ(c.advance(100ms), 1000);
}

TEST(ReplayClock, PausedDoesNotAdvance) {
    ReplayClock c;
    c.setPaused(true);
    EXPECT_EQ(c.advance(1s), 0);
    c.setPaused(false);
    EXPECT_EQ(c.advance(1s), 1000);
}

TEST(ReplayClock, SpeedChangeKeepsOffset) {
    ReplayClock c;
    EXPECT_EQ(c.advance(1s), 1000);
    c.setSpeed(10);
    EXPECT_EQ(c.advance(1s), 11000);
    c.seek(500);
    EXPECT_EQ(c.offset(), 500);
}
