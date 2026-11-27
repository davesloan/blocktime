// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>

#include <gtest/gtest.h>

#include "airline_config.h"

using namespace blocktime;
using namespace std::chrono;

namespace {

const std::string kConfig = R"(airline: { name: BlockTime Air, icao: BTA, callsign: BLOCKTIME }
founded_on: 2026-11-22
starting_cash: 5000000
)";

std::string replaced(std::string text, const std::string& from, const std::string& to) {
    text.replace(text.find(from), from.size(), to);
    return text;
}

std::string parseError(const std::string& yaml) {
    std::string error;
    EXPECT_FALSE(parseAirlineConfig(yaml, &error).has_value());
    return error;
}

}  // namespace

TEST(Config, ParsesTheExample) {
    std::string error;
    const auto config = loadAirlineConfig(BLOCKTIME_SOURCE_DIR "/config/airline.example.yaml", &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->identity.name, "BlockTime Air");
    EXPECT_EQ(config->identity.icaoCode, "BTA");
    EXPECT_EQ(config->identity.callsign, "BLOCKTIME");
    EXPECT_EQ(config->foundedOn, (Date{year{2026}, month{11}, day{22}}));
    EXPECT_EQ(config->startingCashCents, 500'000'000);
}

// Airplanes are leased in the app now (D-242) and fleets are gone (D-244): an older file's lists are ignored.
TEST(Config, IgnoresFleetsAndAirplanes) {
    std::string error;
    const auto config = parseAirlineConfig(kConfig + R"(fleets: [Executive]
airplanes:
  - registration: N738BT
    airframe: 737-800 BBJ2 VIP
    fleet: Executive
)",
                                           &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->identity.icaoCode, "BTA");
}

TEST(Config, RejectsBadValues) {
    EXPECT_NE(parseError(replaced(kConfig, "icao: BTA", "icao: bt")).find("icao"), std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "5000000", "5000001")).find("multiple of 5"), std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "founded_on: 2026-11-22", "founded_on: 22/11/2026")).find("founded_on"),
              std::string::npos);
}

TEST(Config, YamlErrorNamesTheLine) {
    const std::string error = parseError("airline:\n  name: X\n icao: BTA\n");
    EXPECT_EQ(error.rfind("airline.yaml line ", 0), 0u) << error;
}

TEST(Config, ReadsPreferredGates) {
    std::string error;
    const auto config = parseAirlineConfig(kConfig + "preferred_gates:\n  KPDX: [D14, D12]\n  KSAN: [\"20\"]\n", &error);
    ASSERT_TRUE(config.has_value()) << error;
    ASSERT_EQ(config->preferredGates.size(), 2u);
    EXPECT_EQ(config->preferredGates.at("KPDX"), (std::vector<std::string>{"D14", "D12"}));
    EXPECT_EQ(config->preferredGates.at("KSAN"), (std::vector<std::string>{"20"}));
}

TEST(Config, PreferredGatesAreOptional) {
    std::string error;
    const auto config = parseAirlineConfig(kConfig, &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_TRUE(config->preferredGates.empty());
}
