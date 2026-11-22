// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Rejects the junk samples MSFS sends while loading / at the main menu (D-025,
// docs/notes/msfs-junk-data.md). Thresholds follow Little Navmap's atools (reference only, no code copied).

#pragma once

#include <cstddef>
#include <string_view>

#include "sim_vars.h"

namespace blocktime {

enum class SampleProblem {
    None,
    SimNotRunning,
    BadTitle,
    BadAltitude,
    NullIsland,  // parked at 0/0
};

std::string_view toString(SampleProblem p);

// Returns the first problem found, or SampleProblem::None if the sample can be trusted.
SampleProblem checkSample(const TelemetrySample& s, bool simRunning);

// View of a fixed-size, possibly unterminated sim string.
std::string_view simString(const char* buf, std::size_t cap);

}  // namespace blocktime
