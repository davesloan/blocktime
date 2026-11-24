// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The flight phases (recorder.md "Phases"): plan-aware, forward-only, with level-offs and step climbs as sub-states and
// go-around as the one step back. Pure logic, fed one valid telemetry sample at a time from block out to block in.
// The numbers are D-124's, tuned on Dave's two probe flights (tests/data/replay-*.csv). Holds are timed on the
// ActiveClock, so a sim freeze adds nothing to them (D-058).

#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

enum class Phase { AtGate, Pushback, TaxiOut, Takeoff, Climb, Cruise, Descent, Approach, GoAround, Landing, TaxiIn,
                   Arrived };

// "at_gate", "pushback", "taxi_out", "takeoff", "climb", "cruise", "descent", "approach", "go_around", "landing",
// "taxi_in", "arrived"
std::string_view phaseCode(Phase p);

// From the SimBrief plan; 0 / false = unknown.
struct PhasePlan {
    int cruiseAltitudeFt = 0, transitionAltitudeFt = 0, transitionLevelFt = 0;
    bool hasDestination = false;
    double destinationLat = 0, destinationLon = 0;
};

struct PhaseSample {
    TimePoint time{};
    double activeS = 0;  // ActiveClock seconds
    bool parkingBrake = false, onGround = false;
    double altFt = 0;  // indicated
    double aglFt = 0, vsFpm = 0, groundSpeedKt = 0, lat = 0, lon = 0;
    double iasKt = 0;  // not used by the rules; the liftoff event logs it
};

struct PhaseChanged {
    Phase phase, previous;
    TimePoint time;
    int altFt;  // the sample's; a cruise's is its level
};
struct LevelOff {  // a level in Climb, Descent or Approach
    Phase phase;
    TimePoint time;  // when the level began
    int altFt;       // the hold window's mean, to the nearest 100
};
struct LevelOffEnded {
    Phase phase;
    TimePoint time;
    int altFt;  // the whole level's mean, to the nearest 100
};
struct StepClimb {  // the cruise level was left: upward, or downward too far out for top of descent
    TimePoint time;
    int fromFt;
};
struct StepClimbEnded {
    TimePoint time;
    int altFt;
};

enum class MarkerKind { Above10000, TransitionAltitude, Within200Nm, TransitionLevel, Below10000, FinalApproach };

// "above_10000", "transition_altitude", "within_200nm", "transition_level", "below_10000", "final_approach"
std::string_view markerCode(MarkerKind k);

// Each fires once per flight, on the sample that crosses (design.md §13.5's sub-events).
struct Marker {
    MarkerKind kind;
    TimePoint time;
    int altFt;
};

using PhaseEvent = std::variant<PhaseChanged, LevelOff, LevelOffEnded, StepClimb, StepClimbEnded, Marker>;

// D-124
inline constexpr double kPushbackMoveKt = 3.0;      // after the brake set that ends the push
inline constexpr double kTaxiWithoutPushKt = 6.0;   // GSX pushes run at about 4 kt
inline constexpr double kTakeoffRollKt = 40.0;      // taxiing reached 23 kt on the probe flights
inline constexpr double kRollWalkBackKt = 0.5;      // each earlier sample at least this much slower
inline constexpr double kTaxiOutMemoryS = 60.0;     // samples kept for the walk-back
inline constexpr double kClimbAglFt = 1000.0;
inline constexpr double kLevelBandFt = 100.0;       // from the hold window's first sample
inline constexpr double kLevelHoldS = 20.0;
inline constexpr double kLevelLeftFt = 200.0;       // a level ends this far from it
inline constexpr double kCruiseWindowFt = 1000.0;   // from the plan's cruise altitude
inline constexpr double kNoPlanCruiseMinFt = 10000.0;
inline constexpr double kNoPlanCruiseHoldS = 300.0;
inline constexpr double kDescentVsFpm = -500.0;
inline constexpr double kDescentHoldS = 60.0;
inline constexpr double kTodRangeFactor = 2.0;      // times 3 nm per 1,000 ft
inline constexpr double kApproachRangeNm = 30.0;
inline constexpr double kApproachBelowFt = 10000.0;
inline constexpr double kApproachAglFt = 3000.0;
inline constexpr double kGoAroundVsFpm = 500.0;
inline constexpr double kGoAroundHoldS = 10.0;
inline constexpr double kGoAroundGainFt = 200.0;
inline constexpr double kTaxiInKt = 30.0;
inline constexpr double kMarkerAltFt = 10000.0;
inline constexpr double kMarkerRangeNm = 200.0;
inline constexpr double kFinalApproachAglFt = 1000.0;

class PhaseDetector {
public:
    PhaseDetector(PhasePlan plan, TimePoint blockOut, double altFt);

    PhaseChanged started() const { return started_; }  // AtGate → Pushback at block out
    std::vector<PhaseEvent> onSample(const PhaseSample& s);
    PhaseChanged onBlockIn(TimePoint t, double altFt);  // → Arrived

    Phase phase() const { return phase_; }
    std::optional<int> levelFt() const;  // while level in Climb, Descent or Approach
    bool stepClimbing() const { return step_.has_value(); }
    const PhasePlan& plan() const { return plan_; }

private:
    struct Level {
        int altFt = 0;
        TimePoint start{};
        double startActiveS = 0;
        double sum = 0;
        int count = 0;
    };
    struct Window {
        double firstAltFt = 0, startActiveS = 0, sum = 0;
        int count = 0;
        TimePoint start{};
    };
    struct Hold {
        double startActiveS = 0;
        TimePoint start{};
    };

    void advance(std::vector<PhaseEvent>& out, const PhaseSample& s);
    void markers(std::vector<PhaseEvent>& out, const PhaseSample& s);
    void change(std::vector<PhaseEvent>& out, Phase to, TimePoint t, int altFt);
    void onTaxiOut(std::vector<PhaseEvent>& out, const PhaseSample& s);
    void onAirborneLevels(std::vector<PhaseEvent>& out, const PhaseSample& s);
    void onLevelConfirmed(std::vector<PhaseEvent>& out, const PhaseSample& s);
    void onDescentHold(std::vector<PhaseEvent>& out, const PhaseSample& s);
    double distanceToDestinationNm(const PhaseSample& s) const;

    PhasePlan plan_;
    PhaseChanged started_;
    Phase phase_ = Phase::Pushback;

    bool pushBrakeSet_ = false;
    std::deque<PhaseSample> taxi_;          // TaxiOut's recent samples, for the takeoff walk-back
    std::optional<Window> window_;          // the level hold being timed
    std::optional<Level> level_;            // a confirmed level (Climb, Descent, Approach) or the cruise level
    std::optional<Hold> descentHold_;
    std::optional<Hold> goAroundHold_;
    std::optional<TimePoint> leftCruiseAt_;  // when the cruise level was left downward, waiting for the hold
    std::optional<int> step_;               // a step climb in progress: the level it left
    int cruiseLevelFt_ = 0;
    double approachLowestFt_ = 0;
    std::optional<PhaseSample> previous_;  // for the markers' crossings
    bool fired_[6] = {};                   // by MarkerKind
};

}  // namespace blocktime
