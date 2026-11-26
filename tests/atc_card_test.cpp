// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The ATC card (D-197): one slot per kind of instruction, each changed only when ATC changes it, LATEST on the slots
// the last transmission changed. Fed with Dave's KSAN → KSJC leg and made-up calls.

#include <ostream>

#include <gtest/gtest.h>

#include "atc_card.h"
#include "event_log.h"
#include "flight_events.h"

using namespace blocktime;

namespace blocktime {
void PrintTo(const AtcSlot& s, std::ostream* os) {
    *os << "{" << s.title << " '" << s.value << "' aside '" << s.aside << "' " << s.time << (s.latest ? " LATEST" : "")
        << "}";
}
}  // namespace blocktime

namespace {

const RoutePlan kPlan{"PADRZ2", "IKAYE"};
constexpr const char* kPlannedAt = "2026-11-25T18:00:00Z";

AtcMessage msg(long long id, const char* time, const char* words) {
    AtcMessage m;
    m.id = id;
    m.time = time;
    m.atc = words;
    return m;
}

const char* kClearance =
    "Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. Expect FL400 "
    "one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27";

AtcCard card() {
    AtcCard c;
    c.setFlight(kPlannedAt, kPlan);
    return c;
}

// The fixture's ATC, up to `until` (ATC's own time), and its touchdown if before then.
AtcCard fixtureUntil(const char* until) {
    static const auto lines =
        loadFlightEvents(BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc", "2026-11-25T18:43:04.053Z");
    AtcCard c = card();
    const auto cutoff = *parseUtc(until);
    for (const LogLine& l : lines) {
        if (l.type == "touchdown" && l.ts <= cutoff) c.onTouchdown(l.data.value("time", ""));
        if (l.type != "atc_exchange") continue;
        const auto m = atcMessage(l.data);
        if (!m || m->time.empty() || *parseUtc(m->time) > cutoff) continue;
        c.onMessage(*m);
    }
    return c;
}

AtcSlot slot(const char* title, const char* value, const char* aside, const char* time, bool latest) {
    return {title, value, aside, time, latest};
}

}  // namespace

TEST(AtcCard, KsanKsjcFixtureClimbing) {
    EXPECT_EQ(fixtureUntil("2026-11-25T19:00:30Z").slots(CardStage::Airborne),
              (std::vector<AtcSlot>{slot("ALTITUDE", "FL380", "", "2026-11-25T19:00:08Z", true),
                                    slot("ROUTE", "PADRZ2 IKAYE", "", "2026-11-25T18:39:40Z", false),
                                    slot("SPEED", "", "", "", false), slot("APPROACH", "", "", "", false)}));
}

TEST(AtcCard, KsanKsjcFixtureThreeInOneCall) {
    EXPECT_EQ(fixtureUntil("2026-11-25T19:50:30Z").slots(CardStage::Airborne),
              (std::vector<AtcSlot>{slot("ALTITUDE", "2,700", "UNTIL ESTABLISHED", "2026-11-25T19:50:26Z", true),
                                    slot("HEADING", "320", "LEFT", "2026-11-25T19:50:26Z", true),
                                    slot("SPEED", "", "", "", false),
                                    slot("APPROACH", "CLEARED ILS 30L", "", "2026-11-25T19:50:26Z", true)}));
}

TEST(AtcCard, KsanKsjcFixtureTaxiIn) {
    EXPECT_EQ(fixtureUntil("2026-11-25T20:01:00Z").slots(CardStage::Arrival),
              (std::vector<AtcSlot>{slot("TAXI", "K", "TERMINAL A GATE 4", "2026-11-25T19:59:57Z", false),
                                    slot("HOLD SHORT", "CROSS 30R", "", "2026-11-25T20:00:03Z", true)}));
}

TEST(AtcCard, DepartureSlots) {
    EXPECT_EQ(fixtureUntil("2026-11-25T18:46:00Z").slots(CardStage::Departure),
              (std::vector<AtcSlot>{slot("ALTITUDE", "CLIMB VIA SID", "EXP FL400 +10 MIN", "2026-11-25T18:39:40Z", false),
                                    slot("ROUTE", "PADRZ2 IKAYE", "", "2026-11-25T18:39:40Z", false),
                                    slot("TAXI", "A, A7, B, B1", "RWY 27", "2026-11-25T18:45:51Z", true)}));
}

TEST(AtcCard, BeforeTheFirstAltitudeClearance) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T18:39:40Z", kClearance));
    EXPECT_EQ(c.slots(CardStage::Airborne)[0], slot("ALTITUDE", "CLIMB VIA SID", "EXP FL400 +10 MIN", "2026-11-25T18:39:40Z", true));
    c.onMessage(msg(2, "2026-11-25T18:57:56Z", "Climb and maintain FL220"));
    EXPECT_EQ(c.slots(CardStage::Airborne)[0], slot("ALTITUDE", "FL220", "", "2026-11-25T18:57:56Z", true));
    EXPECT_FALSE(c.slots(CardStage::Airborne)[1].latest);  // ROUTE wasn't in the last call
}

