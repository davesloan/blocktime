// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's phase pill (docs/dashboard.md, "The pill and its marker", D-182, D-184): one section per stage, and
// a marker under the current one that moves through a few short sub-phases, so the bar never changes shape.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "flight_stages.h"

namespace blocktime {

struct PillSection {
    std::string name;
    StageState state;
};

// Where the marker sits: the current section, split into `parts` equal parts, in the middle of part `part`.
struct PillMarker {
    int section = 0;
    int part = 0;
    int parts = 1;
    std::string label;  // "Taxiing"; "" in a one-part section (Climb, Cruise), where it would repeat the section name
};

std::vector<PillSection> pillSections(const std::vector<StageView>& stages);  // always six

// Pre-Flight: Preparing, Boarding (once GSX boarding has started). Departure: Pushback, Taxiing, Takeoff. Climb and
// Cruise: one part. Descent: Descending, Approach (a go-around too). Arrival: Landing, Taxiing, At Gate.
PillMarker pillMarker(const std::vector<StageView>& stages, std::string_view phase, bool boardingStarted);

}  // namespace blocktime
