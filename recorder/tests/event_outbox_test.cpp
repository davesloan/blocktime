// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "event_outbox.h"

using namespace blocktime;

namespace {

// Stands in for EventLog::write: records the types it wrote, and fails while `failing` is set.
// `failAfter` lets that many writes succeed before it starts failing (-1 = never).
struct FakeLog {
    bool failing = false;
    int failAfter = -1;
    std::vector<std::string> written;

    EventOutbox::WriteFn fn() {
        return [this](std::string_view type, const nlohmann::json&) {
            if (failAfter == 0) failing = true;
            if (failAfter > 0) --failAfter;
            if (failing) return false;
            written.emplace_back(type);
            return true;
        };
    }
};

}  // namespace

TEST(EventOutbox, WritesStraightThroughWhenLogWorks) {
    FakeLog log;
    EventOutbox box(log.fn());
    EXPECT_TRUE(box.send("block_out", {{"flight_id", "A"}}));
    EXPECT_EQ(box.pending(), 0u);
    EXPECT_EQ(log.written, std::vector<std::string>{"block_out"});
}

TEST(EventOutbox, HoldsEventWhenWriteFails) {
    FakeLog log;
    log.failing = true;
    EventOutbox box(log.fn());
    EXPECT_FALSE(box.send("block_out", {{"flight_id", "A"}}));
    EXPECT_EQ(box.pending(), 1u);
    EXPECT_TRUE(log.written.empty());
}

TEST(EventOutbox, FlushWritesHeldEventsInOrder) {
    FakeLog log;
    log.failing = true;
    EventOutbox box(log.fn());
    box.send("block_out", {});
    box.send("block_in", {});
    log.failing = false;
    EXPECT_TRUE(box.flush());
    EXPECT_EQ(box.pending(), 0u);
    EXPECT_EQ(log.written, (std::vector<std::string>{"block_out", "block_in"}));
}

TEST(EventOutbox, NewEventWaitsBehindHeldOnes) {
    FakeLog log;
    log.failing = true;
    EventOutbox box(log.fn());
    box.send("block_out", {});
    log.failing = false;
    EXPECT_TRUE(box.send("block_in", {}));
    EXPECT_EQ(log.written, (std::vector<std::string>{"block_out", "block_in"}));
}

TEST(EventOutbox, FlushStopsAtFirstFailure) {
    FakeLog log;
    log.failing = true;
    EventOutbox box(log.fn());
    box.send("block_out", {{"flight_id", "A"}});
    box.send("block_in", {{"flight_id", "A"}});
    log.failing = false;
    log.failAfter = 1;  // one write succeeds, then the log fails again
    EXPECT_FALSE(box.flush());
    EXPECT_EQ(log.written, std::vector<std::string>{"block_out"});
    ASSERT_EQ(box.pending(), 1u);
    const auto held = box.held();
    EXPECT_EQ(held.at(0).first, "block_in");
    EXPECT_EQ(held.at(0).second, (nlohmann::json{{"flight_id", "A"}}));
}
