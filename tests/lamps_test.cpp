// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The in-flight panel's lamps (D-195): lit when the switch is on, flashing while it is wrong by the debrief's windows
// (docs/dashboard.md, "Lamps"), dark and never flashing without data.

#include <filesystem>
#include <set>

#include <gtest/gtest.h>

#include "event_log.h"
#include "flight_events.h"
#include "lamps.h"
#include "telemetry_json.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

const TimePoint t0 = *parseUtc("2026-11-25T18:00:00Z");

nlohmann::json cockpit(int belts, bool landing, bool gear = true, bool pmdg = true) {
    return {{"pmdg", pmdg}, {"seat_belts", belts}, {"landing_lights", landing}, {"gear_down", gear}};
}
nlohmann::json altimeter(double mb) { return {{"capt_mb", mb}, {"fo_mb", mb}}; }

// A board for a 737 with the engines running, the beacon and strobes as given, cockpit and altimeter set right for
// the gate: belts on, lights off, gear down, 1019 hPa.
struct Board {
    LampBoard b;
    Board(Family f = Family::B737) {
        b.reset(f);
        sample(t0, true, false, true);
        event(t0, "cockpit", cockpit(2, false));
        event(t0, "altimeter", altimeter(1019));
    }
    void sample(TimePoint at, bool beacon, bool strobe, bool engines = true) {
        TelemetrySample s{};
        s.numEngines = 2;
        s.engCombustion[0] = s.engCombustion[1] = engines ? 1 : 0;
        s.lightBeacon = beacon ? 1 : 0;
        s.lightStrobe = strobe ? 1 : 0;
        b.onSample(s, at);
    }
    void event(TimePoint at, std::string_view type, nlohmann::json data) { b.onEvent(type, data, at); }
    void phase(TimePoint at, const char* p) { event(at, "phase_changed", {{"phase", p}}); }
    void marker(TimePoint at, const char* m) { event(at, "flight_marker", {{"marker", m}}); }
    LampState lamp(Lamp l, TimePoint now) const { return b.lamps(now)[static_cast<int>(l)]; }
    bool wrong(Lamp l, TimePoint now) const { return lamp(l, now).wrong; }
};

}  // namespace

TEST(LampBoard, NothingIsWrongBeforeBlockOut) {
    Board r;
    r.sample(t0 + 1min, false, false);  // engines running, beacon off, at the gate
    for (const LampState& s : r.b.lamps(t0 + 5min)) EXPECT_FALSE(s.wrong) << static_cast<int>(s.lamp);
}

TEST(LampBoard, BeaconFlashesTenSecondsAfterEngineStart) {
    LampBoard b;
    b.reset(Family::B737);
    TelemetrySample s{};
    s.numEngines = 2;
    b.onSample(s, t0);                // engines off
    s.engCombustion[0] = 1;
    b.onSample(s, t0 + 1min);         // first start, 30 s before block out, beacon off
    b.onBlockOut(t0 + 90s);
    EXPECT_FALSE(b.lamps(t0 + 1min + 9s)[0].wrong);
    EXPECT_TRUE(b.lamps(t0 + 1min + 11s)[0].wrong);
    EXPECT_TRUE(b.lamps(t0 + 2min)[0].wrong);
    s.lightBeacon = 1;
    b.onSample(s, t0 + 2min);
    EXPECT_FALSE(b.lamps(t0 + 2min)[0].wrong);
    EXPECT_TRUE(b.lamps(t0 + 2min)[0].lit);
}

TEST(LampBoard, BeltsOnUntilTransitionAltitude) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.phase(t0 + 3min, "taxi_out");
    r.marker(t0 + 20min, "above_10000");
    r.event(t0 + 21min, "cockpit", cockpit(0, false));
    EXPECT_TRUE(r.wrong(Lamp::SeatBelts, t0 + 21min));  // off past 10,000 ft, below the transition altitude
    r.marker(t0 + 24min, "transition_altitude");
    EXPECT_FALSE(r.wrong(Lamp::SeatBelts, t0 + 24min + 1s));
    EXPECT_FALSE(r.wrong(Lamp::SeatBelts, t0 + 50min));  // cruise: Dave's choice
}

