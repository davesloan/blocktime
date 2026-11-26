// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// D-168's plain form for an ATC exchange, read from the KSAN → KSJC fixture's logged lines.

#include <fstream>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "atc_message.h"

using namespace blocktime;

namespace {

// The data of the fixture's atc_exchange line whose entry has this id.
nlohmann::json exchange(long long id) {
    std::ifstream in(BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc/events-2026-11-25.jsonl", std::ios::binary);
    std::string text;
    while (std::getline(in, text)) {
        const auto j = nlohmann::json::parse(text, nullptr, false);
        if (!j.is_object() || j.value("type", "") != "atc_exchange") continue;
        const auto& e = j["data"]["entry"];
        if (e.contains("id") && e["id"].is_number() && e["id"].get<long long>() == id) return j["data"];
    }
    ADD_FAILURE() << "no exchange " << id;
    return nlohmann::json::object();
}

}  // namespace

TEST(AtcMessage, ReadsTheClearanceEntry) {
    const auto m = atcMessage(exchange(110637058));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->id, 110637058);
    EXPECT_EQ(m->time, "2026-11-25T18:39:40Z");
    EXPECT_EQ(m->airport, "KSAN");
    EXPECT_EQ(m->position, "CLEARANCE");
    EXPECT_EQ(m->station, "Lindbergh Clearance");
    EXPECT_EQ(m->frequency, "125.9");
    EXPECT_EQ(m->atc.rfind("Cleared to Norman Y. Mineta San Jose", 0), 0u);
    EXPECT_EQ(m->pilot, "Ready to copy our IFR clearance.");
}

TEST(AtcMessage, NullWordsAreEmpty) {
    const auto m = atcMessage(exchange(110637996));
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->atc, "");
    EXPECT_EQ(m->pilot, "Push and start approved. Tail west.");
}

TEST(AtcMessage, IntercomIsNotAtc) {
    auto data = exchange(110637058);
    data["entry"]["channel"] = "INTERCOM";
    EXPECT_FALSE(atcMessage(data).has_value());
    EXPECT_FALSE(atcMessage(nlohmann::json{{"flight_id", ""}}).has_value());
    EXPECT_FALSE(atcMessage(nlohmann::json{{"entry", "text"}}).has_value());
}
