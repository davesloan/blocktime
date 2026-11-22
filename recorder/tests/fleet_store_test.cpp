// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <filesystem>
#include <string>

#include <gtest/gtest.h>

#include "fleet_store.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

class FleetStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() /
               ("blocktime-test-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        fs::remove_all(dir_);
        fs::create_directories(dir_);
    }
    void TearDown() override { fs::remove_all(dir_); }

    fs::path dir_;
};

std::optional<FleetStore> openMemory() {
    std::string error;
    auto store = FleetStore::open(":memory:", &error);
    EXPECT_TRUE(store.has_value()) << error;
    return store;
}

}  // namespace

TEST_F(FleetStoreTest, OpensInMemory) {
    std::string error;
    auto store = FleetStore::open(":memory:", &error);
    ASSERT_TRUE(store.has_value()) << error;
    EXPECT_TRUE(error.empty());
}

TEST_F(FleetStoreTest, EmptyUntilSeeded) {
    auto store = openMemory();
    ASSERT_TRUE(store);
    EXPECT_TRUE(store->airframes().empty());
}

TEST_F(FleetStoreTest, SeedGivesEightOrderedByRegistration) {
    auto store = openMemory();
    ASSERT_TRUE(store);
    store->seedIfEmpty();
    const auto fleet = store->airframes();
    ASSERT_EQ(fleet.size(), 8u);
    EXPECT_EQ(fleet.front().registration, "N601SX");
    EXPECT_EQ(fleet.back().registration, "N916SX");
    for (std::size_t i = 1; i < fleet.size(); ++i) EXPECT_LT(fleet[i - 1].registration, fleet[i].registration);

    const auto n818 = std::find_if(fleet.begin(), fleet.end(), [](const Airframe& a) { return a.registration == "N818SX"; });
    ASSERT_NE(n818, fleet.end());
    EXPECT_EQ(n818->type, "737-800");
    EXPECT_EQ(n818->configuration, "Two-class");
    EXPECT_EQ(n818->location, "KSAN");
    EXPECT_EQ(n818->status, "Parked");
}

TEST_F(FleetStoreTest, SeedOnlyIntoEmptyTable) {
    auto store = openMemory();
    ASSERT_TRUE(store);
    store->seedIfEmpty();
    store->seedIfEmpty();
    EXPECT_EQ(store->airframes().size(), 8u);
}

TEST_F(FleetStoreTest, ReopenKeepsRows) {
    const std::string file = (dir_ / "blocktime.db").string();
    std::string error;
    {
        auto store = FleetStore::open(file, &error);
        ASSERT_TRUE(store.has_value()) << error;
        store->seedIfEmpty();
    }
    auto again = FleetStore::open(file, &error);
    ASSERT_TRUE(again.has_value()) << error;
    EXPECT_EQ(again->airframes().size(), 8u);
}

TEST_F(FleetStoreTest, UserVersionIsOne) {
    auto store = openMemory();
    ASSERT_TRUE(store);
    EXPECT_EQ(store->userVersion(), 1);
}

TEST_F(FleetStoreTest, OpenFailsInMissingFolder) {
    std::string error;
    auto store = FleetStore::open((dir_ / "missing" / "blocktime.db").string(), &error);
    EXPECT_FALSE(store.has_value());
    EXPECT_FALSE(error.empty());
}

TEST_F(FleetStoreTest, MovedStoreStillWorks) {
    auto a = openMemory();
    ASSERT_TRUE(a);
    a->seedIfEmpty();
    FleetStore b = std::move(*a);
    EXPECT_EQ(b.airframes().size(), 8u);
}
