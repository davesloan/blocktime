// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one reading of the logged cockpit, altimeter and telemetry lines (D-159). The debrief's measurements and the
// in-flight panel's lamps both use it, so a lamp flashes exactly when the debrief would mark the check down (D-195).

#pragma once

#include <chrono>
#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

namespace blocktime {

enum class Family { B737, B777 };
// "777" anywhere (an aircraft folder like "PMDG 777-200ER", or the catalog's family) → B777, else B737.
Family familyOf(std::string_view aircraftFolderOrFamily);

bool beltsOn(const nlohmann::json& cockpit);           // seat_belts 1 (auto) or 2 (on)
bool landingLightsOn(const nlohmann::json& cockpit);   // landing_lights
bool gearDown(const nlohmann::json& cockpit);          // gear_down
std::optional<bool> apuSwitchOn(const nlohmann::json& cockpit);  // nullopt without apu_switch (no PMDG data, D-172)
bool altimetersStd(const nlohmann::json& altimeter);   // capt_mb and fo_mb both within 0.05 of 1013.2
// The seat belts and the 737's landing lights need PMDG's data (D-159): readable on a 777, or once a cockpit line
// with pmdg: true was seen.
bool beltsReadable(Family family, bool pmdgSeen);

// The windows' times, shared by debrief_measure.cpp, debrief_score.cpp and the lamps (docs/debrief.md section 2).
inline constexpr std::chrono::seconds kBeaconTolerance{10};       // the score's "within 10 s"
inline constexpr std::chrono::seconds kTakeoffLightsGrace{10};    // into the takeoff roll
inline constexpr std::chrono::seconds kStdTolerance{60};          // std_late_s passes within 60 s
inline constexpr std::chrono::seconds kDestinationBaroDelay{60};  // set within 60 s of transition_level (D-254)
inline constexpr std::chrono::seconds kLandingLightsDelay{60};    // after below_10000
inline constexpr std::chrono::seconds kStrobesTolerance{180};     // after taxi in starts
inline constexpr int kBaroToleranceTenths = 10;                    // an altimeter within 1 hPa of the ATIS

}  // namespace blocktime
