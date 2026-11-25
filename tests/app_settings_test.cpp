// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "app_settings.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

class AppSettings_ : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    fs::path write(const std::string& name, const std::string& content) {
        const fs::path p = dir_ / name;
        std::ofstream(p, std::ios::binary) << content;
        return p;
    }

    fs::path dir_;
};

}  // namespace

TEST_F(AppSettings_, MissingFileGivesEmptyKey) {
    EXPECT_EQ(loadAppSettings(dir_ / "settings.json").cartoKey, "");
}

TEST_F(AppSettings_, ReadsCartoKey) {
    EXPECT_EQ(loadAppSettings(write("settings.json", R"({"carto_key":"abc"})")).cartoKey, "abc");
}

TEST_F(AppSettings_, ReadsCartoKeyAfterUtf8Bom) {
    // Notepad and PowerShell 5.1's Set-Content -Encoding utf8 write a byte order mark.
    EXPECT_EQ(loadAppSettings(write("settings.json", "\xEF\xBB\xBF{\"carto_key\":\"abc\"}")).cartoKey, "abc");
}

TEST_F(AppSettings_, BadJsonGivesEmptyKey) {
    EXPECT_EQ(loadAppSettings(write("settings.json", R"({"carto_key":)")).cartoKey, "");
}

TEST_F(AppSettings_, NonStringKeyGivesEmptyKey) {
    EXPECT_EQ(loadAppSettings(write("settings.json", R"({"carto_key":42})")).cartoKey, "");
    EXPECT_EQ(loadAppSettings(write("other.json", R"(["carto_key"])")).cartoKey, "");
}

TEST_F(AppSettings_, PruneDeletesOnlyOldFiles) {
    const fs::path oldFile = write("old.png", "x");
    const fs::path newFile = write("new.png", "x");
    fs::create_directories(dir_ / "sub");
    const fs::path oldNested = dir_ / "sub" / "old2.png";
    std::ofstream(oldNested, std::ios::binary) << "x";
    const auto monthAgo = fs::file_time_type::clock::now() - std::chrono::hours(24 * 31);
    fs::last_write_time(oldFile, monthAgo);
    fs::last_write_time(oldNested, monthAgo);

    EXPECT_EQ(pruneOldFiles(dir_, kTileMaxAge), 2u);
    EXPECT_FALSE(fs::exists(oldFile));
    EXPECT_FALSE(fs::exists(oldNested));
    EXPECT_TRUE(fs::exists(newFile));
}

TEST_F(AppSettings_, PruneMissingDirIsZero) {
    EXPECT_EQ(pruneOldFiles(dir_ / "missing", kTileMaxAge), 0u);
}

TEST(Redact, ReplacesEveryCopyOfTheSecret) {
    EXPECT_EQ(redact("GET https://x/1/2/3.png?key=abc_123 failed; retry ?key=abc_123", "abc_123"),
              "GET https://x/1/2/3.png?key=<key> failed; retry ?key=<key>");
}

TEST(Redact, EmptySecretChangesNothing) {
    EXPECT_EQ(redact("nothing to hide", ""), "nothing to hide");
}

TEST_F(AppSettings_, DefaultPathsAreUnderLocalAppDataBlockTime) {
    EXPECT_EQ(defaultSettingsFile().filename(), "settings.json");
    EXPECT_EQ(defaultSettingsFile().parent_path().filename(), "BlockTime");
    EXPECT_EQ(defaultTileCacheDir().filename(), "tiles");
    EXPECT_EQ(defaultTileCacheDir().parent_path(), defaultSettingsFile().parent_path());
}

TEST_F(AppSettings_, ReadsPlanningKeys) {
    const AppSettings s = loadAppSettings(write(
        "settings.json",
        R"({"carto_key":"c","simbrief_user_id":"730097","sayintentions_api_key":"p","sayintentions_developer_key":"d"})"));
    EXPECT_EQ(s.simbriefUserId, "730097");
    EXPECT_EQ(s.sayIntentionsApiKey, "p");
    EXPECT_EQ(s.sayIntentionsDeveloperKey, "d");
    EXPECT_EQ(s.secrets(), (std::vector<std::string>{"c", "p", "d"}));
    EXPECT_TRUE(loadAppSettings(write("empty.json", "{}")).secrets().empty());
}

TEST(Redact, RedactsEverySecret) {
    EXPECT_EQ(redact("GET ?api_key=pilot&key=carto X-Developer-Key: dev", {"pilot", "carto", "dev"}),
              "GET ?api_key=<key>&key=<key> X-Developer-Key: <key>");
    EXPECT_EQ(redact("nothing here", std::vector<std::string>{}), "nothing here");
}

TEST_F(AppSettings_, ReadsClaudeKeyAsSecret) {
    const auto s = loadAppSettings(write("settings.json", R"({"carto_key":"c","claude_api_key":"sk-ant-test"})"));
    EXPECT_EQ(s.claudeApiKey, "sk-ant-test");
    const auto secrets = s.secrets();
    EXPECT_NE(std::find(secrets.begin(), secrets.end(), "sk-ant-test"), secrets.end());
}
