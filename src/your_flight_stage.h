// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What Your Flight shows (docs/dispatch.md, D-233, review 3.1): one stage worked out from the facts, so the page and
// the dashboard's panels never each decide from their own flags.

#pragma once

#include <string_view>

namespace blocktime {

enum class YourFlightStage { Empty, Form, Dispatched, Imported, Accepted, Flying, Arrived, Summary };

struct StageInputs {
    bool formOpen = false;        // the dispatch form is open (new flight, or Change flight)
    bool planned = false;         // a planned flight exists
    bool planImported = false;    // its SimBrief plan is in
    bool accepted = false;        // Accept was pressed (or block out accepted it, D-250)
    bool flightOpen = false;      // between block out and block in
    bool settlePending = false;   // flown, not settled yet
    bool summaryShowing = false;  // a settled flight's summary is held
    bool finishable = false;      // after block in, until Settle Up
};

// The first that applies wins: flightOpen → Flying; settlePending, or summaryShowing and finishable → Arrived;
// summaryShowing → Summary; formOpen and not accepted → Form; accepted → Accepted; planImported → Imported;
// planned → Dispatched; formOpen → Form; else Empty.
YourFlightStage yourFlightStage(const StageInputs& in);

// "empty", "form", "dispatched", "imported", "accepted", "flying", "arrived", "summary".
std::string_view stageName(YourFlightStage stage);

}  // namespace blocktime
