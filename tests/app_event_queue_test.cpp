// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The app's web results reach the event log through the recorder thread (D-127).

#include <thread>

#include <gtest/gtest.h>

#include "app_event_queue.h"

using namespace blocktime;

TEST(AppEventQueue, DrainsInOrderAndEmpties) {
    AppEventQueue q;
    q.push("atis", {{"icao", "KBFI"}});
    q.push("atc_exchange", {{"id", 1}});
    const auto events = q.drain();
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].type, "atis");
    EXPECT_EQ(events[0].data["icao"], "KBFI");
    EXPECT_EQ(events[1].type, "atc_exchange");
    EXPECT_TRUE(q.drain().empty());
}

TEST(AppEventQueue, PushFromAnotherThread) {
    AppEventQueue q;
    std::thread t([&q] {
        for (int i = 0; i < 1000; ++i) q.push("atc_exchange", {{"id", i}});
    });
    std::size_t seen = 0;
    while (seen < 1000) seen += q.drain().size();
    t.join();
    EXPECT_EQ(seen, 1000u);
}
