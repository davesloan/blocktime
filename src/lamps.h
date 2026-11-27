// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The in-flight panel's lamps (D-195, docs/dashboard.md "Lamps"): lit while the switch is on, and wrong (flashing)
// only inside the debrief's windows for that check, read through the debrief's own reading of the cockpit
// (cockpit_reading.h). A lamp whose data isn't in the log is dark and never wrong.

#pragma once

#include <array>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "cockpit_reading.h"
#include "sim_vars.h"

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

enum class Lamp { Beacon, Strobes, LandingLights, SeatBelts, Apu, AltStd, GearDown };  // the strip's order

struct LampState {
    Lamp lamp = Lamp::Beacon;
    bool known = false;  // its data is in the log; unknown lamps are dark and never wrong
    bool lit = false;    // the switch is on (ALT STD: both on STD; GEAR DN: down)
    bool wrong = false;  // inside one of its windows and in the wrong state: the lamp flashes
};

class LampBoard {
public:
    void reset(Family family);  // a new flight
    // phase_changed, flight_marker, cockpit, altimeter, liftoff, touchdown; anything else is ignored. The time is
    // data.time when the line has one (the takeoff is back-dated to the start of the roll), else `at`.
    void onEvent(std::string_view type, const nlohmann::json& data, TimePoint at);
    void onBlockOut(TimePoint at);
    void onBlockIn(TimePoint at);
    void onSample(const TelemetrySample& s, TimePoint at);  // the beacon, the strobes and engine starts
    std::array<LampState, 7> lamps(TimePoint now) const;

private:
    // When a switch changed (time, good): checks 1, 7, 12 and 13 stop at the first fix, as the debrief's secondsUntil.
    using Changes = std::vector<std::pair<TimePoint, bool>>;
    static void record(Changes& log, TimePoint at, bool value);
    // The first time at or after `from` the value was `want` (`from` itself when it already was); nullopt if never.
    static std::optional<TimePoint> firstAt(const Changes& log, TimePoint from, bool want);

    Changes beaconLog_, strobeLog_, apuLog_, stdLog_;
    Family family_ = Family::B737;
    bool pmdgSeen_ = false, hasSample_ = false, beacon_ = false, strobe_ = false, enginesRunning_ = false;
    std::optional<nlohmann::json> cockpit_, altimeter_;
    std::optional<TimePoint> engineStart_;  // the start after the engines were last all off, before block out
    std::optional<TimePoint> blockOut_, blockIn_, taxiOut_, takeoff_, liftoff_, descent_, touchdown_, taxiIn_;
    std::optional<TimePoint> transitionAltitude_, transitionLevel_, below10000_, finalApproach_, goAround_;
};

}  // namespace blocktime
