// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "window_placement.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

class WindowPlacement_ : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    fs::path write(const std::string& content) {
        const fs::path p = dir_ / "window.json";
        std::ofstream(p, std::ios::binary) << content;
        return p;
    }

    fs::path dir_;
};

}  // namespace

TEST_F(WindowPlacement_, SavedPlacementReadsBack) {
    // A window maximized on a monitor left of the main one: negative coordinates are normal there.
    const WindowPlacement saved{.maximized = true, .left = -1700, .top = 120, .right = -400, .bottom = 880};
    const fs::path file = dir_ / "window.json";
    ASSERT_TRUE(saveWindowPlacement(file, saved));

    const auto read = loadWindowPlacement(file);
    ASSERT_TRUE(read.has_value());
    EXPECT_TRUE(read->maximized);
    EXPECT_EQ(read->left, -1700);
    EXPECT_EQ(read->top, 120);
    EXPECT_EQ(read->right, -400);
    EXPECT_EQ(read->bottom, 880);
}

TEST_F(WindowPlacement_, MissingFileGivesNothing) {
    EXPECT_FALSE(loadWindowPlacement(dir_ / "window.json").has_value());
}

TEST_F(WindowPlacement_, BadFileGivesNothing) {
    EXPECT_FALSE(loadWindowPlacement(write("not json")).has_value());
    EXPECT_FALSE(loadWindowPlacement(write(R"({"maximized": true})")).has_value());
    EXPECT_FALSE(loadWindowPlacement(write(R"({"maximized": "yes", "left": 0, "top": 0, "right": 9, "bottom": 9})"))
                     .has_value());
}

TEST_F(WindowPlacement_, RectWithNoSizeGivesNothing) {
    EXPECT_FALSE(
        loadWindowPlacement(write(R"({"maximized": false, "left": 100, "top": 100, "right": 100, "bottom": 500})"))
            .has_value());
    EXPECT_FALSE(
        loadWindowPlacement(write(R"({"maximized": false, "left": 100, "top": 500, "right": 900, "bottom": 400})"))
            .has_value());
}

TEST(WindowPlacement, ReopensMaximizedOnlyIfItWasOrWouldBe) {
    EXPECT_TRUE(reopenMaximized(/*minimized=*/false, /*maximized=*/true, /*restoresToMaximized=*/false));
    EXPECT_FALSE(reopenMaximized(false, false, false));
    // Minimized: it comes back the way it would have restored, never minimized.
    EXPECT_TRUE(reopenMaximized(true, false, true));
    EXPECT_FALSE(reopenMaximized(true, false, false));
}
