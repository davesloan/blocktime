// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dashboard's route arc (docs/dashboard.md, "Route progress", D-182): how far along the planned route the airplane
// is, and the highest altitude flown in each 1% of it, for the arc's colours.

#pragma once

#include <cstddef>
#include <vector>

#include "simbrief.h"

namespace blocktime {

class RouteProgress {
public:
    // The origin, the navlog's fixes, the destination (PlannedFlightView::route). Fewer than two points: an empty
    // route, 0 NM long.
    explicit RouteProgress(std::vector<RoutePoint> route);

    double totalNm() const { return totalNm_; }
    double flownNm() const { return flownNm_; }
    double toGoNm() const { return totalNm_ - flownNm_; }
    double share() const { return totalNm_ > 0 ? flownNm_ / totalNm_ : 0.0; }  // 0..1

    // The latest position, projected onto the legs from the last matched one to three after it, so a route that
    // doubles back can't jump the marker. The distance flown never goes down.
    void update(double lat, double lon);
    void finish();  // block in: the whole route
    void reset();   // a new flight, or an interrupted one

private:
    std::vector<RoutePoint> route_;
    std::vector<double> legNm_, startNm_;  // per leg: its length, and the distance to its start
    double totalNm_ = 0, flownNm_ = 0;
    std::size_t leg_ = 0;  // the last matched leg
};

// The highest altitude in each 1% of the route.
class AltitudeArc {
public:
    static constexpr int kPieces = 100;

    void record(double share, double altFt);
    void reset();
    // kPieces entries: the highest altitude recorded in each piece, or -1 where none was.
    std::vector<double> peaks() const { return peaks_; }

private:
    std::vector<double> peaks_ = std::vector<double>(kPieces, -1.0);
};

// Altitude as a share of the plan's cruise altitude (35,000 ft without one), clamped to 0..1: the arc's colour.
double altitudeShare(double altFt, int cruiseAltitudeFt);

}  // namespace blocktime
