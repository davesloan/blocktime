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
#include "lined_up.h"
#include "log_event.h"
#include "phase_detector.h"
#include "parking.h"
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

// The flight_accepted marker's data (D-236): {planned_flight_id, planned_at, registration, origin, destination,
// at_block_out}. No flight_id: the flight doesn't exist until block out.
nlohmann::json acceptedEventData(const FlightTarget& target, bool atBlockOut);

// The marker block out writes before block_out when the target has a plan but wasn't accepted (D-250); nullopt when
// it was accepted, or with no target.
std::optional<LogEvent> acceptedAtBlockOut(const std::optional<FlightTarget>& target);

class FlightTracker {
public:
    // Kept across flights; newer data for the same airport replaces the old (D-126, D-147).
    void setAirport(AirportFacilities airport);
    // The name of the nearest parking spot of any airport set, within kParkedSpotMaxM; "" if none (D-148).
    std::string parkingAt(double lat, double lon) const;

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
    const std::string& flightId() const { return flightId_; }  // "" with no flight open

private:
    LogEvent toLog(const PhaseEvent& e) const;
    LogEvent bouncesEvent(TimePoint at, int bounces);
    LogEvent altimeterEvent(const AltimeterSample& a) const;
    bool flightLevel(Phase phase, int altFt) const;
    std::vector<AirportRunways> runwayList() const;
    void updateFastSampling(const PhaseSample& s);

    std::map<std::string, AirportFacilities> airports_;
    std::optional<AltimeterSample> altimeter_;  // the latest, flight or not
    std::optional<AltimeterSample> logged_;     // the last one logged in this flight (rounded)

    std::optional<PhaseDetector> detector_;
    std::string destinationIcao_;
    std::optional<LinedUpDetector> linedUp_;
    std::string flightId_;
    std::vector<RollPoint> takeoffRoll_, landingRoll_;
    bool liftoffLogged_ = false;
    TouchdownCapture touchdown_;
    bool fast_ = false;
    bool bouncesDone_ = false;  // the bounce count is logged: fast sampling is over for this flight
};

}  // namespace blocktime