TEST(LampBoard, BeltsOffAfterTransitionLevel) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.phase(t0 + 3min, "taxi_out");
    r.marker(t0 + 24min, "transition_altitude");
    r.event(t0 + 30min, "cockpit", cockpit(0, false));
    r.marker(t0 + 60min, "transition_level");
    EXPECT_FALSE(r.wrong(Lamp::SeatBelts, t0 + 59min));
    EXPECT_TRUE(r.wrong(Lamp::SeatBelts, t0 + 60min + 1s));
    r.event(t0 + 61min, "cockpit", cockpit(1, false));  // AUTO counts as on
    EXPECT_FALSE(r.wrong(Lamp::SeatBelts, t0 + 61min));
}

TEST(LampBoard, LowFlightClosesBeltsWindowAtDescent) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.phase(t0 + 3min, "taxi_out");
    r.event(t0 + 15min, "cockpit", cockpit(0, false));
    EXPECT_TRUE(r.wrong(Lamp::SeatBelts, t0 + 16min));  // never reached the transition altitude: still open
    r.phase(t0 + 20min, "descent");
    EXPECT_FALSE(r.wrong(Lamp::SeatBelts, t0 + 21min));
}

TEST(LampBoard, NoPmdgDataNeverFlashes) {
    Board r;
    r.b.reset(Family::B737);  // a flight that never had PMDG's data (the debrief counts it once seen in the flight)
    r.event(t0, "cockpit", cockpit(0, false, true, false));  // the plain SimVars only: 737 belts and lights unread
    LampBoard& b = r.b;
    b.onBlockOut(t0 + 1min);
    r.phase(t0 + 3min, "taxi_out");
    r.phase(t0 + 10min, "takeoff");
    for (const TimePoint at : {t0 + 5min, t0 + 10min + 30s}) {
        EXPECT_FALSE(r.lamp(Lamp::SeatBelts, at).known);
        EXPECT_FALSE(r.lamp(Lamp::SeatBelts, at).wrong);
        EXPECT_FALSE(r.lamp(Lamp::LandingLights, at).known);
        EXPECT_FALSE(r.lamp(Lamp::LandingLights, at).wrong);
    }
}

TEST(LampBoard, TheSevenFiveSevenSevenNeedsNoPmdgDataForBelts) {
    Board r(Family::B777);
    r.event(t0, "cockpit", cockpit(0, false, true, false));
    r.b.onBlockOut(t0 + 1min);
    r.phase(t0 + 3min, "taxi_out");
    EXPECT_TRUE(r.lamp(Lamp::SeatBelts, t0 + 4min).known);
    EXPECT_TRUE(r.wrong(Lamp::SeatBelts, t0 + 4min));
}

TEST(LampBoard, TakeoffLightsGrace) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.phase(t0 + 3min, "taxi_out");
    r.phase(t0 + 10min, "takeoff");
    EXPECT_FALSE(r.wrong(Lamp::Strobes, t0 + 10min + 9s));
    EXPECT_TRUE(r.wrong(Lamp::Strobes, t0 + 10min + 11s));
    EXPECT_TRUE(r.wrong(Lamp::LandingLights, t0 + 10min + 11s));
    r.event(t0 + 10min + 40s, "liftoff", nlohmann::json::object());
    EXPECT_FALSE(r.wrong(Lamp::Strobes, t0 + 10min + 41s));
    EXPECT_FALSE(r.wrong(Lamp::LandingLights, t0 + 10min + 41s));
}

TEST(LampBoard, StrobesOnAfterTaxiIn) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.event(t0 + 70min, "touchdown", nlohmann::json::object());
    r.phase(t0 + 71min, "taxi_in");
    r.sample(t0 + 71min, true, true);
    EXPECT_FALSE(r.wrong(Lamp::Strobes, t0 + 73min + 59s));
    EXPECT_TRUE(r.wrong(Lamp::Strobes, t0 + 74min + 1s));
    r.b.onBlockIn(t0 + 76min);
    EXPECT_FALSE(r.wrong(Lamp::Strobes, t0 + 76min));
}

TEST(LampBoard, LandingLightsBelow10000) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 60min, "below_10000");
    EXPECT_FALSE(r.wrong(Lamp::LandingLights, t0 + 60min + 59s));
    EXPECT_TRUE(r.wrong(Lamp::LandingLights, t0 + 61min + 1s));
    r.event(t0 + 70min, "touchdown", nlohmann::json::object());
    EXPECT_FALSE(r.wrong(Lamp::LandingLights, t0 + 70min + 1s));
}

