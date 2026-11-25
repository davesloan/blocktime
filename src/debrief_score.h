// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief's rules (docs/debrief.md sections 2-4): stored measurements (debrief_measure.h) to each check's outcome,
// the landing's 0-5 stars (D-158) and the 0-100 score (D-160). Run every time a score is shown, so a changed rule
// re-scores every flight. Every tolerance, band and point is in debrief_score.cpp, and nowhere else. Pure logic.

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "debrief_measure.h"

namespace blocktime {

enum class CheckState { Passed, Missed, NotChecked };

struct CheckOutcome {
    std::string label;   // "Beacon on before pushback"
    std::string detail;  // "2 min 43 s late"; a not-checked check's reason ("No PMDG data")
    CheckState state = CheckState::NotChecked;
    int points = 0;      // 0, or -6 when missed
};

struct ScoredFlight {
    std::vector<CheckOutcome> checks;  // the twelve, in docs/debrief.md's order
    std::optional<int> stars;          // 0-5; nullopt without a landing
    int landingPoints = 0;             // 0 to -30
    std::optional<int> crosswindKt;    // with a landing
    int score = 0;                     // 0-100
};

ScoredFlight scoreFlight(const FlightMeasurements& m);

struct Averages {
    std::optional<double> score, scoreLast10, stars, starsLast10;  // nullopt with no flight (or no landing)
};
Averages averages(const std::vector<ScoredFlight>& newestFirst);

std::string starsText(int stars);  // "★★★☆☆"

}  // namespace blocktime
