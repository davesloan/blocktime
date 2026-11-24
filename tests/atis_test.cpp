// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "sayintentions.h"

using namespace blocktime;

namespace {

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(Atis, GetWxUrl) {
    EXPECT_EQ(getWxUrl("pilot-key", "KBFI", "KSAN"),
              "https://apipri.sayintentions.ai/sapi/getWX?api_key=pilot-key&icao=KBFI,KSAN");
}

TEST(Atis, ParsesActiveRunways) {
    const AtisRunways r = parseAtisRunways(fixture("getwx_live.json"), "KBFI", "KSAN");
    EXPECT_EQ(r.origin, "14R");
    EXPECT_EQ(r.destination, "27");
}

TEST(Atis, FallsBackToActiveRunway) {
    const AtisRunways r = parseAtisRunways(
        R"({"airports":[{"airport":"KBFI","active_runway":"9"},{"airport":"KSAN","active_runway":27}]})", "KBFI", "KSAN");
    EXPECT_EQ(r.origin, "09");
    EXPECT_EQ(r.destination, "27");
}

TEST(Atis, NoAirportsOrErrorGivesNone) {
    for (const char* body : {R"({"airports":[]})", R"({"error":"The provided API key is invalid."})", "not json", ""}) {
        const AtisRunways r = parseAtisRunways(body, "KBFI", "KSAN");
        EXPECT_EQ(r.origin, "") << body;
        EXPECT_EQ(r.destination, "") << body;
    }
}

TEST(Atis, OneAirportWithoutRunways) {
    const AtisRunways r =
        parseAtisRunways(R"({"airports":[{"airport":"KBFI","atis":""},{"airport":"KSAN","active_runways_arriving":"27,28"}]})",
                         "KBFI", "KSAN");
    EXPECT_EQ(r.origin, "");
    EXPECT_EQ(r.destination, "27");
}

TEST(Atis, NormalizeRunway) {
    EXPECT_EQ(normalizeRunway("9R"), "09R");
    EXPECT_EQ(normalizeRunway("16L"), "16L");
    EXPECT_EQ(normalizeRunway("27"), "27");
    EXPECT_EQ(normalizeRunway(" 34C "), "34C");
    EXPECT_EQ(normalizeRunway("RWY27"), "");
    EXPECT_EQ(normalizeRunway("127"), "");
    EXPECT_EQ(normalizeRunway(""), "");
}

TEST(Atis, ErrorText) {  // review finding 2: a bad key must not look like "no ATIS"
    EXPECT_EQ(atisError(R"({"error":"The provided API key is invalid."})"), "The provided API key is invalid.");
    EXPECT_EQ(atisError(fixture("getwx_live.json")), "");
    EXPECT_EQ(atisError(R"({"airports":[]})"), "");
    EXPECT_EQ(atisError("not json"), "");
}

TEST(Atis, ParsesTheLiveFields) {
    const auto a = parseAtis(fixture("getwx_live.json"), "KBFI");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->icao, "KBFI");
    EXPECT_EQ(a->letter, "K");
    EXPECT_EQ(a->altimeter, "30.02");
    EXPECT_EQ(a->departing, "14R,14L");
    EXPECT_EQ(a->arriving, "14R");
    EXPECT_EQ(a->text.rfind("Boeing Field information Kilo", 0), 0u);
}

TEST(Atis, NumbersComeBackAsText) {
    const auto a = parseAtis(R"({"airports":[{"airport":"KSFO","current":"C","atis":"x","altimeter":29.92}]})", "KSFO");
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(a->altimeter, "29.92");
}

TEST(Atis, ErrorOrUnknownAirportIsNothing) {
    EXPECT_FALSE(parseAtis(R"({"error":"The provided API key is invalid."})", "KBFI").has_value());
    EXPECT_FALSE(parseAtis(R"({"airports":[]})", "KBFI").has_value());
    EXPECT_FALSE(parseAtis(fixture("getwx_live.json"), "KSFO").has_value());
    EXPECT_FALSE(parseAtis("not json", "KBFI").has_value());
}

TEST(Atis, OneAirportUrl) {
    EXPECT_EQ(getWxUrl("k e y", "KBFI"), "https://apipri.sayintentions.ai/sapi/getWX?api_key=k%20e%20y&icao=KBFI");
    EXPECT_EQ(commsHistoryUrl("abc"), "https://apipri.sayintentions.ai/sapi/getCommsHistory?api_key=abc");
}
