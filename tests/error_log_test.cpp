// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "error_log.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

class ErrorLogTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    static std::string read(const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

    fs::path dir_;
};

}  // namespace

TEST_F(ErrorLogTest, StreamGoesToTheFirstCandidate) {
    std::ostringstream stream;
    {
        ErrorLog log({dir_ / "errors.log", dir_ / "fallback.log"}, stream, "--- started");
        EXPECT_EQ(log.path(), dir_ / "errors.log");
        stream << "LOST: block_in {}\n";
    }
    EXPECT_EQ(read(dir_ / "errors.log"), "--- started\nLOST: block_in {}\n");
    EXPECT_FALSE(fs::exists(dir_ / "fallback.log"));
    EXPECT_EQ(stream.str(), "");  // nothing went to the stream's own buffer
}

TEST_F(ErrorLogTest, FallsBackWhenTheFirstFolderIsMissing) {
    std::ostringstream stream;
    ErrorLog log({dir_ / "missing" / "errors.log", dir_ / "fallback.log"}, stream, "--- started");
    EXPECT_EQ(log.path(), dir_ / "fallback.log");
}

TEST_F(ErrorLogTest, AppendsAcrossRuns) {
    std::ostringstream stream;
    { ErrorLog log({dir_ / "errors.log"}, stream, "--- run 1"); }
    { ErrorLog log({dir_ / "errors.log"}, stream, "--- run 2"); }
    EXPECT_EQ(read(dir_ / "errors.log"), "--- run 1\n--- run 2\n");
}

TEST_F(ErrorLogTest, WritesReachTheFileWithoutClosingIt) {
    // A crash must not lose what was already written: each write is flushed.
    std::ostringstream stream;
    ErrorLog log({dir_ / "errors.log"}, stream, "--- started");
    stream << "SimConnect call failed";
    EXPECT_EQ(read(dir_ / "errors.log"), "--- started\nSimConnect call failed");
}

TEST_F(ErrorLogTest, StreamIsRestoredAfterwards) {
    std::ostringstream stream;
    { ErrorLog log({dir_ / "errors.log"}, stream, "--- started"); }
    stream << "after";
    EXPECT_EQ(stream.str(), "after");
}

TEST_F(ErrorLogTest, NoUsableCandidateLeavesTheStreamAlone) {
    std::ostringstream stream;
    {
        ErrorLog log({dir_ / "missing" / "errors.log"}, stream, "--- started");
        EXPECT_TRUE(log.path().empty());
        stream << "still here";
    }
    EXPECT_EQ(stream.str(), "still here");
}
