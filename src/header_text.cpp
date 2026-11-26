// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "header_text.h"

#include <cstdlib>
#include <format>

namespace blocktime {
namespace {

bool climbing(std::string_view phase) { return phase == "takeoff" || phase == "climb" || phase == "cruise"; }
bool descending(std::string_view phase) { return phase == "descent" || phase == "approach" || phase == "go_around"; }

}  // namespace

bool isFlightLevel(std::string_view phase, int altFt, int transitionAltitudeFt, int transitionLevelFt) {
    if (climbing(phase)) return transitionAltitudeFt > 0 && altFt >= transitionAltitudeFt;
    if (descending(phase)) return transitionLevelFt > 0 && altFt >= transitionLevelFt;
    return false;
}

int effectiveTransitionFt(int planFt) { return planFt > 0 ? planFt : 18000; }

std::string altitudeText(int ft, bool flightLevel) {
    if (flightLevel) return std::format("FL{:03}", (ft + 50) / 100);
    std::string digits = std::to_string(std::abs(ft));
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) digits.insert(static_cast<std::size_t>(i), ",");
    return (ft < 0 ? "-" : "") + digits + " ft";
}

std::string durationText(std::chrono::seconds s, bool withSeconds) {
    const long long total = s.count() < 0 ? 0 : s.count();
    const long long h = total / 3600, m = total / 60 % 60, sec = total % 60;
    return withSeconds ? std::format("{}:{:02}:{:02}", h, m, sec) : std::format("{}:{:02}", h, m);
}

std::optional<TimePoint> blockInEstimate(std::optional<TimePoint> blockOut, TimePoint now, int planBlockS) {
    if (planBlockS <= 0) return std::nullopt;
    return (blockOut ? *blockOut : now) + std::chrono::seconds(planBlockS);
}

}  // namespace blocktime
