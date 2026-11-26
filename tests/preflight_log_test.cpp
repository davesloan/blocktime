// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the origin's ATC and ATIS said since the flight was planned (D-188), and the Pre-Flight panel's departure and
// clearance views: SimBrief's runway and SID until ATC changes them (D-189).

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "event_log.h"
#include "preflight_log.h"

using namespace blocktime;
using nlohmann::json;

namespace {

struct Line {
    std::string type;
    json data;
};

std::vector<Line> ksanKsjc() {
    std::ifstream in(BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc/events-2026-11-25.jsonl", std::ios::binary);
    std::vector<Line> lines;
    for (std::string text; std::getline(in, text);) {
        const auto j = json::parse(text, nullptr, false);
        if (j.is_object()) lines.push_back({j.value("type", ""), j.value("data", json::object())});
    }
    return lines;
}

std::string fixtureText(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

// Feeds the fixture up to (not including) the first line of type `stop`; everything with "" .
void feed(PreFlightLog& log, const std::string& stop = "") {
    for (const Line& l : ksanKsjc()) {
        if (l.type == stop) return;
        log.onEvent(l.type, l.data);
    }
}

constexpr const char* kKsanClearance =
    "Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. Expect FL400 "
    "one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27";

json exchange(long long id, const std::string& stamp, const std::string& atc, const std::string& airport = "KSAN",
              const std::string& position = "CLEARANCE") {
    return json{{"flight_id", ""},
                {"entry",
                 {{"id", id},
                  {"channel", "COM1"},
                  {"ident", airport},
                  {"position", position},
                  {"stamp_zulu", stamp},
                  {"frequency", "125.9"},
                  {"outgoing_message", atc},
                  {"incoming_message", nullptr}}}};
}

json atis(const std::string& departing, const std::string& metar = "KSAN 251751Z 00000KT 10SM FEW200 19/05 A3009") {
    return json{{"flight_id", ""},  {"icao", "KSAN"},  {"role", "origin"}, {"letter", "L"},
                {"text", "INFO LIMA. WIND CALM. ALTIM 30.09."},    {"metar", metar},
                {"altimeter", "30.09"}, {"departing", departing}, {"arriving", departing}};
}

// N814SX's KSAN → KSJC plan: runway 27, PADRZ2 IKAYE, the takeoff report's 09 and 27.
PlannedFlightView plannedKsan() {
    PlannedFlightView v;
    v.origin = "KSAN";
    v.plannedAt = "2026-11-25T18:00:00Z";
    v.originRunway = "27";
    SimbriefPlan p;
    p.originRunway = "27";
    v.plan = p;
    v.brief = parsePlanBrief(fixtureText("ofp-ksan-ksjc-brief.json"));
    return v;
}

PreFlightLog logFor(const PlannedFlightView& v) {
    PreFlightLog log;
    log.setFlight(v.origin, v.plannedAt);
    return log;
}

}  // namespace

TEST(PreFlightLog, KsanKsjcFixture) {
    PreFlightLog log;
    log.setFlight("KSAN", "2026-11-25T18:00:00Z");
    // The older logs carry the clearance just after block out (polling started there); before liftoff it's all in.
    feed(log, "liftoff");
    ASSERT_TRUE(log.clearance().has_value());
    EXPECT_EQ(log.clearance()->sid, "PADRZ2");
    EXPECT_EQ(log.clearedAt(), parseUtc("2026-11-25T18:39:40Z"));  // ATC's words, not the readback's 18:39:45
    ASSERT_NE(log.atis(), nullptr);
    EXPECT_EQ(log.atis()->value("letter", ""), "L");
    EXPECT_TRUE(log.blockedOut());
    EXPECT_FALSE(log.airborne());
    ASSERT_TRUE(log.taxi().has_value());
    EXPECT_EQ(log.taxi()->route, "A, A7, B, B1");
    EXPECT_EQ(log.atcRunway(), "27");

    log.onEvent("liftoff", json{{"airport", "KSAN"}});
    EXPECT_TRUE(log.airborne());
}

TEST(PreFlightLog, IgnoresMessagesBeforePlanning) {
    PreFlightLog log;
    log.setFlight("KSAN", "2026-11-25T18:45:00Z");
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));
    EXPECT_FALSE(log.clearance().has_value());
}

