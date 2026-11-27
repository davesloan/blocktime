// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "airframe_read.h"
#include "fleet_rules.h"

using namespace blocktime;

namespace {

std::string readFixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

}  // namespace

TEST(AirframeRead, RequestCarriesImageAndSchema) {
    const std::string bytes = "\x89PNG\r\n\x1a\nabc";
    const auto j = nlohmann::json::parse(buildAirframeReadRequest(bytes, "image/png"));
    EXPECT_EQ(j["model"], "claude-opus-5-5");
    const auto& content = j["messages"][0]["content"];
    ASSERT_TRUE(content.is_array());
    const auto& image = content[0];
    EXPECT_EQ(image["type"], "image");
    EXPECT_EQ(image["source"]["type"], "base64");
    EXPECT_EQ(image["source"]["media_type"], "image/png");
    EXPECT_EQ(image["source"]["data"], "iVBORw0KGgphYmM=");
    const auto& schema = j["output_config"]["format"]["schema"];
    EXPECT_EQ(schema["required"].size(), 15u);
    EXPECT_EQ(schema["additionalProperties"], false);
    EXPECT_NE(j["system"].get<std::string>().find("Max Landing Weight"), std::string::npos);
}

TEST(AirframeRead, ParsesGoodAnswer) {
    const AirframeReadAnswer a = parseAirframeReadResponse(200, readFixture("airframe_read_ok.json"));
    ASSERT_TRUE(a.reading.has_value()) << a.error;
    const SimbriefReading& r = *a.reading;
    EXPECT_EQ(r.registration, "N738SX");
    EXPECT_EQ(r.finNumber, "738");
    EXPECT_EQ(r.internalId, "730097_1791702236118");
    EXPECT_EQ(r.baseType, "B738");
    EXPECT_EQ(r.maxPassengers, 160);
    EXPECT_EQ(r.mlwLb, 146300);
    EXPECT_EQ(r.maxCargoLb, 18000);
    EXPECT_EQ(r.minRunwayFt, 7000);
    EXPECT_TRUE(missingFields(r).empty());
}

// Review Focus 1: a field Claude couldn't see stays unset, and the form asks for it.
TEST(AirframeRead, NullFieldsStayUnset) {
    const AirframeReadAnswer a = parseAirframeReadResponse(200, readFixture("airframe_read_partial.json"));
    ASSERT_TRUE(a.reading.has_value()) << a.error;
    EXPECT_FALSE(a.reading->mlwLb.has_value());
    EXPECT_EQ(missingFields(*a.reading), std::vector<std::string>{"Max Landing Weight"});
}

TEST(AirframeRead, Errors) {
    EXPECT_EQ(parseAirframeReadResponse(401, R"({"type":"error","error":{"type":"authentication_error",)"
                                             R"("message":"invalid x-api-key"}})")
                  .error,
              "Claude: invalid x-api-key");
    EXPECT_EQ(parseAirframeReadResponse(200, R"({"stop_reason":"refusal","content":[]})").error,
              "Claude declined to read it");
    EXPECT_EQ(parseAirframeReadResponse(200, "not json").error, "Claude's answer wasn't readable");
    std::string kgs = readFixture("airframe_read_ok.json");
    kgs.replace(kgs.find("\\\"LBS\\\""), 7, "\\\"KGS\\\"");
    EXPECT_EQ(parseAirframeReadResponse(200, kgs).error, "Set SimBrief's weight units to LBS and take a new screenshot.");
}
