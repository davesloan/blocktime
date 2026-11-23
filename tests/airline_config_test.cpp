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
fleets: [Executive]
airplanes:
  - registration: N738BT
    airframe: 737-800 BBJ2 VIP
    fleet: Executive
    simbrief_airframe_id: "1_1"
    leased_on: 2026-11-22
    location: KBFI
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
    ASSERT_EQ(config->fleets.size(), 1u);
    ASSERT_EQ(config->airplanes.size(), 1u);
    const ConfigAirplane& a = config->airplanes[0];
    EXPECT_EQ(a.registration, "N738BT");
    EXPECT_EQ(a.tailNumber, "738");
    EXPECT_EQ(a.airframe, "737-800 BBJ2 VIP");
    EXPECT_EQ(a.fleet, "Executive");
    EXPECT_EQ(a.locationIcao, "KBFI");
    EXPECT_EQ(a.leasedOn, (Date{year{2026}, month{11}, day{22}}));
    EXPECT_TRUE(a.awaitingDelivery);
}

TEST(Config, OptionalFieldsDefault) {
    std::string error;
    const auto config = parseAirlineConfig(kConfig, &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->airplanes[0].tailNumber, "");
    EXPECT_FALSE(config->airplanes[0].awaitingDelivery);
}

TEST(Config, RejectsLeaseBeforeFounding) {
    const std::string error = parseError(replaced(kConfig, "leased_on: 2026-11-22", "leased_on: 2026-11-21"));
    EXPECT_NE(error.find("before founded_on"), std::string::npos) << error;
}

TEST(Config, RejectsDuplicateRegistration) {
    const std::string second = R"(  - registration: N738BT
    airframe: 737-800 BBJ2 VIP
    fleet: Executive
    simbrief_airframe_id: "1_2"
    leased_on: 2026-11-22
    location: KBFI
)";
    const std::string error = parseError(kConfig + second);
    EXPECT_NE(error.find("duplicate registration"), std::string::npos) << error;
}

TEST(Config, RejectsUnknownFleet) {
    const std::string error = parseError(replaced(kConfig, "fleet: Executive", "fleet: Cargo"));
    EXPECT_NE(error.find("fleet"), std::string::npos) << error;
    EXPECT_NE(error.find("Cargo"), std::string::npos) << error;
}

TEST(Config, RejectsBadValues) {
    EXPECT_NE(parseError(replaced(kConfig, "icao: BTA", "icao: bt")).find("icao"), std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "5000000", "5000001")).find("multiple of 5"), std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "N738BT", "n738 bt")).find("registration"), std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "location: KBFI", "location: BFI")).find("location"), std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "founded_on: 2026-11-22", "founded_on: 22/11/2026")).find("founded_on"),
              std::string::npos);
    EXPECT_NE(parseError(replaced(kConfig, "    location: KBFI\n", "")).find("location"), std::string::npos);
}

TEST(Config, YamlErrorNamesTheLine) {
    const std::string error = parseError("airline:\n  name: X\n icao: BTA\n");
    EXPECT_EQ(error.rfind("airline.yaml line ", 0), 0u) << error;
}

TEST(Config, LimitOverridesAndFuelPrice) {
    std::string error;
    auto config = parseAirlineConfig(kConfig, &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->fuelPriceCentsPer100Lb, 4100);
    EXPECT_FALSE(config->airplanes[0].limits.mlwLb.has_value());

    const std::string yaml =
        replaced(replaced(kConfig, "location: KBFI", "location: KBFI\n    limits: { mlw_lb: 140000 }"),
                 "starting_cash: 5000000", "starting_cash: 5000000\nfuel_price_per_100lb: 41.5");
    config = parseAirlineConfig(yaml, &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->fuelPriceCentsPer100Lb, 4150);
    const LimitOverrides& o = config->airplanes[0].limits;
    EXPECT_EQ(o.mlwLb, 140000);
    EXPECT_FALSE(o.minRunwayFt.has_value());
    EXPECT_FALSE(o.passengerWeightLb.has_value());
    EXPECT_FALSE(o.bagWeightLb.has_value());
    EXPECT_FALSE(o.maxPayloadLb.has_value());

    const AirplaneLimits merged = withOverrides({7000, 146300, 190, 55, 36113}, o);
    EXPECT_EQ(merged.mlwLb, 140000);
    EXPECT_EQ(merged.minRunwayFt, 7000);
    EXPECT_EQ(merged.maxPayloadLb, 36113);
}

TEST(Config, FlightNumberIsOptional) {
    std::string error;
    auto config = parseAirlineConfig(kConfig, &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->airplanes[0].flightNumber, "");
    config = parseAirlineConfig(replaced(kConfig, "location: KBFI", "location: KBFI\n    flight_number: 8"), &error);
    ASSERT_TRUE(config.has_value()) << error;
    EXPECT_EQ(config->airplanes[0].flightNumber, "8");
}
