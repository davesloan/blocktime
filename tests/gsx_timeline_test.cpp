// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GsxTimeline: gsx_service events as services, each given its leg (milestone 8, D-138).

#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "gsx_timeline.h"

using namespace blocktime;

namespace {

std::string at(const std::string& hms) { return "2026-11-24T" + hms + ".000Z"; }

void gsx(GsxTimeline& t, const std::string& service, const std::string& state, const std::string& hms) {
    t.onEvent("gsx_service", {{"service", service}, {"state", state}, {"time", at(hms)}});
}
void block(GsxTimeline& t, const std::string& type, const std::string& hms, const std::string& id = "F1") {
    t.onEvent(type, {{"flight_id", id}, {"time", at(hms)}});
}
void sim(GsxTimeline& t, const std::string& type, const std::string& hms) { t.onEvent(type, {{"time", at(hms)}}); }

}  // namespace

TEST(GsxTimeline, RefuelCompletesAtCompleted) {
    GsxTimeline t;
    gsx(t, "refuelling", "requested", "10:00:00");
    gsx(t, "refuelling", "performing", "10:01:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    gsx(t, "refuelling", "available", "10:05:10");
    ASSERT_EQ(t.nextLeg().size(), 1u);
    EXPECT_EQ(t.nextLeg()[0].service, "refuelling");
    EXPECT_EQ(t.nextLeg()[0].startIso, at("10:01:00"));
    EXPECT_EQ(t.nextLeg()[0].endIso, at("10:05:00"));
    EXPECT_FALSE(t.nextLeg()[0].running);
}

TEST(GsxTimeline, BoardingCompletesOnAvailable) {
    GsxTimeline t;
    gsx(t, "boarding", "performing", "10:00:00");
    gsx(t, "boarding", "available", "10:20:00");
    ASSERT_EQ(t.nextLeg().size(), 1u);
    EXPECT_EQ(t.nextLeg()[0].endIso, at("10:20:00"));
    EXPECT_FALSE(t.nextLeg()[0].running);
}

TEST(GsxTimeline, PerformingIsRunning) {
    GsxTimeline t;
    gsx(t, "catering", "performing", "10:00:00");
    ASSERT_EQ(t.nextLeg().size(), 1u);
    EXPECT_TRUE(t.nextLeg()[0].running);
    EXPECT_EQ(t.nextLeg()[0].endIso, "");
}

TEST(GsxTimeline, RequestedAloneIsNoSpan) {
    GsxTimeline t;
    gsx(t, "catering", "requested", "10:00:00");
    gsx(t, "catering", "available", "10:01:00");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, BypassedIsNoSpan) {
    GsxTimeline t;
    gsx(t, "catering", "bypassed", "10:00:00");
    gsx(t, "catering", "available", "10:01:00");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, AbandonedSpanIsRemoved) {
    GsxTimeline t;
    gsx(t, "catering", "performing", "10:00:00");
    gsx(t, "catering", "bypassed", "10:02:00");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, BlockOutTakesCompletedServices) {
    GsxTimeline t;
    gsx(t, "refuelling", "performing", "10:00:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    gsx(t, "boarding", "performing", "10:06:00");
    block(t, "block_out", "10:10:00");
    ASSERT_EQ(t.departed().size(), 1u);
    EXPECT_EQ(t.departed()[0].service, "refuelling");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, ServicesAfterBlockInAreTheNextLegs) {
    GsxTimeline t;
    gsx(t, "boarding", "performing", "10:00:00");
    gsx(t, "boarding", "completed", "10:05:00");
    block(t, "block_out", "10:10:00");
    block(t, "block_in", "12:00:00");
    gsx(t, "refuelling", "performing", "12:05:00");
    gsx(t, "refuelling", "completed", "12:10:00");
    ASSERT_EQ(t.nextLeg().size(), 1u);
    EXPECT_EQ(t.nextLeg()[0].service, "refuelling");
    ASSERT_EQ(t.departed().size(), 1u);
    EXPECT_EQ(t.departed()[0].service, "boarding");
}

TEST(GsxTimeline, DeboardingBelongsToArrival) {
    GsxTimeline t;
    block(t, "block_out", "10:00:00");
    block(t, "block_in", "12:00:00");
    gsx(t, "deboarding", "requested", "12:01:00");
    gsx(t, "deboarding", "performing", "12:02:00");
    gsx(t, "deboarding", "completed", "12:06:00");
    ASSERT_EQ(t.arrival().size(), 1u);
    EXPECT_EQ(t.arrival()[0].startIso, at("12:02:00"));
    EXPECT_EQ(t.arrival()[0].endIso, at("12:06:00"));
    EXPECT_EQ(t.lastBlockInFlightId(), "F1");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, DeboardingBeforeAnyBlockInIgnored) {
    GsxTimeline t;
    gsx(t, "deboarding", "performing", "10:00:00");
    gsx(t, "deboarding", "completed", "10:05:00");
    EXPECT_TRUE(t.arrival().empty());
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, BlockOutEndsTheArrival) {
    GsxTimeline t;
    block(t, "block_out", "10:00:00");
    block(t, "block_in", "12:00:00");
    gsx(t, "deboarding", "performing", "12:02:00");
    gsx(t, "deboarding", "completed", "12:06:00");
    block(t, "block_out", "13:00:00", "F2");
    EXPECT_TRUE(t.arrival().empty());
    EXPECT_EQ(t.lastBlockInFlightId(), "");
}

TEST(GsxTimeline, SimStoppedClearsNextLeg) {
    GsxTimeline t;
    gsx(t, "refuelling", "performing", "10:00:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    sim(t, "sim_stopped", "10:06:00");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, SimDisconnectedClearsNextLeg) {
    GsxTimeline t;
    gsx(t, "refuelling", "performing", "10:00:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    sim(t, "sim_disconnected", "10:06:00");
    EXPECT_TRUE(t.nextLeg().empty());
}

TEST(GsxTimeline, RecorderExitKeepsNextLeg) {
    GsxTimeline t;
    gsx(t, "refuelling", "performing", "10:00:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    // BlockTime closed for a new build while the sim carried on: the services already done still count.
    t.onEvent("sim_disconnected", {{"reason", "recorder_exit"}, {"time", at("10:06:00")}});
    ASSERT_EQ(t.nextLeg().size(), 1u);
    EXPECT_EQ(t.nextLeg()[0].service, "refuelling");
}

TEST(GsxTimeline, SimStoppedDropsRunningDeboarding) {
    GsxTimeline t;
    block(t, "block_out", "10:00:00");
    block(t, "block_in", "12:00:00");
    gsx(t, "deboarding", "performing", "12:02:00");
    sim(t, "sim_stopped", "12:03:00");
    EXPECT_TRUE(t.arrival().empty());
}

TEST(GsxTimeline, InterruptedDropsServices) {
    GsxTimeline t;
    gsx(t, "refuelling", "performing", "10:00:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    block(t, "block_out", "10:10:00");
    block(t, "flight_interrupted", "10:20:00");
    EXPECT_TRUE(t.departed().empty());
    EXPECT_TRUE(t.nextLeg().empty());
    EXPECT_TRUE(t.arrival().empty());
}

TEST(GsxTimeline, PushbackByGsx) {
    GsxTimeline t;
    gsx(t, "pushback", "requested", "10:00:00");
    gsx(t, "pushback", "performing", "10:01:00");
    block(t, "block_out", "10:01:10");
    EXPECT_TRUE(t.pushbackByGsx());
    EXPECT_TRUE(t.nextLeg().empty());  // pushback is never a service line
    GsxTimeline u;
    block(u, "block_out", "10:01:10");
    EXPECT_FALSE(u.pushbackByGsx());
}

TEST(GsxTimeline, PushbackOverIsNotByGsx) {
    GsxTimeline t;
    gsx(t, "pushback", "performing", "09:00:00");
    gsx(t, "pushback", "available", "09:03:00");  // an earlier push, long done
    block(t, "block_out", "10:01:10");
    EXPECT_FALSE(t.pushbackByGsx());
}

TEST(GsxTimeline, TwoRefuelsBothKept) {
    GsxTimeline t;
    gsx(t, "refuelling", "performing", "10:00:00");
    gsx(t, "refuelling", "completed", "10:05:00");
    gsx(t, "refuelling", "available", "10:05:10");
    gsx(t, "refuelling", "performing", "10:20:00");
    gsx(t, "refuelling", "completed", "10:22:00");
    ASSERT_EQ(t.nextLeg().size(), 2u);
    EXPECT_EQ(t.nextLeg()[1].startIso, at("10:20:00"));
}

// N916SX KSAN to KSLC, 2026-11-24 (events-2026-11-24.jsonl).
TEST(GsxTimeline, KslcTurnaround) {
    GsxTimeline t;
    gsx(t, "refuelling", "requested", "05:32:59");
    gsx(t, "catering", "requested", "05:33:24");
    gsx(t, "refuelling", "performing", "05:34:35");
    gsx(t, "catering", "performing", "05:35:33");
    gsx(t, "boarding", "requested", "05:38:32");
    gsx(t, "boarding", "performing", "05:38:57");
    gsx(t, "catering", "completed", "05:39:23");
    gsx(t, "catering", "available", "05:39:33");
    gsx(t, "refuelling", "completed", "05:39:42");
    gsx(t, "refuelling", "available", "05:39:53");
    gsx(t, "boarding", "completed", "05:47:24");
    gsx(t, "boarding", "available", "05:47:35");
    for (const char* s : {"catering", "refuelling", "boarding", "deboarding"}) gsx(t, s, "bypassed", "05:48:41");
    gsx(t, "pushback", "performing", "05:48:41");
    block(t, "block_out", "05:49:17");
    for (const char* s : {"catering", "refuelling", "boarding", "deboarding", "pushback"}) {
        gsx(t, s, "available", "05:52:25");
    }
    block(t, "block_in", "07:48:20");
    gsx(t, "deboarding", "requested", "07:49:32");
    gsx(t, "deboarding", "performing", "07:49:56");
    gsx(t, "deboarding", "completed", "07:52:47");
    gsx(t, "deboarding", "available", "07:53:00");

    EXPECT_TRUE(t.pushbackByGsx());
    ASSERT_EQ(t.departed().size(), 3u);
    EXPECT_EQ(t.departed()[0].service, "refuelling");
    EXPECT_EQ(t.departed()[0].startIso, at("05:34:35"));
    EXPECT_EQ(t.departed()[0].endIso, at("05:39:42"));
    EXPECT_EQ(t.departed()[1].service, "catering");
    EXPECT_EQ(t.departed()[1].startIso, at("05:35:33"));
    EXPECT_EQ(t.departed()[1].endIso, at("05:39:23"));
    EXPECT_EQ(t.departed()[2].service, "boarding");
    EXPECT_EQ(t.departed()[2].startIso, at("05:38:57"));
    EXPECT_EQ(t.departed()[2].endIso, at("05:47:24"));
    ASSERT_EQ(t.arrival().size(), 1u);
    EXPECT_EQ(t.arrival()[0].startIso, at("07:49:56"));
    EXPECT_EQ(t.arrival()[0].endIso, at("07:52:47"));
    EXPECT_TRUE(t.nextLeg().empty());
}
