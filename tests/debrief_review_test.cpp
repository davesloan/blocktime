// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Claude's review (D-156, D-161): the request built from a flight, and its answer read back.

#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "debrief_measure.h"
#include "debrief_review.h"
#include "event_log.h"
#include "flight_events.h"

using namespace blocktime;

namespace {

const std::vector<LogLine>& fixture() {
    static const std::vector<LogLine> lines =
        loadFlightEvents(BLOCKTIME_SOURCE_DIR "/tests/data/flight-klas-ksan", "2026-11-24T23:53:16.837Z");
    return lines;
}

ReviewInput input() {
    ReviewInput in;
    in.header = "KLAS → KSAN · SOX8 · N814SX (PMDG 737-800BBJ2) · out 23:53Z in 00:52Z";
    in.events = fixture();
    // A cabin crew message, which the review must not see (INTERCOM channels).
    in.events.push_back({*parseUtc("2026-11-25T00:30:00.000Z"), "atc_exchange",
                         nlohmann::json::parse(R"({"entry":{"id":1,"channel":"INTERCOM2","station_name":"Cabin Crew",
                             "incoming_message":"","outgoing_message":"Dear Guests, snacks are served.",
                             "stamp_zulu":"2026-11-25T00:30:00Z"}})")});
    in.measurements = measureFlight(in.events, false);
    in.recentMisses = {{"Beacon on before pushback"}, {}};
    return in;
}

}  // namespace

TEST(DebriefReview, TimelineOfTheFixture) {
    const std::string t = buildTimeline(fixture());
    EXPECT_NE(t.find("touchdown"), std::string::npos);
    EXPECT_NE(t.find("00:49:11 autopilot off"), std::string::npos) << t.substr(0, 2000);
    EXPECT_NE(t.find("seat belt sign on"), std::string::npos);
    EXPECT_NE(t.find("beacon on"), std::string::npos);
    EXPECT_NE(t.find("at the 1,000 ft marker"), std::string::npos);
    EXPECT_EQ(t.find("telemetry"), std::string::npos);
    EXPECT_LT(t.size(), 20000u);  // the telemetry is condensed, not copied
}

TEST(DebriefReview, RequestBody) {
    const auto body = nlohmann::json::parse(buildReviewRequest(input()));
    EXPECT_EQ(body["model"], "claude-opus-5-5");
    EXPECT_EQ(body["max_tokens"], 16000);
    EXPECT_EQ(body["thinking"]["type"], "adaptive");
    EXPECT_EQ(body["output_config"]["effort"], "medium");
    EXPECT_EQ(body["output_config"]["format"]["type"], "json_schema");
    const auto& schema = body["output_config"]["format"]["schema"];
    EXPECT_EQ(schema["additionalProperties"], false);
    EXPECT_EQ(schema["required"], nlohmann::json::array({"summary", "work_on", "landing", "atc"}));
    EXPECT_EQ(body["fallbacks"], "default");
    EXPECT_FALSE(body.contains("temperature"));
    EXPECT_TRUE(body["system"].is_string());
    const std::string user = body["messages"][0]["content"].get<std::string>();
    EXPECT_EQ(body["messages"][0]["role"], "user");
    EXPECT_NE(user.find("KLAS → KSAN"), std::string::npos);
    EXPECT_NE(user.find("Beacon on before pushback: missed"), std::string::npos);
    EXPECT_NE(user.find("SoCal Approach: Turn left heading 115, maintain 5300"), std::string::npos);
    EXPECT_NE(user.find("Pilot: Left heading 115"), std::string::npos);
    EXPECT_EQ(user.find("Cabin Crew"), std::string::npos);
    EXPECT_EQ(user.find("snacks"), std::string::npos);
    EXPECT_NE(user.find("INFO QUEBEC"), std::string::npos);
    EXPECT_NE(user.find("1 flight ago"), std::string::npos);
}

