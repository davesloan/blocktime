// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// "Final approach": lined up with one of the destination's runways (D-143, D-145, D-148). Every runway end is
// checked; of the ends that qualify, the nearest centreline wins, so close parallels get the right name.

#pragma once

#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "phase_detector.h"
#include "runway_match.h"

namespace blocktime {

inline constexpr double kLinedUpRangeNm = 10.0;          // of the threshold, before it
inline constexpr double kLinedUpOffHeadingDeg = 10.0;    // the track against the runway's heading
inline constexpr double kLinedUpOffCentrelineNm = 0.5;
inline constexpr double kLinedUpHoldS = 10.0;            // active seconds
inline constexpr double kTrackBaseM = 150.0;             // the track is taken over at least this far

struct LinedUp {
    std::string runway;  // "27R"
    TimePoint time;      // when the hold began
    int altFt = 0;       // indicated, then
    double distanceNm = 0;  // to the threshold, then
};

class LinedUpDetector {
public:
    explicit LinedUpDetector(std::vector<Runway> destinationRunways);
    // Only airborne in Descent or Approach. Fires once; a GoAround sample re-arms it.
    std::optional<LinedUp> onSample(const PhaseSample& s, Phase phase);

private:
    struct End {
        std::string name;
        double headingDeg = 0, lat = 0, lon = 0;  // landing heading (true) and threshold
    };
    struct Point {
        double lat = 0, lon = 0;
    };
    struct Hold {
        std::string runway;
        double startActiveS = 0;
        LinedUp at;
    };

    std::optional<double> track(const PhaseSample& s) const;

    std::vector<End> ends_;
    std::deque<Point> recent_;
    std::optional<Hold> hold_;
    bool fired_ = false;
};

}  // namespace blocktime