TEST(LampBoard, GoAroundRestartsLandingLights) {
    // As the debrief: the window starts again at the go-around (docs/debrief.md section 2).
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 60min, "below_10000");
    r.event(t0 + 60min + 10s, "cockpit", cockpit(2, true));
    r.phase(t0 + 66min, "go_around");
    r.event(t0 + 66min + 30s, "cockpit", cockpit(2, false));
    EXPECT_TRUE(r.wrong(Lamp::LandingLights, t0 + 66min + 31s));
}

TEST(LampBoard, ApuOnAfterTransitionAltitude) {
    Board r;
    r.event(t0, "cockpit", {{"pmdg", true}, {"seat_belts", 2}, {"apu_switch", true}});
    r.b.onBlockOut(t0 + 1min);
    EXPECT_TRUE(r.lamp(Lamp::Apu, t0 + 2min).lit);
    EXPECT_FALSE(r.wrong(Lamp::Apu, t0 + 2min));
    r.marker(t0 + 24min, "transition_altitude");
    EXPECT_TRUE(r.wrong(Lamp::Apu, t0 + 24min + 1s));
    r.event(t0 + 25min, "cockpit", {{"pmdg", true}, {"seat_belts", 2}, {"apu_switch", false}});
    EXPECT_FALSE(r.wrong(Lamp::Apu, t0 + 25min));
}

TEST(LampBoard, NoApuSwitchIsUnknown) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 24min, "transition_altitude");
    EXPECT_FALSE(r.lamp(Lamp::Apu, t0 + 30min).known);
    EXPECT_FALSE(r.wrong(Lamp::Apu, t0 + 30min));
}

TEST(LampBoard, AltStdBothWays) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 24min, "transition_altitude");
    EXPECT_FALSE(r.wrong(Lamp::AltStd, t0 + 24min + 59s));
    EXPECT_TRUE(r.wrong(Lamp::AltStd, t0 + 25min + 1s));  // still on 1019 a minute past the transition altitude
    r.event(t0 + 26min, "altimeter", altimeter(1013.2));
    EXPECT_TRUE(r.lamp(Lamp::AltStd, t0 + 26min).lit);
    EXPECT_FALSE(r.wrong(Lamp::AltStd, t0 + 26min));
    r.marker(t0 + 60min, "transition_level");
    EXPECT_FALSE(r.wrong(Lamp::AltStd, t0 + 60min + 59s));
    EXPECT_TRUE(r.wrong(Lamp::AltStd, t0 + 61min + 1s));  // still on STD a minute past the transition level
    r.event(t0 + 62min, "altimeter", altimeter(1022.4));
    EXPECT_FALSE(r.wrong(Lamp::AltStd, t0 + 62min));
}

TEST(LampBoard, GearUpOnFinal) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.event(t0 + 30min, "cockpit", cockpit(2, true, false));
    EXPECT_FALSE(r.wrong(Lamp::GearDown, t0 + 31min));  // up in flight: fine
    r.marker(t0 + 68min, "final_approach");
    EXPECT_TRUE(r.wrong(Lamp::GearDown, t0 + 68min));
    r.event(t0 + 70min, "touchdown", nlohmann::json::object());
    EXPECT_FALSE(r.wrong(Lamp::GearDown, t0 + 70min));
}

TEST(LampBoard, GoAroundClosesTheGearWindow) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 68min, "final_approach");
    r.phase(t0 + 68min + 30s, "go_around");
    r.event(t0 + 68min + 40s, "cockpit", cockpit(2, true, false));  // gear up for the go-around
    EXPECT_FALSE(r.wrong(Lamp::GearDown, t0 + 69min));
}

TEST(LampBoard, EventTimeComesFromTheLine) {
    // The takeoff is back-dated to the start of the roll: its data.time counts, not when it arrived.
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.event(t0 + 10min + 30s, "phase_changed", {{"phase", "takeoff"}, {"time", "2026-11-25T18:10:00Z"}});
    EXPECT_TRUE(r.wrong(Lamp::Strobes, t0 + 10min + 11s));
}

