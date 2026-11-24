// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "match_reporter.h"

#include "geo.h"

namespace blocktime {

namespace {

constexpr double kMovedM = 1000.0;  // a move this far may be a different airport

bool bannerChanges(const MatchResult& before, const MatchResult& now) {
    return before.problem != now.problem || before.plannedFlightId != now.plannedFlightId ||
           before.aircraftFolder != now.aircraftFolder ||
           distanceMeters(before.lat, before.lon, now.lat, now.lon) > kMovedM;
}

}  // namespace

std::optional<MatchResult> MatchReporter::onSample(const std::optional<FlightTarget>& target, const BlockSample& s) {
    last_ = s;
    return recheck(target);
}

std::optional<MatchResult> MatchReporter::recheck(const std::optional<FlightTarget>& target) {
    if (!last_) return std::nullopt;
    MatchResult m = checkMatch(target, *last_);
    if (reported_ && !bannerChanges(*reported_, m)) return std::nullopt;
    reported_ = m;
    return m;
}

void MatchReporter::reset() {
    last_.reset();
    reported_.reset();
}

}  // namespace blocktime
