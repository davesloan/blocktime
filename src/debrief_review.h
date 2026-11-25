// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Claude's review of a flight (D-156, D-161, docs/debrief.md section 5): the Messages API request built from the
// flight's log lines, checks and landing, and the answer read back. No network here: ClaudeReviewer (the app) and
// blocktime-debrief send the body. Pure logic.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "debrief_measure.h"
#include "debrief_store.h"
#include "flight_events.h"

namespace blocktime {

inline constexpr const char* kReviewModel = "claude-opus-5-5";
inline constexpr const char* kReviewUrl = "https://api.anthropic.com/v1/messages";
inline constexpr const char* kAnthropicVersion = "2026-10-21";
inline constexpr const char* kFallbackBeta = "server-side-fallback-2026-07-01";

struct ReviewInput {
    std::string header;  // "KLAS → KSAN · SOX8 · N814SX (PMDG 737-800BBJ2) · out 23:53Z in 00:52Z"
    std::vector<LogLine> events;
    FlightMeasurements measurements;
    std::vector<std::vector<std::string>> recentMisses;  // the last five flights' missed check labels, newest first
};

// The flight as one line per event: phases, markers, cockpit and altimeter changes, the touchdown, and lines condensed
// from the telemetry (beacon, strobes, engines, the state at the 1,000 ft marker, the sink from it to touchdown).
std::string buildTimeline(const std::vector<LogLine>& events);

// The JSON body of the Messages API request.
std::string buildReviewRequest(const ReviewInput& in);

struct ReviewAnswer {
    std::optional<StoredReview> review;  // writtenAt left empty: the caller stamps it
    std::string error;                   // why there's no review; quotes no key
};
ReviewAnswer parseReviewResponse(int httpStatus, std::string_view body);

}  // namespace blocktime
