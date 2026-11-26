// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "lamps.h"

#include <algorithm>

#include "event_log.h"

namespace blocktime {

namespace {

// [from, to): open-ended while `to` hasn't happened.
bool inside(TimePoint now, std::optional<TimePoint> from, std::optional<TimePoint> to) {
    return from && now >= *from && (!to || now < *to);
}

}  // namespace

void LampBoard::reset(Family family) {
    *this = LampBoard{};
    family_ = family;
}

void LampBoard::onEvent(std::string_view type, const nlohmann::json& data, TimePoint at) {
    if (data.is_object() && data.contains("time") && data["time"].is_string()) {
        if (auto t = parseUtc(data["time"].get<std::string>())) at = *t;
    }
    if (type == "cockpit") {
        cockpit_ = data;
        pmdgSeen_ = pmdgSeen_ || data.value("pmdg", false);
    } else if (type == "altimeter") {
        altimeter_ = data;
    } else if (type == "liftoff") {
        liftoff_ = at;
    } else if (type == "touchdown") {
        touchdown_ = at;
    } else if (type == "phase_changed") {
        const std::string phase = data.value("phase", "");
        // The first taxi out, takeoff and descent, the last taxi in and go-around, as the debrief takes them.
        if (phase == "taxi_out" && !taxiOut_) taxiOut_ = at;
        else if (phase == "takeoff" && !takeoff_) takeoff_ = at;
        else if (phase == "descent" && !descent_) descent_ = at;
        else if (phase == "taxi_in") taxiIn_ = at;
        else if (phase == "go_around") goAround_ = at;
    } else if (type == "flight_marker") {
        const std::string marker = data.value("marker", "");
        if (marker == "transition_altitude" && !transitionAltitude_) transitionAltitude_ = at;
        else if (marker == "transition_level") transitionLevel_ = at;
        else if (marker == "below_10000") below10000_ = at;
        else if (marker == "final_approach") finalApproach_ = at;
    }
}

void LampBoard::onBlockOut(TimePoint at) { blockOut_ = at; }
void LampBoard::onBlockIn(TimePoint at) { blockIn_ = at; }

void LampBoard::onSample(const TelemetrySample& s, TimePoint at) {
    hasSample_ = true;
    beacon_ = s.lightBeacon != 0;
    strobe_ = s.lightStrobe != 0;
    bool running = false;
    for (int i = 0; i < std::clamp(s.numEngines, 0, 4); ++i) running = running || s.engCombustion[i] != 0;
    if (running && !enginesRunning_ && !blockOut_) engineStart_ = at;  // the debrief's flightStart()
    enginesRunning_ = running;
}

std::array<LampState, 7> LampBoard::lamps(TimePoint now) const {
    std::array<LampState, 7> out;
    for (int i = 0; i < 7; ++i) out[i].lamp = static_cast<Lamp>(i);

    // Beacon (check 1): off, from 10 s after the earlier of block out and the engine start, until block in.
    LampState& beacon = out[static_cast<int>(Lamp::Beacon)];
    beacon.known = hasSample_;
    beacon.lit = beacon_;
    if (blockOut_) {
        const TimePoint start = engineStart_ ? std::min(*engineStart_, *blockOut_) : *blockOut_;
        beacon.wrong = beacon.known && !beacon_ && inside(now, start + kBeaconTolerance, blockIn_);
    }

    // Strobes (checks 5 and 12): off from 10 s into the takeoff roll to liftoff; on from 180 s into taxi in.
    LampState& strobes = out[static_cast<int>(Lamp::Strobes)];
    strobes.known = hasSample_;
    strobes.lit = strobe_;
    const std::optional<TimePoint> rollGrace =
        takeoff_ ? std::optional<TimePoint>(*takeoff_ + kTakeoffLightsGrace) : std::nullopt;
    if (strobes.known) {
        strobes.wrong = (!strobe_ && inside(now, rollGrace, liftoff_)) ||
                        (strobe_ && taxiIn_ && inside(now, *taxiIn_ + kStrobesTolerance, blockIn_));
    }

    // The seat belts and landing lights need PMDG's data on the 737 (D-159).
    const bool readable = cockpit_ && beltsReadable(family_, pmdgSeen_);

    // Landing lights (checks 5 and 10): off from 10 s into the roll to liftoff; off from 60 s after the last
    // below_10000 (or the go-around after it, as the debrief) to touchdown.
    LampState& landing = out[static_cast<int>(Lamp::LandingLights)];
    landing.known = readable;
    if (readable) {
        landing.lit = landingLightsOn(*cockpit_);
        std::optional<TimePoint> approach;
        if (below10000_) {
            approach = *below10000_ + kLandingLightsDelay;
            if (goAround_ && *goAround_ > *approach) approach = goAround_;
        }
        const bool landed = touchdown_ && below10000_ && *touchdown_ > *below10000_;
        landing.wrong = !landing.lit && (inside(now, rollGrace, liftoff_) || (approach && !landed && now >= *approach));
    }

    // Seat belts (checks 2 and 8): off from taxi out to the transition altitude (else the descent, else the
    // touchdown, D-200); off from the last transition_level to block in.
    LampState& belts = out[static_cast<int>(Lamp::SeatBelts)];
    belts.known = readable;
    if (readable) {
        belts.lit = beltsOn(*cockpit_);
        const std::optional<TimePoint> climbEnd = transitionAltitude_ ? transitionAltitude_
                                                  : descent_          ? descent_
                                                                      : touchdown_;
        belts.wrong = !belts.lit && (inside(now, taxiOut_, climbEnd) || inside(now, transitionLevel_, blockIn_));
    }

    // APU (check 13): the selector on from the transition altitude to block in. Only PMDG's data has it (D-172).
    LampState& apu = out[static_cast<int>(Lamp::Apu)];
    if (cockpit_) {
        if (const auto on = apuSwitchOn(*cockpit_)) {
            apu.known = true;
            apu.lit = *on;
            apu.wrong = *on && inside(now, transitionAltitude_, blockIn_);
        }
    }

    // ALT STD (checks 7 and 9): not on STD from 60 s after the transition altitude to the transition level; still on
    // STD from 60 s after the last transition level to touchdown.
    LampState& alt = out[static_cast<int>(Lamp::AltStd)];
    alt.known = altimeter_.has_value();
    if (alt.known) {
        alt.lit = altimetersStd(*altimeter_);
        const std::optional<TimePoint> climb =
            transitionAltitude_ ? std::optional<TimePoint>(*transitionAltitude_ + kStdTolerance) : std::nullopt;
        const std::optional<TimePoint> descent =
            transitionLevel_ ? std::optional<TimePoint>(*transitionLevel_ + kDestinationBaroDelay) : std::nullopt;
        const bool landed = touchdown_ && transitionLevel_ && *touchdown_ > *transitionLevel_;
        alt.wrong = (!alt.lit && inside(now, climb, transitionLevel_)) || (alt.lit && descent && !landed && now >= *descent);
    }

    // Gear down (check 11): up from the last final_approach marker to touchdown; a go-around after it closes it.
    LampState& gear = out[static_cast<int>(Lamp::GearDown)];
    gear.known = cockpit_.has_value();
    if (gear.known) {
        gear.lit = gearDown(*cockpit_);
        const bool closed = (touchdown_ && finalApproach_ && *touchdown_ > *finalApproach_) ||
                            (goAround_ && finalApproach_ && *goAround_ > *finalApproach_);
        gear.wrong = !gear.lit && finalApproach_ && !closed && now >= *finalApproach_;
    }
    return out;
}

}  // namespace blocktime