TEST(DebriefReview, ParsesAnAnswer) {
    const std::string body = R"({"id":"msg_1","model":"claude-opus-5-5","stop_reason":"end_turn",
        "content":[{"type":"thinking","thinking":""},
                   {"type":"text","text":"{\"summary\":\"A tidy flight.\",\"work_on\":[\"Beacon first\",\"Lights at 10,000\"],\"landing\":\"Firm.\",\"atc\":\"Levelled as cleared.\"}"}]})";
    const ReviewAnswer a = parseReviewResponse(200, body);
    ASSERT_TRUE(a.review) << a.error;
    EXPECT_EQ(a.review->summary, "A tidy flight.");
    EXPECT_EQ(a.review->workOn, (std::vector<std::string>{"Beacon first", "Lights at 10,000"}));
    EXPECT_EQ(a.review->landing, "Firm.");
    EXPECT_EQ(a.review->atc, "Levelled as cleared.");
    EXPECT_EQ(a.review->model, "claude-opus-5-5");
}

TEST(DebriefReview, ParsesEachFailure) {
    EXPECT_EQ(parseReviewResponse(401, R"({"type":"error","error":{"type":"authentication_error","message":"invalid x-api-key"}})").error,
              "Claude: invalid x-api-key");
    EXPECT_EQ(parseReviewResponse(529, R"({"type":"error","error":{"type":"overloaded_error","message":"Overloaded"}})").error,
              "Claude: Overloaded");
    EXPECT_EQ(parseReviewResponse(502, "<html>bad gateway</html>").error, "Claude answered HTTP 502");
    EXPECT_EQ(parseReviewResponse(200, R"({"model":"claude-opus-5-5","stop_reason":"refusal","content":[]})").error,
              "Claude declined to write it");
    EXPECT_EQ(parseReviewResponse(200, R"({"model":"claude-opus-5-5","stop_reason":"max_tokens",
        "content":[{"type":"text","text":"{\"summ"}]})").error, "Claude's answer was cut off");
    EXPECT_EQ(parseReviewResponse(200, R"({"model":"claude-opus-5-5","stop_reason":"end_turn",
        "content":[{"type":"text","text":"Sure! Here is your debrief."}]})").error, "Claude's answer wasn't readable");
    EXPECT_EQ(parseReviewResponse(200, R"({"model":"claude-opus-5-5","stop_reason":"end_turn",
        "content":[{"type":"text","text":"{\"summary\":\"x\"}"}]})").error, "Claude's answer wasn't readable");
    EXPECT_EQ(parseReviewResponse(200, "not json").error, "Claude's answer wasn't readable");
    EXPECT_FALSE(parseReviewResponse(401, "{}").review);
}

TEST(DebriefReview, TimelineStartsWithThisFlight) {
    // The lead-in before block out reaches back to the previous leg's landing: only its state at the start shows.
    const std::string t = buildTimeline(fixture());
    EXPECT_EQ(t.rfind("23:53:16 at the start: engines off, beacon off, strobes off, seat belt sign off, landing lights "
                      "off, gear down",
                      0),
              0u)
        << t.substr(0, 300);
    EXPECT_EQ(t.find("23:2"), std::string::npos);
    EXPECT_EQ(t.find("23:4"), std::string::npos);
    EXPECT_NE(t.find("no bounce after touchdown"), std::string::npos);
}

TEST(DebriefReview, NullFieldsFromSayIntentionsDontThrow) {
    // SayIntentions sends null for fields it has no value for (seen for outgoing_message and the *_english ones).
    ReviewInput in = input();
    in.events.push_back({*parseUtc("2026-11-25T00:31:00.000Z"), "atc_exchange",
                         nlohmann::json::parse(R"({"entry":{"id":2,"channel":null,"station_name":null,
                             "incoming_message":null,"outgoing_message":"Say again?","stamp_zulu":null}})")});
    in.events.push_back({*parseUtc("2026-11-25T00:31:05.000Z"), "atc_exchange",
                         nlohmann::json::parse(R"({"entry":{"id":3,"channel":"COM1","station_name":null,
                             "incoming_message":null,"outgoing_message":"Descend FL240.","stamp_zulu":null}})")});
    in.events.push_back({*parseUtc("2026-11-25T00:31:10.000Z"), "atis",
                         nlohmann::json::parse(R"({"icao":null,"letter":null,"role":"destination","text":null})")});
    std::string body;
    EXPECT_NO_THROW(body = buildReviewRequest(in));
    const std::string user = nlohmann::json::parse(body)["messages"][0]["content"].get<std::string>();
    EXPECT_EQ(user.find("Say again?"), std::string::npos);    // no channel: not known to be ATC
    EXPECT_NE(user.find("ATC: Descend FL240."), std::string::npos);
    ReviewAnswer a;
    EXPECT_NO_THROW(a = parseReviewResponse(200, R"({"model":null,"stop_reason":null,"content":[{"type":null},
        {"type":"text","text":"{\"summary\":\"x\",\"work_on\":[],\"landing\":\"\",\"atc\":\"\"}"}]})"));
    ASSERT_TRUE(a.review);
    EXPECT_EQ(a.review->model, "claude-opus-5-5");
}