TEST(PreFlightLog, OtherAirportIgnored) {
    PreFlightLog log;
    log.setFlight("KLAS", "2026-11-25T18:00:00Z");
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));
    log.onEvent("atis", atis("27"));
    EXPECT_FALSE(log.clearance().has_value());
    EXPECT_EQ(log.atis(), nullptr);
}

TEST(PreFlightLog, BlockInStartsOver) {
    PreFlightLog log;
    log.setFlight("KSAN", "2026-11-25T18:00:00Z");
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));
    log.onEvent("atis", atis("27"));
    log.onEvent("block_out", json::object());
    log.onEvent("liftoff", json::object());
    log.onEvent("block_in", json::object());
    EXPECT_FALSE(log.clearance().has_value());
    EXPECT_EQ(log.atis(), nullptr);
    EXPECT_FALSE(log.blockedOut());
    EXPECT_FALSE(log.airborne());
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));
    log.onEvent("flight_interrupted", json::object());
    EXPECT_FALSE(log.clearance().has_value());
}

TEST(PreFlightLog, CountdownFromAtcTime) {
    PreFlightLog log;
    log.setFlight("KSAN", "2026-11-25T18:00:00Z");
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));  // re-logged at block out
    EXPECT_EQ(log.clearedAt(), parseUtc("2026-11-25T18:39:40Z"));
    log.onEvent("atc_exchange",
                exchange(2, "2026-11-25T18:50:00Z", "Cleared to San Jose via the PADRZ2 departure. Squawk 4321."));
    EXPECT_EQ(log.clearedAt(), parseUtc("2026-11-25T18:50:00Z"));
    EXPECT_EQ(log.clearance()->squawk, "4321");
    // An older clearance arriving late doesn't replace the newer one.
    log.onEvent("atc_exchange", exchange(3, "2026-11-25T18:30:00Z", kKsanClearance));
    EXPECT_EQ(log.clearance()->squawk, "4321");
}

TEST(PreFlightLog, TaxiAfterLiftoffIgnored) {
    PreFlightLog log;
    log.setFlight("KSAN", "2026-11-25T18:00:00Z");
    log.onEvent("liftoff", json::object());
    log.onEvent("atc_exchange", exchange(4, "2026-11-25T18:55:00Z", "Runway 9 taxi via B.", "KSAN", "GROUND"));
    EXPECT_FALSE(log.taxi().has_value());
}

TEST(DepartureView, PlanThenAtis) {
    const PlannedFlightView v = plannedKsan();
    PreFlightLog log = logFor(v);
    DepartureView d = departureView(v, log);
    EXPECT_EQ(d.runway, "27");
    EXPECT_EQ(d.planRunway, "27");
    EXPECT_FALSE(d.runwayChanged);
    EXPECT_EQ(d.headingDeg, 275);
    EXPECT_EQ(d.condition, "Dry");
    EXPECT_EQ(d.wind, "");
    EXPECT_EQ(d.atisLetter, "");

    log.onEvent("atis", atis("27"));
    d = departureView(v, log);
    EXPECT_EQ(d.wind, "Calm");
    EXPECT_EQ(d.temperature, "19 °C");
    EXPECT_EQ(d.altimeter, "30.09 inHg");
    EXPECT_EQ(d.atisLetter, "L");

    log.onEvent("atis", atis("30L,30R"));
    d = departureView(v, log);
    EXPECT_EQ(d.runway, "30L");
    EXPECT_TRUE(d.runwayChanged);
    EXPECT_FALSE(d.headingDeg.has_value());  // not in the takeoff report
}

