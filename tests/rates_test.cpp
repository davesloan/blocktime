// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "rates.h"

using namespace blocktime;
namespace fs = std::filesystem;

namespace {

std::string repoRates() {
    std::ifstream in(BLOCKTIME_SOURCE_DIR "/config/rates.yaml", std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

fs::path writeTemp(const std::string& text) {
    const fs::path file = fs::temp_directory_path() / "blocktime-rates-test" / "rates.yaml";
    fs::create_directories(file.parent_path());
    std::ofstream(file, std::ios::binary) << text;
    return file;
}

}  // namespace

TEST(Rates, RepoFileMatchesTheDefaults) {
    std::string error;
    const auto rates = loadRates(BLOCKTIME_SOURCE_DIR "/config/rates.yaml", &error);
    ASSERT_TRUE(rates) << error;
    const Rates d;
    EXPECT_EQ(rates->fuelPer100Lb, 4100);
    EXPECT_EQ(rates->landingPer1000LbMlw, d.landingPer1000LbMlw);
    EXPECT_EQ(rates->cargoHandlingPer1000Lb, d.cargoHandlingPer1000Lb);
    EXPECT_EQ(rates->flightAttendantPerHour, d.flightAttendantPerHour);
    EXPECT_EQ(rates->narrowbody, d.narrowbody);
    EXPECT_EQ(rates->widebody, d.widebody);
    EXPECT_EQ(rates->catering, d.catering);
    EXPECT_EQ(rates->economyPerNm, 0.075);
    EXPECT_EQ(rates->vipFarePerNm, 0.48);
    EXPECT_EQ(rates->cargoPer100LbPerNm, 0.028);
    EXPECT_EQ(*rates, d);  // every field, including the ones not named above
}

TEST(Rates, MissingKeyNamesFileAndKey) {
    std::string text = repoRates();
    const auto at = text.find("fuel_per_100lb:");
    ASSERT_NE(at, std::string::npos);
    text.replace(at, std::string("fuel_per_100lb:").size(), "fuel_cost_per_100lb:");
    std::string error;
    EXPECT_FALSE(loadRates(writeTemp(text), &error));
    EXPECT_NE(error.find("rates.yaml"), std::string::npos) << error;
    EXPECT_NE(error.find("fuel_per_100lb"), std::string::npos) << error;
}

TEST(Rates, FuelPriceKeepsCents) {
    std::string text = repoRates();
    const auto at = text.find("fuel_per_100lb: 41");
    ASSERT_NE(at, std::string::npos);
    text.replace(at, std::string("fuel_per_100lb: 41").size(), "fuel_per_100lb: 41.5");
    std::string error;
    const auto rates = loadRates(writeTemp(text), &error);
    ASSERT_TRUE(rates) << error;
    EXPECT_EQ(rates->fuelPer100Lb, 4150);
}
