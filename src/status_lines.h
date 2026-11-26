// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's three status lines (docs/dashboard.md, "The status lines" and "Every status line"; D-152, D-182),
// built from the same events as FlightStages. Before block out there's one line: "Preparing aircraft for flight", or
// one shared line for every GSX service of the leg. From block out each phase or event adds a line; level-offs, step
// climbs, GSX and deboarding change the newest line in place. After block in, the shutdown checklist.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "flight_stages.h"
#include "gsx_timeline.h"

namespace blocktime {

// What the lines need from the flight and its plan; set by the app whenever the flight changes.
struct StatusContext {
    int cruiseAltitudeFt = 0, transitionAltitudeFt = 0, transitionLevelFt = 0;  // 0: no plan
    std::string destinationCity, departureRunway;
};

struct StatusLine {
    std::string text, startIso;  // startIso "" when the line has no time
};

// The arrival_gate event's gate name: BlockTime's spot when it assigned one or it's unconfirmed, else SayIntentions'
// gate (arrivalGateText's rule, D-155).
std::string arrivalGateName(const nlohmann::json& arrivalGate);

class StatusLines {
public:
    void setContext(StatusContext context) { context_ = std::move(context); }
    // The events FlightStages takes: flight events, block events, gsx_service, sim_stopped and sim_disconnected.
    void onEvent(std::string_view type, const nlohmann::json& data);
    // The latest shutdown checklist values; counted only after block in. True if the lines or checklist changed.
    bool onShutdownChecks(const ShutdownChecks& checks, const std::string& iso);
    void showNextLeg();  // the summary was dismissed

    std::vector<StatusLine> lines() const;           // newest first, at most three
    std::vector<ChecklistItem> checklist() const;    // empty until block in

private:
    void push(std::string text, const std::string& iso);
    void onPhase(const std::string& phase, const std::string& iso, const nlohmann::json& d);
    void parked(const std::string& iso);
    void updateReady(const std::string& iso);
    void clearFlight();  // everything but the GSX timeline and the context
    std::string preFlightText() const;
    std::string preFlightStart() const;
    std::string flightLevelText(int ft, std::string_view phase) const;

    StatusContext context_;
    GsxTimeline gsx_;
    std::vector<StatusLine> lines_;  // the flight's lines, oldest first
    bool showingFlight_ = false;
    std::string phase_;
    std::optional<std::string> levelRestore_;  // the line a level-off replaced, while level
    bool finalShown_ = false;                  // this approach's Final approach line
    std::string finalRunway_, arrivalGate_, parking_;
    std::optional<std::size_t> landedLine_, taxiInLine_, parkedLine_, deboardingLine_;
    bool arrived_ = false, ready_ = false;
    std::optional<ShutdownChecks> checks_;
};

}  // namespace blocktime