// Review finding: the plan's 30R is among the ATIS's "30L,30R", so it stays, unflagged (D-189).
TEST(DepartureView, ParallelRunwayInTheAtisKeepsThePlans) {
    PlannedFlightView v = plannedKsan();
    v.plan->originRunway = "30R";
    PreFlightLog log = logFor(v);
    log.onEvent("atis", atis("30L,30R"));
    const DepartureView d = departureView(v, log);
    EXPECT_EQ(d.runway, "30R");
    EXPECT_FALSE(d.runwayChanged);
}

TEST(DepartureView, AtcRunwayWins) {
    const PlannedFlightView v = plannedKsan();
    PreFlightLog log = logFor(v);
    log.onEvent("atis", atis("27"));
    log.onEvent("atc_exchange",
                exchange(1, "2026-11-25T18:39:40Z",
                         "Cleared to San Jose via the PADRZ2 departure. Climb via the SID. Squawk 3166. Expect runway 9"));
    const DepartureView d = departureView(v, log);
    EXPECT_EQ(d.runway, "09");
    EXPECT_TRUE(d.runwayChanged);
    EXPECT_EQ(d.headingDeg, 95);
}

TEST(DepartureView, WetWithRain) {
    const PlannedFlightView v = plannedKsan();
    PreFlightLog log = logFor(v);
    log.onEvent("atis", atis("27", "KSAN 251751Z 18010KT 5SM -RA OVC012 14/12 A2992"));
    EXPECT_EQ(departureView(v, log).condition, "Wet");
}

TEST(ClearanceView, PlanSidBeforeClearance) {
    const PlannedFlightView v = plannedKsan();
    const PreFlightLog log = logFor(v);
    const ClearanceView c = clearanceView(v, log);
    EXPECT_FALSE(c.received);
    EXPECT_EQ(c.sid, "PADRZ2 IKAYE");
    EXPECT_FALSE(c.sidChanged);
    EXPECT_EQ(c.squawk, "");

    PlannedFlightView noPlan = v;
    noPlan.plan.reset();
    noPlan.brief.reset();
    EXPECT_EQ(clearanceView(noPlan, log).sid, "");
}

TEST(ClearanceView, FullSid) {
    const PlannedFlightView v = plannedKsan();
    PreFlightLog log = logFor(v);
    log.onEvent("atc_exchange", exchange(1, "2026-11-25T18:39:40Z", kKsanClearance));
    log.onEvent("atc_exchange", exchange(2, "2026-11-25T18:45:51Z", "Runway 27 taxi via A, A7, B, B1.", "KSAN", "GROUND"));
    const ClearanceView c = clearanceView(v, log);
    EXPECT_TRUE(c.received);
    EXPECT_EQ(c.sid, "PADRZ2 IKAYE");
    EXPECT_FALSE(c.sidChanged);
    EXPECT_EQ(c.initialAltitude, "Climb via SID");
    EXPECT_EQ(c.squawk, "3166");
    EXPECT_EQ(c.departureFrequency, "119.6");
    EXPECT_EQ(c.taxiRoute, "A, A7, B, B1");
}

TEST(ClearanceView, NoSidInClearanceKeepsPlan) {
    const PlannedFlightView v = plannedKsan();
    PreFlightLog log = logFor(v);
    log.onEvent("atc_exchange",
                exchange(1, "2026-11-25T18:39:40Z", "Cleared to San Jose via radar vectors. Then as filed. Squawk 3166."));
    const ClearanceView c = clearanceView(v, log);
    EXPECT_TRUE(c.received);
    EXPECT_EQ(c.sid, "PADRZ2 IKAYE");
    EXPECT_FALSE(c.sidChanged);
}

TEST(ClearanceView, UnknownSidIsMarkedChanged) {
    const PlannedFlightView v = plannedKsan();
    PreFlightLog log = logFor(v);
    log.onEvent("atc_exchange",
                exchange(1, "2026-11-25T18:39:40Z", "Cleared to San Jose via the ZOOO4 departure. Squawk 3166."));
    const ClearanceView c = clearanceView(v, log);
    EXPECT_EQ(c.sid, "ZOOO4");
    EXPECT_TRUE(c.sidChanged);
}
