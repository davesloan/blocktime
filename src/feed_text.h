// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The event feed's short lines for what happens in a flight, without the time in front. The recorder prefixes
// "HH:MM:SS  " and shows them as they happen; the replay shows the same lines from the log (docs/replay.md).

#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "block_detector.h"

namespace blocktime {

// The line for a flight event worth seeing as it happens ("CLIMB", "LIFTOFF  14R", "ARRIVAL GATE  D14  (assigned)");
// "" for the rest.
std::string feedText(std::string_view type, const nlohmann::json& data);

// The line for a block event: BLOCK OUT, BLOCK IN, NOT RECORDING or FLIGHT INTERRUPTED.
std::string blockFeedText(const BlockEvent& event);

}  // namespace blocktime
