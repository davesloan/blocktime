// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The in-flight panel's readouts for each phase (D-194, docs/dashboard.md "Phase readouts"). Every phase starts with
// SOULS and FUEL REMAINING (D-203); a readout with no value is still listed, with "" for the dash, so the grid
// doesn't jump.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "plan_brief.h"
#include "preflight_log.h"

namespace blocktime {

enum class PanelPhase { Departure, Climb, Cruise, Arrival, ShortFinal, Landed };

// Departure: at_gate, pushback, taxi_out, takeoff (to liftoff). Climb, Cruise. Arrival: descent, approach, go_around.
// ShortFinal: on the last final_approach marker (after the last go-around) to touchdown. Landed: from touchdown.
PanelPhase panelPhase(std::string_view phase, bool onFinal, bool touchedDown);

struct ReadoutView {
    std::string title, value, unit, tag;  // tag: the aside or source ("ATIS", "PLAN 30R", "SET AT TL")
    bool flagged = false;                 // the amber PLAN flag (D-189)
    bool words = false;                   // Readout's smaller font
    bool wide = false;                    // two columns (APPROACHES, v2)
    bool ticks = false;                   // a value that ticks: never flips (FUEL REMAINING)
    int stars = -1;                       // LANDING only: 0-5
    bool operator==(const ReadoutView&) const = default;
};

struct InFlightFacts {  // what the readouts need; the model fills it from the log, the plan and the last sample
    DepartureView departure;
    ClearanceView clearance;                           // Pre-Flight's views (Task 3)
    int cruiseLevelFt = 0;                             // the plan's; 0 = none
    const nlohmann::json* destinationAtis = nullptr;  // the latest `atis` with role destination
    std::string atcStar, atcRunway;                    // from the card: the descend-via STAR, the approach's runway
    std::string towerWind;                             // windText of the landing clearance's wind; ""
    PlanBrief plan;
    int passengers = 0, crew = 0;                      // SOULS
    int fuelLb = 0;                                    // the last sample's, for FUEL REMAINING
    int vrefKt = 0, landingFlaps = 0;                  // the cockpit's; 0 = not set
    std::optional<int> stars;                          // after touchdown and bounces
    int touchdownFpm = 0;                              // the sink rate, positive
    double touchdownG = 0;                             // the touchdown's G; 0 = not logged
    std::string gate;                                  // arrival_gate's `gate` ("Gate 4"); ""
    // The predictions (D-192): distances to go, and local times the Qt model formats ("11:01 AM"); none = a dash.
    std::optional<int> tocNm, todNm, fuelAtLandingLb, planLandingFuelLb;
    std::string tocTime, todTime, landingTime, planLandingTime;
};

std::vector<ReadoutView> inFlightReadouts(PanelPhase phase, const InFlightFacts& f);

// A parking spot for a GATE or ARR GATE readout: "Gate 4" → "4"; other spots ("Parking 70") as they are.
std::string gateReadoutText(std::string_view spot);

}  // namespace blocktime
