// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <string>

#include <gtest/gtest.h>

#include "open_flight.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

// Log lines in the real format (D-020), as EventLog writes them.
std::string blockOut(const std::string& id) {
    return std::format(R"({{"ts":"2026-11-22T16:43:04.207Z","type":"block_out","data":{{"flight_id":"{}","fuel_lb":15761.7}}}})", id);
}
std::string blockIn(const std::string& id) {
    return std::format(R"({{"ts":"2026-11-22T17:06:15.671Z","type":"block_in","data":{{"block_time_s":1400,"flight_id":"{}"}}}})", id);
}
std::string interrupted(const std::string& id) {
    return std::format(R"({{"ts":"2026-11-22T17:29:22.596Z","type":"flight_interrupted","data":{{"flight_id":"{}","reason":"sim_quit"}}}})", id);
}
std::string telemetry() {
    return R"({"ts":"2026-11-22T16:50:00.000Z","type":"telemetry","data":{"on_ground":false,"fuel_lb":15400.0}})";
}

class OpenFlight : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    // Writes one file; each line ends with '\n' unless `tornTail` is given, which is appended without one.
    void file(const std::string& name, std::initializer_list<std::string> lines, const std::string& tornTail = "") {
        std::ofstream out(dir_ / name, std::ios::binary);
        for (const auto& l : lines) out << l << '\n';
        out << tornTail;
    }

    fs::path dir_;
};

}  // namespace

TEST_F(OpenFlight, EmptyOrMissingDirHasNone) {
    EXPECT_EQ(findOpenFlight(dir_), std::nullopt);
    EXPECT_EQ(findOpenFlight(dir_ / "missing"), std::nullopt);
}

TEST_F(OpenFlight, BlockOutAloneIsOpen) {
    file("events-2026-11-22.jsonl", {telemetry(), blockOut("A"), telemetry()});
    EXPECT_EQ(findOpenFlight(dir_), "A");
}

TEST_F(OpenFlight, BlockInClosesIt) {
    file("events-2026-11-22.jsonl", {blockOut("A"), telemetry(), blockIn("A")});
    EXPECT_EQ(findOpenFlight(dir_), std::nullopt);
}

TEST_F(OpenFlight, InterruptedClosesIt) {
    file("events-2026-11-22.jsonl", {blockOut("A"), interrupted("A"), telemetry()});
    EXPECT_EQ(findOpenFlight(dir_), std::nullopt);
}

TEST_F(OpenFlight, SecondFlightOpenAfterFirstClosed) {
    file("events-2026-11-22.jsonl", {blockOut("A"), blockIn("A"), telemetry(), blockOut("B"), telemetry()});
    EXPECT_EQ(findOpenFlight(dir_), "B");
}

TEST_F(OpenFlight, FoundInYesterdaysFile) {
    file("events-2026-11-21.jsonl", {blockOut("A"), telemetry()});
    file("events-2026-11-22.jsonl", {telemetry(), telemetry()});
    EXPECT_EQ(findOpenFlight(dir_), "A");
}

TEST_F(OpenFlight, NewestFileWins) {
    file("events-2026-11-21.jsonl", {blockOut("A")});
    file("events-2026-11-22.jsonl", {interrupted("A")});
    EXPECT_EQ(findOpenFlight(dir_), std::nullopt);
}

TEST_F(OpenFlight, TornLastLineIsSkipped) {
    file("events-2026-11-22.jsonl", {blockOut("A")}, R"({"ts":"2026-11-22T17:0)");
    EXPECT_EQ(findOpenFlight(dir_), "A");
}

TEST_F(OpenFlight, TornBlockLineIsSkipped) {
    file("events-2026-11-22.jsonl", {blockOut("A")}, R"({"ts":"2026-11-22T17:06:15.671Z","type":"block_in","da)");
    EXPECT_EQ(findOpenFlight(dir_), "A");
}

TEST_F(OpenFlight, OtherFilesIgnored) {
    file("notes.txt", {blockOut("A")});
    file("events-2026-11-22.jsonl.bak", {blockOut("B")});
    EXPECT_EQ(findOpenFlight(dir_), std::nullopt);
}

TEST_F(OpenFlight, LookbackIsLimited) {
    file("events-2026-11-01.jsonl", {blockOut("A")});
    for (int day = 2; day <= 8; ++day) file(std::format("events-2026-11-{:02}.jsonl", day), {telemetry()});
    EXPECT_EQ(findOpenFlight(dir_), std::nullopt);
    EXPECT_EQ(findOpenFlight(dir_, 8), "A");
}
