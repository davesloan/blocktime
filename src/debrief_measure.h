// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the debrief measures (docs/debrief.md sections 2-3, D-157, D-160): each check part's value, or why it couldn't
// be measured, and the landing's facts, from one flight's log lines (flight_events.h). Measured once, at settling, and
// stored; the rules that judge them are in debrief_score.h and run whenever they're shown. Pure logic.
//
// What each part measures. "State at t" is the latest line at or before t; "seconds off" sums the time a value was
// off inside a window; a value never reached counts to block in.
// - beacon_late_s: seconds from the earlier of block out and the engine start (the start after the engines were last
//   all off) until the beacon is on; 0 if it already was.
// - belts_off_departure_s: seat belt sign off from taxi out to the above_10000 marker (or the descent, or touchdown).
// - taxi_max_kt: the highest ground speed on the ground during taxi out and taxi in.
// - origin_baro_off_tenths: at the takeoff roll, the larger of the two altimeters' distance from the origin ATIS then
//   in force (or the one before it, whichever is closer), in tenths of hPa.
// - takeoff_strobes, takeoff_landing_lights: 1 if on at the takeoff roll.
// - over_250_s: seconds airborne below 10,000 ft indicated above 260 kt.
// - std_late_s: seconds from the transition_altitude marker until both altimeters read STD (1013.2 hPa).
// - belts_off_descent_s: seat belt sign off from the transition_level marker to block in.
// - destination_baro_off_tenths: as the origin's, 60 s after the transition_level marker, against destination ATIS.
// - landing_lights_off_s: landing lights off from the last below_10000 marker + 60 s to the touchdown.
// - stable_*: at the last final_approach marker: gear down, flaps at the FMC's landing setting, sink in fpm, IAS minus
//   VREF, and a lined_up since the last go-around.
// - strobes_on_after_landing_s: seconds from the start of taxi in until the strobes are off.
// - apu_on_after_transition_s: seconds from the transition_altitude marker until the APU selector is off (D-172).
// - apu_on_at_shutdown: 1 if the APU selector is on when the last engine stops at block in (D-173): Dave may switch
//   it on only once parked, so not at the brake set and not necessarily running yet.

#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "flight_events.h"

namespace blocktime {

enum class Family { B737, B777 };

inline constexpr std::array<const char*, 19> kCheckParts = {
    "beacon_late_s",          "belts_off_departure_s",       "taxi_max_kt",          "origin_baro_off_tenths",
    "takeoff_strobes",        "takeoff_landing_lights",      "over_250_s",           "std_late_s",
    "belts_off_descent_s",    "destination_baro_off_tenths", "landing_lights_off_s", "stable_gear_down",
    "stable_landing_flaps",   "stable_sink_fpm",             "stable_vref_offset_kt", "stable_lined_up",
    "strobes_on_after_landing_s", "apu_on_after_transition_s", "apu_on_at_shutdown"};

inline constexpr std::array<const char*, 5> kSkipReasons = {"no_pmdg_data", "no_vref", "no_atis", "no_marker",
                                                            "diverted"};

struct CheckValue {
    std::string part;
    int value = 0;
};
struct CheckSkip {
    std::string part, reason;
};

struct LandingFacts {
    int vsFpm = 0;        // the sink rate, positive
    int gHundredths = 0;  // 1.47 G is 147
    int bounces = 0;
    int iasKt = 0;
    int headingTrueDeg = 0, windDirDeg = 0, windKt = 0;  // the sim's, true
    // PLANE PITCH DEGREES as the sim gives it, ×10 (the SDK documents nose up as negative; debrief_score.h flips it,
    // so a wrong guess about the sign is fixed in one place and re-scores every flight). nullopt: not logged.
    std::optional<int> simPitchTenths;
    std::optional<int> bankTenths;                        // right wing down positive; nullopt: not logged
    std::optional<int> vrefKt;                            // nullopt: not set or no PMDG data
    std::optional<int> offCentrelineDm, offHeadingTenths;  // information only (D-122); nullopt: no runway named
};

struct FlightMeasurements {
    Family family = Family::B737;
    std::vector<CheckValue> values;
    std::vector<CheckSkip> skipped;       // every part of kCheckParts is in exactly one of the two
    std::optional<LandingFacts> landing;  // nullopt: no touchdown logged
};

FlightMeasurements measureFlight(const std::vector<LogLine>& lines, bool diverted);

// When the flight starts for the debrief: the engine start before block out (the start after the engines were last
// all off, since the lead-in can reach the previous leg), else block out. nullopt without a block_out.
std::optional<std::chrono::system_clock::time_point> flightStart(const std::vector<LogLine>& lines);

// The FMC's landing flaps detents in degrees, by the sim's FLAPS HANDLE INDEX (D-148), per family.
int flapDetentDegrees(Family family, int handleIndex);

}  // namespace blocktime
