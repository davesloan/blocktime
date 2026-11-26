// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "route_progress.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

#include "geo.h"

namespace blocktime {
namespace {

constexpr double kMetresPerNm = 1852.0;
constexpr std::size_t kLegsAhead = 3;  // how far past the last matched leg a position may match (D-182)

struct Projection {
    double t = 0;         // along the leg, 0..1
    double offNm = 0;     // distance from the leg
};

// `p` projected onto the leg a → b in a flat frame centred on a, in NM. Legs are short enough for this.
Projection project(const RoutePoint& a, const RoutePoint& b, double lat, double lon) {
    const double k = std::cos(a.lat * std::numbers::pi / 180.0) * 60.0;
    const double bx = (b.lon - a.lon) * k, by = (b.lat - a.lat) * 60.0;
    const double px = (lon - a.lon) * k, py = (lat - a.lat) * 60.0;
    const double len2 = bx * bx + by * by;
    const double t = len2 > 0 ? std::clamp((px * bx + py * by) / len2, 0.0, 1.0) : 0.0;
    return {t, std::hypot(px - t * bx, py - t * by)};
}

}  // namespace

RouteProgress::RouteProgress(std::vector<RoutePoint> route) : route_(std::move(route)) {
    if (route_.size() < 2) return;
    for (std::size_t i = 0; i + 1 < route_.size(); ++i) {
        const RoutePoint& a = route_[i];
        const RoutePoint& b = route_[i + 1];
        startNm_.push_back(totalNm_);
        legNm_.push_back(distanceMeters(a.lat, a.lon, b.lat, b.lon) / kMetresPerNm);
        totalNm_ += legNm_.back();
    }
}

void RouteProgress::update(double lat, double lon) {
    if (legNm_.empty()) return;
    const std::size_t last = std::min(legNm_.size() - 1, leg_ + kLegsAhead);
    double best = std::numeric_limits<double>::max();
    std::size_t bestLeg = leg_;
    double bestAlong = 0;
    for (std::size_t i = leg_; i <= last; ++i) {
        const Projection p = project(route_[i], route_[i + 1], lat, lon);
        if (p.offNm < best) {
            best = p.offNm;
            bestLeg = i;
            bestAlong = startNm_[i] + p.t * legNm_[i];
        }
    }
    leg_ = bestLeg;
    flownNm_ = std::max(flownNm_, bestAlong);
}

void RouteProgress::finish() {
    flownNm_ = totalNm_;
    if (!legNm_.empty()) leg_ = legNm_.size() - 1;
}

void RouteProgress::reset() {
    flownNm_ = 0;
    leg_ = 0;
}

void AltitudeArc::record(double share, double altFt) {
    const int piece = std::clamp(static_cast<int>(share * kPieces), 0, kPieces - 1);
    peaks_[static_cast<std::size_t>(piece)] = std::max(peaks_[static_cast<std::size_t>(piece)], altFt);
}

void AltitudeArc::reset() { std::fill(peaks_.begin(), peaks_.end(), -1.0); }

double altitudeShare(double altFt, int cruiseAltitudeFt) {
    const double cruise = cruiseAltitudeFt > 0 ? cruiseAltitudeFt : 35000.0;
    return std::clamp(altFt / cruise, 0.0, 1.0);
}

}  // namespace blocktime
