// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "airline_db.h"
#include "data_dir.h"
#include "sqlite_db.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

class DataDirSeed : public ::testing::Test {
protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / ("blocktime-test-DataDirSeed-" +
                                             std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(root_);
        fs::create_directories(from() / "events");
        std::ofstream(from() / "airline.yaml") << "airline: {}\n";
        std::ofstream(from() / "settings.json") << "{}\n";
        std::ofstream(from() / "errors.log") << "old errors\n";
        std::ofstream(from() / "events" / "events-2026-11-27.jsonl") << "{}\n";
        std::string error;
        auto db = db::Database::open(from() / "blocktime.db", &error);
        ASSERT_TRUE(db.has_value()) << error;
        ASSERT_TRUE(migrateAirlineDatabase(*db, &error)) << error;
        ASSERT_TRUE(db->exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                             " VALUES ('opening_cash', '2026-11-22T00:00:00Z', 500000000)"));
        source_ = std::move(db);  // kept open, as a recording copy would have it
    }
    void TearDown() override {
        source_.reset();
        fs::remove_all(root_);
    }
    fs::path from() const { return root_ / "real"; }
    fs::path to() const { return root_ / "copy"; }

    fs::path root_;
    std::optional<db::Database> source_;
};

}  // namespace

TEST_F(DataDirSeed, CopiesTheDatabaseAndConfig) {
    std::string error;
    ASSERT_TRUE(seedDataDir(from(), to(), false, &error)) << error;
    EXPECT_TRUE(fs::exists(to() / "airline.yaml"));
    EXPECT_TRUE(fs::exists(to() / "settings.json"));
    EXPECT_FALSE(fs::exists(to() / "errors.log"));
    EXPECT_FALSE(fs::exists(to() / "events"));

    auto copy = db::Database::open(to() / "blocktime.db", &error);
    ASSERT_TRUE(copy.has_value()) << error;
    EXPECT_EQ(copy->userVersion(), kAirlineSchemaVersion);
    db::Statement q(*copy, "SELECT SUM(amount_cents) FROM ledger_line");
    ASSERT_TRUE(q.next());
    EXPECT_EQ(q.integer(0), 500000000);

    // Writing to the copy leaves the real airline alone.
    ASSERT_TRUE(copy->exec("INSERT INTO ledger_line (category, posted_at, amount_cents)"
                           " VALUES ('lease', '2026-11-23T00:00:00Z', -5000)"));
    db::Statement real(*source_, "SELECT COUNT(*) FROM ledger_line");
    ASSERT_TRUE(real.next());
    EXPECT_EQ(real.integer(0), 1);
}

TEST_F(DataDirSeed, CopiesTheEventLogsWhenAsked) {
    std::string error;
    ASSERT_TRUE(seedDataDir(from(), to(), true, &error)) << error;
    EXPECT_TRUE(fs::exists(to() / "events" / "events-2026-11-27.jsonl"));
}

TEST_F(DataDirSeed, AMissingSourceGivesAnEmptyFolder) {
    std::string error;
    ASSERT_TRUE(seedDataDir(root_ / "nothing-here", to(), true, &error)) << error;
    EXPECT_TRUE(fs::is_directory(to()));
    EXPECT_TRUE(fs::is_empty(to()));
}

TEST(DataDir, DefaultPathsFollowTheChosenFolder) {
    const fs::path before = dataDir();
    EXPECT_EQ(before, realDataDir());
    setDataDir("C:/somewhere/BlockTime-dev");
    EXPECT_EQ(dataDir(), fs::path("C:/somewhere/BlockTime-dev"));
    EXPECT_EQ(defaultAirlineDatabaseFile(), fs::path("C:/somewhere/BlockTime-dev") / "blocktime.db");
    setDataDir(before);
}