// Review 1.11: an exchange logged again once ATC's reply came in is shown once, with the reply.
TEST(DebriefReview, ReloggedExchangeKeepsTheLastCopy) {
    ReviewInput in = input();
    in.events.push_back({*parseUtc("2026-11-25T00:31:00.000Z"), "atc_exchange",
                         nlohmann::json::parse(R"({"entry":{"id":7,"channel":"COM1","station_name":"SoCal Approach",
                             "incoming_message":"Request lower.","outgoing_message":"","stamp_zulu":"2026-11-25T00:31:00Z"}})")});
    in.events.push_back({*parseUtc("2026-11-25T00:31:06.000Z"), "atc_exchange",
                         nlohmann::json::parse(R"({"entry":{"id":7,"channel":"COM1","station_name":"SoCal Approach",
                             "incoming_message":"Request lower.","outgoing_message":"Descend and maintain 8,000.",
                             "stamp_zulu":"2026-11-25T00:31:00Z"}})")});
    const std::string user =
        nlohmann::json::parse(buildReviewRequest(in))["messages"][0]["content"].get<std::string>();
    const auto pilot = user.find("Pilot: Request lower.");
    ASSERT_NE(pilot, std::string::npos);
    EXPECT_EQ(user.find("Pilot: Request lower.", pilot + 1), std::string::npos);  // once
    EXPECT_NE(user.find("SoCal Approach: Descend and maintain 8,000."), std::string::npos);
}

TEST(DebriefReview, TimelineGivesHeightsAndSpeedSpells) {
    // From the first real review: without these, Claude guessed the autopilot's height and put the 260 kt spell in
    // the climb.
    const std::string t = buildTimeline(fixture());
    const auto ap = t.find("00:49:11 autopilot off at ");
    ASSERT_NE(ap, std::string::npos) << t;
    EXPECT_NE(t.substr(ap, 60).find("ft above the ground"), std::string::npos) << t.substr(ap, 60);
    EXPECT_NE(t.find("landing lights on at "), std::string::npos);
    const auto spell = t.find("above 260 kt below 10,000 ft for 1");
    ASSERT_NE(spell, std::string::npos) << t;
    EXPECT_EQ(t.substr(spell - 9, 5), "00:39") << t.substr(spell - 9, 80);
    EXPECT_NE(t.substr(spell, 100).find("descending"), std::string::npos) << t.substr(spell, 100);
}

TEST(DebriefReview, TimelineGivesFlapsInDegrees) {
    // The sim test's review read "flaps handle 8" as short of flaps 40: the handle's eighth detent is flaps 40.
    const std::string t = buildTimeline(fixture());
    EXPECT_NE(t.find("at the 1,000 ft marker: 125 kt, sinking 791 fpm, flaps 40"), std::string::npos) << t;
    EXPECT_EQ(t.find("flaps handle"), std::string::npos);
}

TEST(DebriefReview, InstructionsCarryDavesCorrections) {
    // From Dave's notes on the sim test's review (2026-11-25): these must stay in the instructions.
    const auto system = nlohmann::json::parse(buildReviewRequest(input()))["system"].get<std::string>();
    EXPECT_NE(system.find("ATC compliance never appears in summary or work_on"), std::string::npos);
    EXPECT_NE(system.find("published altitude constraint"), std::string::npos);
    EXPECT_NE(system.find("Don't comment on radio frequencies or phraseology"), std::string::npos);
    EXPECT_NE(system.find("touching down up to about 10 kt below VREF is normal"), std::string::npos);
    EXPECT_NE(system.find("never call a passed check a fault"), std::string::npos);
}

TEST(DebriefReview, TimelineShowsTheApu) {
    const std::string t = buildTimeline(fixture());
    EXPECT_NE(t.find("23:58:23 APU switched off"), std::string::npos) << t.substr(0, 1500);
    EXPECT_NE(t.find("00:46:41 APU switched on"), std::string::npos);
    EXPECT_NE(t.find("00:47:07 APU running"), std::string::npos);
}
