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
// - VREF, landing flaps, the autopilot and the APU: PMDG only, so they're left out of the event without it (the plain
//   APU SWITCH never moved on either family).

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
    bool apuSwitch = false;      // the APU selector away from OFF (737 APU_Selector, 777 ELEC_APU_Selector)
    bool apuRunning = false;     // 777: APURunning; 737: selector on and EGT at least kApuRunningEgtC (D-172)
};

// The 737 has no APU-running flag in PMDG's data. On the probe flights a start peaked near 750 C about 30 s after the
// switch and the running APU then settled at 390-470 C; 300 C is passed about 25 s into the start, and with the switch
// off (cooling down) it never counts (docs/notes/probe-flight.md, D-172).
inline constexpr double kApuRunningEgtC = 300;

struct PlainCockpit {
    bool landingLight = false;  // LIGHT LANDING
    bool seatBeltSign = false;  // CABIN SEATBELTS ALERT SWITCH
    // GEAR TOTAL PCT EXTENDED: despite the name and the "percent" unit it reads 0 to 1 (1.0 = down), seen on the probe
    // flights and the milestone 10 sim test (docs/notes/msfs-observed-behaviour.md).
    double gearExtended = 0;
    bool paused = false;        // the sim is paused: the sample is ignored
};

inline constexpr double kGearDownFraction = 0.995;

class CockpitMonitor {
public:
    void onPlain(const PlainCockpit& p, TimePoint now);
    void onPmdg(const PmdgCockpit& p, TimePoint now);
    // The event's data when it changed since the last one written and at least 1 s has passed since then.
    std::optional<nlohmann::json> due(TimePoint now);
    // The current state, changed or not (nullopt before the first plain sample): written at block out, so each
    // flight's log holds the cockpit state however long ago it last changed (cockpit lines are change-based).
    std::optional<nlohmann::json> snapshot(TimePoint now);
    void reset();  // a new sim session

private:
    nlohmann::json current() const;

    std::optional<PlainCockpit> plain_;
    std::optional<PmdgCockpit> pmdg_;
    nlohmann::json last_;  // null until the first event
    std::optional<TimePoint> lastWritten_;
};

}  // namespace blocktime
