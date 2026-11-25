// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The telemetry line's logged form, both ways: the recorder writes it, the replay reads it back (docs/replay.md).
// Block events are read back here too; block_detector.h writes them.

#pragma once

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include "block_detector.h"
#include "sim_vars.h"

namespace blocktime {

// The `telemetry` line's data.
nlohmann::json toJson(const TelemetrySample& s);
// Its inverse. Fields the log doesn't have (per-engine fuel flow, the cockpit fields; anything an older log lacks)
// are zero.
TelemetrySample telemetryFromJson(const nlohmann::json& data);

// A logged `block_out` or `block_in` line read back. nullopt for any other type, or when its flight_id or time is
// missing or doesn't parse.
std::optional<BlockEvent> blockEventFromJson(std::string_view type, const nlohmann::json& data);

}  // namespace blocktime
