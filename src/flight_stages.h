// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// design.md §13.5's six stages, built from the recorder's flight events (D-128): phase_changed, level_off(_ended),
// step_climb(_ended), flight_marker, liftoff, touchdown and landing_runway, with the fields FlightTracker logs. Each
// stage lists its sub-events with their start and end times. Pre-Departure fills in with milestone 8's GSX services,
// and Arrival's gate comes with milestone 9.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

enum class Stage { PreDeparture, Departure, Climb, Cruise, Descent, Arrival };
enum class StageState { Upcoming, Current, Done };

struct StageLine {
    std::string text;
    std::string start;  // "HH:MMZ"
    std::string end;    // "HH:MMZ"; "" for an instant or a line still running
    bool running = false;
};

struct StageView {
    Stage stage;
    std::string name;
    StageState state;
    std::vector<StageLine> lines;  // by start time
};

class FlightStages {
public:
    void onEvent(std::string_view type, const nlohmann::json& data);
    void reset();

    std::vector<StageView> view() const;  // always six; before any event, Pre-Departure is Current
    // The phase with its sub-state: "Climb — level at 11,000 ft", "Cruise — step climb"; "" before block out.
    std::string phaseLabel() const;

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

    std::vector<Line> lines_;
    std::string phase_;
    std::optional<std::size_t> phaseLine_, cruiseLine_, levelLine_, stepLine_, landedLine_;
    int levelFt_ = 0;
    bool levelFl_ = false;
    int stepFromFt_ = 0;
    std::optional<double> touchdownVsFpm_;
    std::string landingRunway_;
};

}  // namespace blocktime