// Dave's KSAN → KSJC leg (tests/data/flight-ksan-ksjc): flown with every switch on time. The only lamp to flash is
// GEAR DN on short final, because the 0.11.0 recorder logged the gear as up all flight (debrief scored it missed;
// docs/notes/msfs-observed-behaviour.md).
TEST(LampBoard, KsanKsjcFixture) {
    const auto lines = loadFlightEvents(BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc", "2026-11-25T18:43:04.053Z");
    ASSERT_FALSE(lines.empty());
    LampBoard b;
    b.reset(Family::B737);
    const std::vector<std::pair<const char*, std::set<Lamp>>> moments = {
        {"2026-11-25T18:44:04Z", {}},  // block out + 1 min
        {"2026-11-25T18:49:00Z", {}},  // taxi
        {"2026-11-25T18:52:20Z", {}},  // the takeoff roll
        {"2026-11-25T19:15:00Z", {}},  // cruise
        {"2026-11-25T19:30:00Z", {}},  // descent
        {"2026-11-25T19:58:00Z", {Lamp::GearDown}},  // short final
        {"2026-11-25T20:00:30Z", {}},  // taxi in
    };
    std::size_t next = 0;
    for (const LogLine& l : lines) {
        while (next < moments.size() && l.ts > *parseUtc(moments[next].first)) {
            const TimePoint at = *parseUtc(moments[next].first);
            std::set<Lamp> wrong;
            for (const LampState& s : b.lamps(at))
                if (s.wrong) wrong.insert(s.lamp);
            EXPECT_EQ(wrong, moments[next].second) << moments[next].first;
            EXPECT_FALSE(b.lamps(at)[static_cast<int>(Lamp::Apu)].known);  // no apu_switch in a 0.11.0 log
            EXPECT_TRUE(b.lamps(at)[static_cast<int>(Lamp::SeatBelts)].known);
            ++next;
        }
        if (l.type == "telemetry") b.onSample(telemetryFromJson(l.data), l.ts);
        else if (l.type == "block_out") b.onBlockOut(l.ts);
        else if (l.type == "block_in") b.onBlockIn(l.ts);
        else b.onEvent(l.type, l.data, l.ts);
    }
    EXPECT_EQ(next, moments.size());
}

// Review fix: checks 1, 7, 12 and 13 stop at the first fix (the debrief's secondsUntil). After it, the switch is Dave's
// again and the lamp never nags (D-195).
TEST(LampBoard, ApuOnAgainAfterLandingDoesNotFlash) {
    Board r;
    r.event(t0, "cockpit", {{"pmdg", true}, {"seat_belts", 2}, {"apu_switch", true}});
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 24min, "transition_altitude");
    r.event(t0 + 25min, "cockpit", {{"pmdg", true}, {"seat_belts", 2}, {"apu_switch", false}});
    r.event(t0 + 72min, "cockpit", {{"pmdg", true}, {"seat_belts", 2}, {"apu_switch", true}});  // started on taxi in
    EXPECT_FALSE(r.wrong(Lamp::Apu, t0 + 73min));
}

TEST(LampBoard, BeaconOffLaterDoesNotFlash) {
    Board r;  // beacon on at the gate
    r.b.onBlockOut(t0 + 1min);
    r.sample(t0 + 30min, false, true);  // off in cruise: not scored
    EXPECT_FALSE(r.wrong(Lamp::Beacon, t0 + 31min));
}

TEST(LampBoard, StrobesBackOnToCrossARunway) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.event(t0 + 70min, "touchdown", nlohmann::json::object());
    r.phase(t0 + 71min, "taxi_in");
    r.sample(t0 + 71min, true, true);
    r.sample(t0 + 72min, true, false);  // off
    r.sample(t0 + 75min, true, true);   // on again to cross a runway
    EXPECT_FALSE(r.wrong(Lamp::Strobes, t0 + 76min));
}

TEST(LampBoard, QnhInCruiseDoesNotFlash) {
    Board r;
    r.b.onBlockOut(t0 + 1min);
    r.marker(t0 + 24min, "transition_altitude");
    r.event(t0 + 24min + 30s, "altimeter", altimeter(1013.2));
    r.event(t0 + 40min, "altimeter", altimeter(1019));  // set early: the debrief judged the climb already
    EXPECT_FALSE(r.wrong(Lamp::AltStd, t0 + 41min));
}