TEST(AtcCard, RepeatedExchangeChangesNothing) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T18:39:40Z", kClearance));
    c.onMessage(msg(2, "2026-11-25T18:57:56Z", "Climb and maintain FL220"));
    const auto before = c.slots(CardStage::Airborne);
    c.onMessage(msg(1, "2026-11-25T18:39:40Z", kClearance));  // logged again at block out
    EXPECT_EQ(c.slots(CardStage::Airborne), before);
}

TEST(AtcCard, OutOfOrderMessagesApplyInAtcTime) {
    AtcCard c = card();
    c.onMessage(msg(2, "2026-11-25T18:57:56Z", "Climb and maintain FL220"));
    c.onMessage(msg(1, "2026-11-25T18:39:40Z", kClearance));
    EXPECT_EQ(c.slots(CardStage::Airborne)[0].value, "FL220");
}

TEST(AtcCard, UnreadCallKeepsLatest) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T19:50:26Z",
                    "Turn left heading 320. Descend and maintain 2,700 until established on the localizer. Cleared for "
                    "the ILS runway 30L approach."));
    c.onMessage(msg(2, "2026-11-25T19:51:16Z", "Traffic, 9 o'clock, 5 miles, Airbus, 4,000."));
    const auto s = c.slots(CardStage::Airborne);
    EXPECT_TRUE(s[0].latest);
    EXPECT_TRUE(s[1].latest);
    EXPECT_TRUE(s[3].latest);
}

TEST(AtcCard, PreviousLegIgnored) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T17:10:00Z", "Climb and maintain FL350"));  // before this flight was planned
    EXPECT_EQ(c.slots(CardStage::Airborne)[0].value, "");
}

TEST(AtcCard, MessageWithoutTimeIsDropped) {
    AtcCard c = card();
    c.onMessage(msg(1, "", "Climb and maintain FL350"));
    EXPECT_EQ(c.slots(CardStage::Airborne)[0].value, "");
}

TEST(AtcCard, GoAroundClearsApproach) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T19:50:26Z", "Cleared for the ILS runway 30L approach."));
    c.onGoAround("2026-11-25T19:57:30Z");
    EXPECT_EQ(c.slots(CardStage::Airborne)[3], slot("APPROACH", "", "", "", false));
    c.onMessage(msg(2, "2026-11-25T19:58:10Z", "Climb and maintain 3,000"));
    EXPECT_EQ(c.slots(CardStage::Airborne)[3].value, "");
}

TEST(AtcCard, TouchdownClearsTheTaxiOut) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T18:45:51Z", "Runway 27 taxi via A, A7, B, B1."));
    c.onTouchdown("2026-11-25T19:58:59Z");
    EXPECT_EQ(c.slots(CardStage::Arrival)[0], slot("TAXI", "", "", "", false));
}

TEST(AtcCard, NewPlanStartsOver) {
    AtcCard c = card();
    c.onMessage(msg(1, "2026-11-25T18:57:56Z", "Climb and maintain FL220"));
    c.setFlight("2026-11-25T20:30:00Z", kPlan);
    EXPECT_EQ(c.slots(CardStage::Airborne)[0].value, "");
}

// What the arrival and short-final readouts take from the card (D-194).
TEST(AtcCard, ArrivalFacts) {
    const AtcCard c = fixtureUntil("2026-11-25T20:01:00Z");
    EXPECT_EQ(c.star(), "SILCN6 TROXX");
    EXPECT_EQ(c.approachRunway(), "30L");
    EXPECT_EQ(c.landingWind(), "360° / 4 kt");
    const AtcCard early = fixtureUntil("2026-11-25T19:00:00Z");
    EXPECT_EQ(early.star(), "");
    EXPECT_EQ(early.approachRunway(), "");
    EXPECT_EQ(early.landingWind(), "");
}
