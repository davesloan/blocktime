// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Pre-Flight panel's clearance (D-153, D-163): the clearance and the taxi route read from ATC's spoken words, and
// the 30-minute countdown from the clearance. Plain C++, for the dashboard.

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

struct Clearance {
    std::string sid;                 // "PADRZ2"; "" when none is named
    std::string initialAltitude;     // "Climb via SID", "5,000 ft", "FL190"; "" when none is heard
    std::string departureFrequency;  // "119.6", as spoken
    std::string squawk;              // "3166"
    std::string runway;              // "Expect runway 8L" → "08L"; ""
    std::string expectLevel;         // "Expect FL400 one-zero minutes after departure" → "FL400"; ""
    int expectMinutes = 0;           // 10; 0 when not said
};

// ATC's words. nullopt unless they contain "cleared to" and a squawk.
std::optional<Clearance> parseClearance(std::string_view atcWords);

// The plan's full form "PADRZ2 IKAYE" when ATC's SID has the plan's name (or ATC named none, D-189); else ATC's own,
// and *changed is set.
std::string planSidForm(std::string_view atcSid, std::string_view planSid, std::string_view planTransition,
                        bool* changed);

struct TaxiInstruction {
    std::string runway;  // normalized, "27"
    std::string route;   // "A, A7, B, B1": everything after "via", without the final full stop
};

// nullopt unless the words name a runway, then "taxi", then "via".
std::optional<TaxiInstruction> parseTaxi(std::string_view atcWords);

inline constexpr std::chrono::minutes kClearanceValidity{30};  // D-153

struct Countdown {
    std::string text;  // "26:40"; "0:00" once expired
    bool urgent = false;   // 5 minutes or less left
    bool expired = false;
};

Countdown clearanceCountdown(TimePoint clearedAt, TimePoint now);

}  // namespace blocktime
