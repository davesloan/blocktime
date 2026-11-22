// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "telemetry_filter.h"

#include <cmath>
#include <cstring>

namespace blocktime {

namespace {
constexpr double kMinAltitudeFt = -2000.0;
constexpr double kMaxAltitudeFt = 200000.0;
constexpr double kNullIslandToleranceDeg = 0.1;
constexpr double kNullIslandMaxGroundSpeedKt = 5.0;
}  // namespace

std::string_view toString(SampleProblem p) {
    switch (p) {
        case SampleProblem::None: return "none";
        case SampleProblem::SimNotRunning: return "sim_not_running";
        case SampleProblem::BadTitle: return "bad_title";
        case SampleProblem::BadAltitude: return "bad_altitude";
        case SampleProblem::NullIsland: return "position_0_0";
    }
    return "unknown";
}

std::string_view simString(const char* buf, std::size_t cap) {
    return {buf, strnlen(buf, cap)};
}

SampleProblem checkSample(const TelemetrySample& s, bool simRunning) {
    if (!simRunning) return SampleProblem::SimNotRunning;

    // Title must be terminated inside its buffer and contain at least one visible character.
    const std::size_t len = strnlen(s.title, sizeof(s.title));
    if (len == 0 || len == sizeof(s.title)) return SampleProblem::BadTitle;
    bool visible = false;
    for (std::size_t i = 0; i < len; ++i) {
        const auto c = static_cast<unsigned char>(s.title[i]);
        if (c < 0x20 && c != '\t') return SampleProblem::BadTitle;
        if (c > 0x20) visible = true;
    }
    if (!visible) return SampleProblem::BadTitle;

    for (double alt : {s.altitudeFt, s.indicatedAltitudeFt}) {
        if (!std::isfinite(alt) || alt < kMinAltitudeFt || alt > kMaxAltitudeFt) return SampleProblem::BadAltitude;
    }

    if (std::fabs(s.latitudeDeg) < kNullIslandToleranceDeg && std::fabs(s.longitudeDeg) < kNullIslandToleranceDeg &&
        s.groundSpeedKt < kNullIslandMaxGroundSpeedKt) {
        return SampleProblem::NullIsland;
    }
    return SampleProblem::None;
}

}  // namespace blocktime
