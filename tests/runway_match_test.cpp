// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Naming the runway from the ground roll (D-126), on the probe flights' rolls and the sim's runways at KBFI, KPDX and
// KSFO (tests/data/runways-*.json, from the probe's facility data).

#include <algorithm>
#include <cmath>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "replay.h"
#include "runway_match.h"

using namespace blocktime;

namespace {

AirportRunways airport(const std::string& icao) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/runways-") + icao + ".json");
    return airportRunwaysFromJson(nlohmann::json::parse(in));
}

std::vector<AirportRunways> allAirports() { return {airport("KBFI"), airport("KPDX"), airport("KSFO")}; }

// The takeoff roll: on the ground at 40 kt or more, before the first airborne sample. The landing roll: the same,
// after the last airborne sample.
std::vector<RollPoint> roll(const std::string& fixture, bool takeoff) {
    const auto r = replay::loadReplay(fixture);
    const auto& s = r.samples;
    std::size_t firstAir = 0, lastAir = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i].onGround) continue;
        if (firstAir == 0) firstAir = i;
        lastAir = i;
    }
    std::vector<RollPoint> out;
    const std::size_t from = takeoff ? 0 : lastAir + 1, to = takeoff ? firstAir : s.size();
    for (std::size_t i = from; i < to; ++i)
        if (s[i].onGround && s[i].groundSpeedKt >= 40) out.push_back({s[i].lat, s[i].lon});
    return out;
}

// Moves every point `m` metres toward `bearingDeg`.
std::vector<RollPoint> shifted(std::vector<RollPoint> pts, double m, double bearingDeg) {
    const double b = bearingDeg * std::numbers::pi / 180.0;
    for (auto& p : pts) {
        p.lat += m * std::cos(b) / 111195.0;
        p.lon += m * std::sin(b) / (111195.0 * std::cos(p.lat * std::numbers::pi / 180.0));
    }
    return pts;
}

}  // namespace

TEST(RunwayMatch, NamesTheProbeFlightsRunways) {
    const auto airports = allAirports();
    struct Case {
        std::string fixture;
        bool takeoff;
        std::string icao, runway;
    };
    for (const Case& c : {Case{"replay-kbfi-kpdx.csv", true, "KBFI", "14R"},
                          Case{"replay-kbfi-kpdx.csv", false, "KPDX", "10R"},
                          Case{"replay-kpdx-ksfo.csv", true, "KPDX", "10R"},
                          Case{"replay-kpdx-ksfo.csv", false, "KSFO", "28R"}}) {
        const RunwayName n = nameRunway(airports, roll(c.fixture, c.takeoff));
        EXPECT_EQ(n.icao, c.icao) << c.fixture << (c.takeoff ? " takeoff" : " landing");
        EXPECT_EQ(n.runway, c.runway) << c.fixture << (c.takeoff ? " takeoff" : " landing");
        EXPECT_LT(n.offCentrelineM, 10.0);
        EXPECT_LT(n.offHeadingDeg, 2.0);
    }
}

TEST(RunwayMatch, DirectionPicksTheEnd) {
    auto pts = roll("replay-kbfi-kpdx.csv", true);
    std::reverse(pts.begin(), pts.end());
    EXPECT_EQ(nameRunway(allAirports(), pts).runway, "32L");
}

TEST(RunwayMatch, NoRunwaysGiveNoName) {
    const auto pts = roll("replay-kbfi-kpdx.csv", true);
    EXPECT_EQ(nameRunway({}, pts).runway, "");
    EXPECT_EQ(nameRunway({airport("KSFO")}, pts).runway, "");
    EXPECT_EQ(nameRunway(allAirports(), {}).runway, "");
}

TEST(RunwayMatch, TooFarFromAnyCentreline) {
    // 150 m to the south-west of 14R, away from the parallel 14L.
    const auto pts = shifted(roll("replay-kbfi-kpdx.csv", true), 150, 240);
    EXPECT_EQ(nameRunway(allAirports(), pts).runway, "");
}

TEST(RunwayMatch, ShortRollGivesNoName) {
    auto pts = roll("replay-kbfi-kpdx.csv", true);
    pts.resize(2);  // two samples a second apart at about 45 kt: well under 300 m
    EXPECT_EQ(nameRunway(allAirports(), pts).runway, "");
}

TEST(RunwayMatch, RunwayEndNames) {
    EXPECT_EQ(runwayEndName(3, 0), "03");
    EXPECT_EQ(runwayEndName(10, 2), "10R");
    EXPECT_EQ(runwayEndName(28, 1), "28L");
    EXPECT_EQ(runwayEndName(16, 3), "16C");
}

TEST(RunwayMatch, JsonRoundTrip) {
    const AirportRunways a = airport("KPDX");
    ASSERT_EQ(a.runways.size(), 3u);
    EXPECT_EQ(a.icao, "KPDX");
    EXPECT_EQ(a.runways[1].primaryNumber, 10);
    EXPECT_EQ(a.runways[1].primaryDesignator, 1);
    EXPECT_NEAR(a.runways[1].headingDeg, 119.05, 0.01);
    EXPECT_NEAR(a.runways[1].lengthM, 2996.1, 0.1);
    const AirportRunways back = airportRunwaysFromJson(toJson(a));
    EXPECT_EQ(back.icao, "KPDX");
    ASSERT_EQ(back.runways.size(), 3u);
    EXPECT_EQ(back.runways[2].secondaryNumber, a.runways[2].secondaryNumber);
    EXPECT_DOUBLE_EQ(back.runways[2].lat, a.runways[2].lat);
    EXPECT_DOUBLE_EQ(back.runways[2].widthM, a.runways[2].widthM);
}
