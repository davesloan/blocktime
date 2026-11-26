// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// design.md §13.5's six stages, built from the recorder's flight events (D-128): phase_changed, level_off(_ended),
// step_climb(_ended), flight_marker, liftoff, touchdown and landing_runway, with the fields FlightTracker logs. Each
// stage lists its sub-events with their start and end times. Arrival's gate comes with milestone 9.
//
// Milestone 8 (D-134..D-136, D-138): GSX's services, through a GsxTimeline fed gsx_service, block and sim session
// events. Until block out the view is the next leg: Pre-Flight with the services so far. From block out (or the
// first phase) until showNextLeg() the view is the flight: the services it took at block out, then its deboarding
// and "Ready for shutdown or turnaround" with the shutdown checklist.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "gsx_timeline.h"

namespace blocktime {

enum class Stage { PreDeparture, Departure, Climb, Cruise, Descent, Arrival };
enum class StageState { Upcoming, Current, Done };

struct ChecklistItem {
    std::string text;
    bool done = false;
};

struct StageLine {
    std::string text;
    std::string start;  // "HH:MMZ"
    std::string end;    // "HH:MMZ"; "" for an instant or a line still running
    bool running = false;
    std::vector<ChecklistItem> checks;  // only the Ready line has any (D-135)
};

// What the shutdown checklist reads, from telemetry (D-135).
struct ShutdownChecks {
    bool enginesOff = false, parkingBrakeSet = false, beaconOff = false, strobeOff = false;
    bool operator==(const ShutdownChecks&) const = default;
};

// Shared with the dashboard's status lines (StatusLines).
std::string serviceName(const std::string& service);  // "refuelling" → "Refueling" (US spelling on screen)
bool anyRunning(const std::vector<ServiceSpan>& spans);
bool anyCompleted(const std::vector<ServiceSpan>& spans);
// The shutdown checklist (D-135): engines, brake, deboarding (only after GSX deboarding), beacon, strobes.
std::vector<ChecklistItem> shutdownChecklist(const ShutdownChecks& checks, const std::vector<ServiceSpan>& arrival);

struct StageView {
    Stage stage;
    std::string name;
    StageState state;
    std::vector<StageLine> lines;  // by start time
};

class FlightStages {
public:
    // Also block_out, block_in, flight_interrupted, sim_stopped, sim_disconnected and gsx_service.
    void onEvent(std::string_view type, const nlohmann::json& data);
    // The latest checklist values. Count only after block in while the flight shows; true if the view changed.
    bool onShutdownChecks(const ShutdownChecks& c, const std::string& iso);
    // The summary was dismissed: drop the last flight and show the next leg (D-134).
    void showNextLeg();
    bool showingFlight() const { return showingFlight_; }
    bool hasNextLegServices() const { return !gsx_.nextLeg().empty(); }
    // GSX boarding has started for the next block out: the pill's marker moves from Preparing to Boarding (D-184).
    bool boardingStarted() const;

    std::vector<StageView> view() const;  // always six; before any event, Pre-Flight is Current
    // The phase with its sub-state: "Climb — level at 11,000 ft", "Cruise — step climb"; "" before block out.
    std::string phaseLabel() const;
    // The recorder's current phase code ("taxi_out"); "" before block out. The pill's marker (pillMarker, D-184).
    const std::string& phase() const { return phase_; }

private:
    struct Line {
        Stage stage;
        std::string text, startIso, endIso;
        bool running = false;
    };

    std::size_t add(Stage stage, std::string text, const std::string& iso, bool running);
    void close(std::optional<std::size_t>& line, const std::string& iso);
    void onPhase(const std::string& phase, const std::string& iso, int altFt, bool flightLevel);
    std::string landedText() const;
    void clearFlight();  // everything but the GSX timeline
    void updateReady(const std::string& iso);
    StageLine readyLine() const;

    std::vector<Line> lines_;
    std::string phase_;
    std::optional<std::size_t> phaseLine_, cruiseLine_, levelLine_, stepLine_, landedLine_;
    std::optional<std::size_t> finalLine_, shortFinalLine_;  // D-149
    std::string parking_;  // block_in's spot, for "Parked at ..." (D-148)
    int levelFt_ = 0;
    bool levelFl_ = false;
    int stepFromFt_ = 0;
    std::optional<double> touchdownVsFpm_;
    std::string landingRunway_;

    GsxTimeline gsx_;
    bool showingFlight_ = false;
    std::optional<ShutdownChecks> checks_;
    std::string readyIso_;  // when "Ready for shutdown or turnaround" appeared; "" until then
};

}  // namespace blocktime
