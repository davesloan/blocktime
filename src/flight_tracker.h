// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One flight, from block out to block in, as log events: its phases, level-offs and markers (PhaseDetector), the
// runways it used (runway_match.h, D-126), its touchdown (TouchdownCapture, D-125) and the altimeter settings (D-123).
// The recorder feeds it and writes what it returns; it says when the fast touchdown samples are wanted.
// Pure logic, so it can be tested without the sim.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "block_detector.h"
#include "flight_match.h"
#include "log_event.h"
#include "phase_detector.h"
#include "runway_match.h"
#include "touchdown_capture.h"

namespace blocktime {

// KOHLSMAN SETTING MB:1/2 and KOHLSMAN SETTING STD:1/2 (D-123).
struct AltimeterSample {
    double captMb = 0, foMb = 0;
    bool captStd = false, foStd = false;
};

inline constexpr double kTouchdownArmAglFt = 300;     // D-125
inline constexpr double kTouchdownDisarmAglFt = 500;  // climbing back through this: a go-around
inline constexpr double kRunwayRollKt = 40;           // the roll's samples, for naming the runway

class FlightTracker {
public:
    // Kept across flights; a newer list for the same airport replaces the old one.
    void setRunways(AirportRunways airport);

    // Starts the flight: phase_changed (pushback) and the current altimeter settings.
    std::vector<LogEvent> onBlockOut(const BlockOut& out, const std::optional<FlightTarget>& target, double altFt);
    std::vector<LogEvent> onSample(const PhaseSample& s);
    std::vector<LogEvent> onFastSample(const FastSample& s);
    // Remembered always; logged only while a flight is open, when it changes (to 0.1 hPa).
    std::vector<LogEvent> onAltimeter(const AltimeterSample& a);
    // Ends the flight: phase_changed (arrived).
    std::vector<LogEvent> onBlockIn(const BlockIn& in, double altFt);
    void onFlightEnded();  // interrupted: forget the flight

    bool wantsFastSampling() const { return fast_; }
    bool flightOpen() const { return detector_.has_value(); }

private:
    LogEvent toLog(const PhaseEvent& e) const;
    LogEvent altimeterEvent(const AltimeterSample& a) const;
    bool flightLevel(Phase phase, int altFt) const;
    std::vector<AirportRunways> runwayList() const;
    void updateFastSampling(const PhaseSample& s);

    std::map<std::string, AirportRunways> runways_;
    std::optional<AltimeterSample> altimeter_;  // the latest, flight or not
    std::optional<AltimeterSample> logged_;     // the last one logged in this flight (rounded)

    std::optional<PhaseDetector> detector_;
    std::string flightId_;
    std::vector<RollPoint> takeoffRoll_, landingRoll_;
    bool liftoffLogged_ = false;
    TouchdownCapture touchdown_;
    bool fast_ = false;
};

}  // namespace blocktime
