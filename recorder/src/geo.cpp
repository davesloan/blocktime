// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "geo.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace blocktime {

double distanceMeters(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg) {
    constexpr double kEarthRadiusM = 6371000.0;
    constexpr double kRad = std::numbers::pi / 180.0;
    const double dLat = (lat2Deg - lat1Deg) * kRad;
    const double dLon = (lon2Deg - lon1Deg) * kRad;
    const double a = std::sin(dLat / 2) * std::sin(dLat / 2) +
                     std::cos(lat1Deg * kRad) * std::cos(lat2Deg * kRad) * std::sin(dLon / 2) * std::sin(dLon / 2);
    return 2 * kEarthRadiusM * std::asin(std::sqrt(std::clamp(a, 0.0, 1.0)));
}

}  // namespace blocktime
