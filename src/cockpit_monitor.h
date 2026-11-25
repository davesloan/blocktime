// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The cockpit state the debrief checks (docs/debrief.md section 1, D-159), merged from the plain SimVars and PMDG's
// SDK broadcast into one `cockpit` event, written when it changes and at most once a second. Facts only: the recorder
// never judges them. Pure logic, so it can be tested without the sim.
//
// Where each value comes from (docs/notes/probe-flight.md):
// - landing lights and seat belts: PMDG's broadcast when it has arrived (the 737's seat belt SimVar never moves, and
//   LIGHT LANDING misses its retractable lights); the plain SimVars otherwise;
// - the gear: GEAR TOTAL PCT EXTENDED, ignored while paused (the sim's gear values read 0 then);
// - VREF, landing flaps and the autopilot: PMDG only, so they're left out of the event without it.

#pragma once

#include <chrono>
#include <optional>

#include <nlohmann/json.hpp>

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

struct PmdgCockpit {             // one PMDG broadcast, decoded
    int seatBelts = 0;           // 0 off, 1 auto, 2 on
    bool landingLights = false;  // 737: any fixed light on or retractable at ON; 777: any landing light
    int vrefKt = 0;              // FMC_LandingVREF; 0 = not set
    int landingFlaps = 0;        // FMC_LandingFlaps, degrees; 0 = not set
    bool autopilot = false;      // 737: CMD A or B; 777: either AP engaged
};

struct PlainCockpit {
    bool landingLight = false;  // LIGHT LANDING
    bool seatBeltSign = false;  // CABIN SEATBELTS ALERT SWITCH
    double gearPct = 0;         // GEAR TOTAL PCT EXTENDED, 0-100
    bool paused = false;        // the sim is paused: the sample is ignored
};

inline constexpr double kGearDownPct = 99.5;

class CockpitMonitor {
public:
    void onPlain(const PlainCockpit& p, TimePoint now);
    void onPmdg(const PmdgCockpit& p, TimePoint now);
    // The event's data when it changed since the last one written and at least 1 s has passed since then.
    std::optional<nlohmann::json> due(TimePoint now);
    void reset();  // a new sim session

private:
    nlohmann::json current() const;

    std::optional<PlainCockpit> plain_;
    std::optional<PmdgCockpit> pmdg_;
    nlohmann::json last_;  // null until the first event
    std::optional<TimePoint> lastWritten_;
};

}  // namespace blocktime
