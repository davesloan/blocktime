// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The banner's answer (D-088): the same check a brake release would get, reported only when what the banner shows
// changes: the problem, the planned flight, the aircraft, or a move of more than a kilometre (a different airport).
// Distance and fuel change with every sample, so they alone don't count. It's checked on each sample and again
// whenever the recorder loop asks (recheck), against the last sample: telemetry is asked for on change, so a parked
// airplane sends none, and a plan made at the gate would otherwise wait for the next switch to be noticed.

#pragma once

#include <optional>

#include "block_detector.h"
#include "flight_match.h"

namespace blocktime {

class MatchReporter {
public:
    // The result to report, or nullopt when the banner wouldn't change.
    std::optional<MatchResult> onSample(const std::optional<FlightTarget>& target, const BlockSample& s);
    // The last sample checked against the target as it is now; nullopt before a sample or when nothing changed.
    std::optional<MatchResult> recheck(const std::optional<FlightTarget>& target);
    // A new sim session: forget the last sample and what was reported.
    void reset();

private:
    std::optional<BlockSample> last_;
    std::optional<MatchResult> reported_;
};

}  // namespace blocktime
