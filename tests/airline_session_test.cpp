// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "airline_db.h"
#include "airline_session.h"
#include "sqlite_db.h"

using namespace blocktime;
using namespace std::chrono;
namespace fs = std::filesystem;

namespace {

constexpr Date d(int y, unsigned m, unsigned day) { return Date{year{y}, month{m}, std::chrono::day{day}}; }

bool hasNoticeStartingWith(const AirlineSession& session, const std::string& prefix) {
    const auto& n = session.notices();
    return std::any_of(n.begin(), n.end(), [&](const std::string& s) { return s.rfind(prefix, 0) == 0; });
}

class AirlineSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
        paths_ = {dir_ / "blocktime.db", BLOCKTIME_SOURCE_DIR "/data/aircraft", dir_ / "airline.yaml"};
    }
    void TearDown() override { fs::remove_all(dir_); }

    void copyConfig() {
        fs::copy_file(BLOCKTIME_SOURCE_DIR "/config/airline.example.yaml", paths_.airlineConfig);
    }

    void importKbfi() {
        AirlineDbOpen opened = openAirlineDatabase(paths_.database);
        ASSERT_TRUE(opened.db.has_value()) << opened.error;
        ASSERT_TRUE(opened.db->exec(
            "INSERT INTO airport (icao_code, name, latitude, longitude, elevation_ft, country_code, time_zone,"
            " longest_runway_ft) VALUES ('KBFI', 'Boeing Field', 47.53, -122.30, 21, 'US', 'America/Los_Angeles', 10007)"));
    }

    fs::path dir_;
    AirlinePaths paths_;
    std::ostringstream warnings_;
};

}  // namespace

TEST_F(AirlineSessionTest, FreshStartLeasesTheExample) {
    copyConfig();
    importKbfi();
    AirlineSession session = AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    EXPECT_TRUE(session.notices().empty()) << session.notices().front();
    EXPECT_EQ(session.airlineName(), "BlockTime Air");
    ASSERT_EQ(session.airplanes().size(), 1u);
    EXPECT_EQ(session.airplanes()[0].registration, "N738BT");
    EXPECT_EQ(session.cash(), 485'000'000);
    EXPECT_EQ(session.ledger().size(), 2u);
    EXPECT_EQ(warnings_.str(), "");
}

TEST_F(AirlineSessionTest, MissingConfigIsANotice) {
    importKbfi();
    AirlineSession session = AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    EXPECT_TRUE(hasNoticeStartingWith(session, "No airline.yaml in ")) << ::testing::PrintToString(session.notices());
    EXPECT_EQ(session.airlineName(), "");
    EXPECT_TRUE(session.airplanes().empty());
    EXPECT_EQ(session.cash(), 0);
}

TEST_F(AirlineSessionTest, BadConfigStillShowsTheDatabase) {
    copyConfig();
    importKbfi();
    AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    std::ofstream(paths_.airlineConfig) << "airline:\n  name: X\n icao: BTA\n";

    AirlineSession again = AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    EXPECT_TRUE(hasNoticeStartingWith(again, "airline.yaml line ")) << ::testing::PrintToString(again.notices());
    EXPECT_EQ(again.airplanes().size(), 1u);
    EXPECT_EQ(again.ledger().size(), 2u);
    EXPECT_EQ(again.cash(), 485'000'000);
}

TEST_F(AirlineSessionTest, TickChargesAMonthThatFallsDueWhileOpen) {
    copyConfig();
    importKbfi();
    AirlineSession session = AirlineSession::start(paths_, d(2026, 12, 21), warnings_);
    EXPECT_EQ(session.ledger().size(), 2u);
    session.tick(d(2026, 12, 22));
    EXPECT_EQ(session.ledger().size(), 3u);
    EXPECT_EQ(session.cash(), 470'000'000);
    session.tick(d(2026, 12, 22));
    EXPECT_EQ(session.ledger().size(), 3u);
}

TEST_F(AirlineSessionTest, NoAirportsIsANotice) {
    copyConfig();
    AirlineSession session = AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    EXPECT_TRUE(hasNoticeStartingWith(session, "No airport data yet. Run blocktime-import (README)."));
    EXPECT_TRUE(session.airplanes().empty());
}

TEST_F(AirlineSessionTest, TestDatabaseSetAsideIsANotice) {
    copyConfig();
    {
        std::string error;
        auto old = db::Database::open(paths_.database, &error);
        ASSERT_TRUE(old.has_value()) << error;
        ASSERT_TRUE(old->exec("CREATE TABLE airframes (registration TEXT); PRAGMA user_version = 1;"));
    }
    AirlineSession session = AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    EXPECT_TRUE(hasNoticeStartingWith(session, "The Qt test's database was set aside as blocktime.test-schema.db."));
}

TEST_F(AirlineSessionTest, DriftWarningsGoToTheWarningStream) {
    copyConfig();
    importKbfi();
    AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    std::ifstream in(paths_.airlineConfig);
    std::stringstream text;
    text << in.rdbuf();
    in.close();
    std::string yaml = text.str();
    yaml.replace(yaml.find("replace-me"), 10, "changed-id");
    std::ofstream(paths_.airlineConfig) << yaml;

    AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    EXPECT_NE(warnings_.str().find("N738BT: airline.yaml changed simbrief_airframe_id"), std::string::npos) << warnings_.str();
}

TEST_F(AirlineSessionTest, TickReportsWhetherAnythingChanged) {
    copyConfig();
    importKbfi();
    AirlineSession session = AirlineSession::start(paths_, d(2026, 12, 21), warnings_);
    EXPECT_FALSE(session.tick(d(2026, 12, 21)));
    EXPECT_TRUE(session.tick(d(2026, 12, 22)));
    EXPECT_FALSE(session.tick(d(2026, 12, 22)));
}

TEST_F(AirlineSessionTest, GateContextFromCatalogAndConfig) {
    copyConfig();
    importKbfi();
    AirlineSession session = AirlineSession::start(paths_, d(2026, 11, 22), warnings_);
    const auto vip = session.gateContext("737-800 BBJ2", FlightKind::Vip, "KPDX");
    ASSERT_TRUE(vip.has_value());
    EXPECT_DOUBLE_EQ(vip->wingspanM, 35.8);
    EXPECT_EQ(vip->use, SpotUse::Gate);
    EXPECT_EQ(vip->preferred, (std::vector<std::string>{"D14"}));  // airline.example.yaml
    const auto charter = session.gateContext("737-800 BBJ2", FlightKind::Charter, "KSAN");
    ASSERT_TRUE(charter.has_value());
    EXPECT_EQ(charter->use, SpotUse::Ramp);
    EXPECT_TRUE(charter->preferred.empty());
    EXPECT_FALSE(session.gateContext("A320", FlightKind::Passenger, "KPDX").has_value());
}
