// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The event feed's lines, shared by the recorder and the replay (milestone 11).

#include <gtest/gtest.h>

#include "event_log.h"
#include "feed_text.h"

using namespace blocktime;

TEST(FeedText, FlightEvents) {
    EXPECT_EQ(feedText("phase_changed", {{"phase", "climb"}}), "CLIMB");
    EXPECT_EQ(feedText("phase_changed", {{"phase", "taxi_out"}}), "TAXI OUT");
    EXPECT_EQ(feedText("liftoff", {{"runway", "14R"}}), "LIFTOFF  14R");
    EXPECT_EQ(feedText("flight_accepted", {{"origin", "KSAN"}, {"destination", "KSJC"}, {"at_block_out", false}}),
              "FLIGHT ACCEPTED  KSAN → KSJC");
    EXPECT_EQ(feedText("flight_accepted", {{"origin", "KSAN"}, {"destination", "KSJC"}, {"at_block_out", true}}),
              "FLIGHT ACCEPTED  KSAN → KSJC  (at block out)");
    EXPECT_EQ(feedText("lined_up", {{"runway", "14R"}, {"distance_nm", 0.31}}), "LINED UP  runway 14R  0.3 nm");
    EXPECT_EQ(feedText("gsx_service", {{"service", "boarding"}, {"state", "performing"}}), "GSX boarding performing");
    EXPECT_EQ(feedText("gsx_service", {{"service", "boarding"}, {"state", "available"}}), "");
    EXPECT_EQ(feedText("touchdown", {{"vs_fpm", 120.4}, {"g", 1.214}}), "TOUCHDOWN  120 fpm  1.21 g");
    EXPECT_EQ(feedText("arrival_gate", {{"gate", "D14"}, {"result", "assigned"}}), "ARRIVAL GATE  D14  (assigned)");
    EXPECT_EQ(feedText("level_off", {{"alt_ft", 5000}}), "");
}

TEST(FeedText, BlockEvents) {
    BlockOut out;
    out.time = *parseUtc("2026-11-25T18:43:04.053Z");
    out.atcId = "N814SX";
    out.fuelLb = 15316.4;
    EXPECT_EQ(blockFeedText(out), "BLOCK OUT  N814SX  released 18:43:04  fuel 15316 lb");

    BlockIn in;
    in.time = *parseUtc("2026-11-25T20:01:45.912Z");
    in.fuelLb = 9876;
    in.blockTimeS = 4718;
    EXPECT_EQ(blockFeedText(in), "BLOCK IN  brake set 20:01:45  fuel 9876 lb  block 1:18");

    BlockOutRefused refused;
    refused.match.problem = MatchProblem::WrongAirport;
    EXPECT_EQ(blockFeedText(refused), "NOT RECORDING  " + std::string(problemCode(MatchProblem::WrongAirport)));

    EXPECT_EQ(blockFeedText(FlightInterrupted{"x", "sim_stopped"}), "FLIGHT INTERRUPTED  (sim_stopped)");
}
