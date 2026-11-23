// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Small geometry helpers on the WGS-84 sphere approximation.

#pragma once

namespace blocktime {

// Great-circle distance in metres between two lat/lon points (haversine, Earth radius 6,371,000 m).
double distanceMeters(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg);

}  // namespace blocktime
