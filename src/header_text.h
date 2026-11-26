// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Texts and rules shared by the dashboard's header and status lines (docs/dashboard.md, D-182), and the flight-level
// rule the recorder's flight_level flag also uses.

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

// Flight level or feet: climbing phases (takeoff, climb, cruise) at or above the transition altitude, descending ones
// (descent, approach, go_around) at or above the transition level. A transition of 0 is never a flight level.
bool isFlightLevel(std::string_view phase, int altFt, int transitionAltitudeFt, int transitionLevelFt);
// The plan's transition, or 18,000 ft without one (the dashboard only; the recorder keeps 0).
int effectiveTransitionFt(int planFt);
// 33000 → "FL330" with a flight level; 8500 → "8,500 ft".
std::string altitudeText(int ft, bool flightLevel);
// 3900 s → "1:05"; with seconds, 2470 s → "0:41:10".
std::string durationText(std::chrono::seconds s, bool withSeconds);
// Until the predictions (D-120): block out (or now, before it) plus the plan's block time; none without a plan.
std::optional<TimePoint> blockInEstimate(std::optional<TimePoint> blockOut, TimePoint now, int planBlockS);

}  // namespace blocktime
