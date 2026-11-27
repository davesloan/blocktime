// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading a screenshot of an airplane's SimBrief airframe page with Claude (D-242, docs/fleet.md): the Messages API
// request with the image and a fixed JSON shape for the answer, and the answer read back into a SimbriefReading. No
// network here: ClaudeReviewer (the app) and blocktime-airframe send the body. Pure logic.

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "fleet_rules.h"

namespace blocktime {

// The JSON body of the Messages API request: the image (its bytes and media type, "image/png" or "image/jpeg") and a
// prompt naming each SimBrief label, with structured output so the answer is one object.
std::string buildAirframeReadRequest(std::string_view imageBytes, std::string_view mediaType);

struct AirframeReadAnswer {
    std::optional<SimbriefReading> reading;
    std::string error;  // why there's no reading; quotes no key
};
// Reads Claude's answer. A field Claude couldn't see is left unset. Weights other than LBS are refused.
AirframeReadAnswer parseAirframeReadResponse(int httpStatus, std::string_view body);

}  // namespace blocktime
