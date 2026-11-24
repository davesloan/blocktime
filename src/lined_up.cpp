// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lined_up.h"

#include <cmath>
#include <numbers>

namespace blocktime {

namespace {

constexpr double kMetresPerDegree = 111320.0;
constexpr double kMetresPerNm = 1852.0;
constexpr double kDeg = std::numbers::pi / 180.0;

// (east, north) metres of b from a, flat; fine within the 10 nm this looks at.
struct Offset {
    double east = 0, north = 0;
};
Offset offset(double latA, double lonA, double latB, double lonB) {
    return {(lonB - lonA) * kMetresPerDegree * std::cos(latA * kDeg), (latB - latA) * kMetresPerDegree};
}

double angleDiff(double a, double b) {
    double d = std::fmod(std::abs(a - b), 360.0);
    return d > 180.0 ? 360.0 - d : d;
}

}  // namespace

LinedUpDetector::LinedUpDetector(std::vector<Runway> destinationRunways) {
    for (const Runway& r : destinationRunways) {
        // Each end's threshold is half the length back from the centre along its landing heading.
        const double half = r.lengthM / 2.0;
        const auto end = [&](int number, int designator, double headingDeg) {
            const double h = headingDeg * kDeg;
            const double lat = r.lat - half * std::cos(h) / kMetresPerDegree;
            const double lon = r.lon - half * std::sin(h) / (kMetresPerDegree * std::cos(r.lat * kDeg));
            ends_.push_back(End{runwayEndName(number, designator), std::fmod(headingDeg + 360.0, 360.0), lat, lon});
        };
        end(r.primaryNumber, r.primaryDesignator, r.headingDeg);
        end(r.secondaryNumber, r.secondaryDesignator, r.headingDeg + 180.0);
    }
}

std::optional<double> LinedUpDetector::track(const PhaseSample& s) const {
    // From the most recent earlier point at least kTrackBaseM away.
    for (auto it = recent_.rbegin(); it != recent_.rend(); ++it) {
        const Offset o = offset(it->lat, it->lon, s.lat, s.lon);
        if (std::hypot(o.east, o.north) >= kTrackBaseM) {
            return std::fmod(std::atan2(o.east, o.north) / kDeg + 360.0, 360.0);
        }
    }
    return std::nullopt;
}

std::optional<LinedUp> LinedUpDetector::onSample(const PhaseSample& s, Phase phase) {
    if (phase == Phase::GoAround) {
        fired_ = false;
        hold_.reset();
    }
    const bool looking = !fired_ && !s.onGround && (phase == Phase::Descent || phase == Phase::Approach);
    const std::optional<double> trk = looking ? track(s) : std::nullopt;
    recent_.push_back({s.lat, s.lon});
    if (recent_.size() > 30) recent_.pop_front();
    if (!trk) {
        hold_.reset();
        return std::nullopt;
    }

    const End* best = nullptr;
    double bestLateralM = kLinedUpOffCentrelineNm * kMetresPerNm;
    double bestBeforeM = 0;
    for (const End& e : ends_) {
        if (angleDiff(*trk, e.headingDeg) > kLinedUpOffHeadingDeg) continue;
        const Offset o = offset(e.lat, e.lon, s.lat, s.lon);
        const double h = e.headingDeg * kDeg;
        const double along = o.east * std::sin(h) + o.north * std::cos(h);  // > 0 past the threshold
        const double lateral = std::abs(o.east * std::cos(h) - o.north * std::sin(h));
        if (along > 0 || -along > kLinedUpRangeNm * kMetresPerNm) continue;
        if (lateral <= bestLateralM) {
            best = &e;
            bestLateralM = lateral;
            bestBeforeM = -along;
        }
    }
    if (best == nullptr) {
        hold_.reset();
        return std::nullopt;
    }
    if (!hold_ || hold_->runway != best->name) {
        hold_ = Hold{best->name, s.activeS,
                     LinedUp{best->name, s.time, static_cast<int>(std::lround(s.altFt)),
                             std::round(bestBeforeM / kMetresPerNm * 10.0) / 10.0}};
    }
    if (s.activeS - hold_->startActiveS < kLinedUpHoldS) return std::nullopt;
    fired_ = true;
    LinedUp out = hold_->at;
    hold_.reset();
    return out;
}

}  // namespace blocktime
