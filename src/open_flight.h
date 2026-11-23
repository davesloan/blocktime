// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Finds a flight that a killed recorder (or a power cut) left open in the event log, so the next start can
// close it as interrupted (D-048). A flight is never resumed (D-046).

#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>

namespace blocktime {

inline constexpr std::size_t kOpenFlightLookbackFiles = 7;  // D-048: about a week of daily log files

// Looks at events-YYYY-MM-DD.jsonl files in `dir`, newest first, at most `maxFiles` of them. In the newest file that
// has any block_out, block_in or flight_interrupted line, the last such line decides: a block_out means that flight
// is still open, and its flight_id is returned. Lines that don't parse (a torn last line, D-036) are skipped.
std::optional<std::string> findOpenFlight(const std::filesystem::path& dir,
                                          std::size_t maxFiles = kOpenFlightLookbackFiles);

}  // namespace blocktime
