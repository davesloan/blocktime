// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The ATC log and ATIS de-duplication (D-123, D-127).

#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "atc_log.h"
#include "sayintentions.h"

using namespace blocktime;

namespace {

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

CommsHistory history() {
    auto h = parseCommsHistory(fixture("comms_history.json"));
    EXPECT_TRUE(h.has_value());
    return h.value_or(CommsHistory{});
}

Atis atis(const std::string& icao, const std::string& letter, const std::string& text) {
    Atis a;
    a.icao = icao;
    a.letter = letter;
    a.text = text;
    return a;
}

}  // namespace

TEST(CommsHistory, ParsesTheDocumentedShape) {
    const CommsHistory h = history();
    EXPECT_EQ(h.flightId, "987654321");
    ASSERT_EQ(h.entries.size(), 2u);
    EXPECT_EQ(h.entries[0]["station_name"], "Boeing Ground");
    EXPECT_EQ(h.entries[1]["id"], 12346);
}

TEST(CommsHistory, ErrorIsNothing) {
    EXPECT_FALSE(parseCommsHistory(R"({"error":"No active flight could be found."})").has_value());
    EXPECT_FALSE(parseCommsHistory("[]").has_value());
    EXPECT_FALSE(parseCommsHistory("").has_value());
}

TEST(CommsDedupe, LogsEachExchangeOnce) {
    CommsDedupe d;
    EXPECT_EQ(d.toLog(history()).size(), 2u);
    EXPECT_TRUE(d.toLog(history()).empty());
}

TEST(CommsDedupe, RelogsAnExchangeThatChanged) {
    CommsDedupe d;
    CommsHistory h = history();
    d.toLog(h);
    h.entries[1]["outgoing_message"] = "Xly 8, wind 160 at 8, runway 14R, cleared for takeoff.";
    const auto again = d.toLog(h);
    ASSERT_EQ(again.size(), 1u);
    EXPECT_EQ(again[0]["id"], 12346);
    EXPECT_EQ(again[0]["outgoing_message"], "Xly 8, wind 160 at 8, runway 14R, cleared for takeoff.");
    EXPECT_TRUE(d.toLog(h).empty());
}

TEST(CommsDedupe, NewFlightStartsOver) {
    CommsDedupe d;
    CommsHistory h = history();
    d.toLog(h);
    h.flightId = "111";
    EXPECT_EQ(d.toLog(h).size(), 2u);
}

TEST(CommsDedupe, EntryWithoutAnIdIsKeyedByItsText) {
    CommsDedupe d;
    CommsHistory h;
    h.flightId = "1";
    h.entries.push_back({{"stamp_zulu", "2026-11-23T22:47:10Z"}, {"frequency", "121.900"}, {"incoming_message", "a"}});
    h.entries.push_back({{"stamp_zulu", "2026-11-23T22:48:10Z"}, {"frequency", "121.900"}, {"incoming_message", "b"}});
    EXPECT_EQ(d.toLog(h).size(), 2u);
    EXPECT_TRUE(d.toLog(h).empty());
}

TEST(CommsDedupe, ResetForgets) {
    CommsDedupe d;
    d.toLog(history());
    d.reset();
    EXPECT_EQ(d.toLog(history()).size(), 2u);
}

TEST(AtisDedupe, OnlyChanges) {
    AtisDedupe d;
    EXPECT_TRUE(d.isNew(atis("KBFI", "K", "Kilo")));
    EXPECT_FALSE(d.isNew(atis("KBFI", "K", "Kilo")));
    EXPECT_TRUE(d.isNew(atis("KBFI", "L", "Lima")));
    EXPECT_TRUE(d.isNew(atis("KPDX", "L", "Lima")));  // another airport, tracked apart
    EXPECT_TRUE(d.isNew(atis("KBFI", "L", "Lima, amended")));
    d.reset();
    EXPECT_TRUE(d.isNew(atis("KBFI", "L", "Lima, amended")));
}
