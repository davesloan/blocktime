// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's phase pill (docs/dashboard.md, "The pill's divisions", D-182): one section per stage, and inside
// Departure and Descent a fixed set of the recorder's phases, so the bar never changes shape.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "flight_stages.h"

namespace blocktime {

struct PillSection {
    std::string name;
    StageState state;
    std::vector<StageState> divisions;  // Departure: pushback, taxi out, takeoff; Descent: descent, approach, landing,
                                        // taxi in; the others none
};

// Always six sections. Inside the current stage, divisions before `phase` are done, its own is current (a go-around
// counts as approach) and the rest upcoming; a done stage's are all done, an upcoming stage's all upcoming.
std::vector<PillSection> pillSections(const std::vector<StageView>& stages, std::string_view phase);

}  // namespace blocktime
