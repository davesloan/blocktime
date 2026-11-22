// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "block_detector.h"

#include <algorithm>

#include "event_log.h"
#include "geo.h"
#include "telemetry_filter.h"

namespace blocktime {

BlockSample toBlockSample(const TelemetrySample& t, TimePoint time) {
    BlockSample s;
    s.time = time;
    s.parkingBrake = t.parkingBrake != 0;
    s.onGround = t.onGround != 0;
    s.latDeg = t.latitudeDeg;
    s.lonDeg = t.longitudeDeg;
    s.altAglFt = t.altitudeAglFt;
    s.groundSpeedKt = t.groundSpeedKt;
    s.fuelLb = t.fuelTotalLb;
    s.fuelGal = t.fuelTotalGal;
    // Same engine count as the logged eng_running, so the log explains every decision.
    const int engines = std::clamp(t.numEngines, 0, 4);
    for (int i = 0; i < engines; ++i) s.enginesRunning += t.engCombustion[i] != 0 ? 1 : 0;
    s.atcId = simString(t.atcId, sizeof(t.atcId));
    s.title = simString(t.title, sizeof(t.title));
    return s;
}

std::optional<BlockEvent> BlockDetector::onSample(const BlockSample& s) {
    // A release or set is a change between two samples; the first sample only sets the baseline (D-042).
    const bool released = lastBrake_.has_value() && *lastBrake_ && !s.parkingBrake;
    const bool set = lastBrake_.has_value() && !*lastBrake_ && s.parkingBrake;
    lastBrake_ = s.parkingBrake;
    return open_ ? onSampleInFlight(s, released, set) : onSampleNoFlight(s, released);
}

// Block out (D-031): a release on the ground, confirmed by >= 10 m of movement with the brake still off.
std::optional<BlockEvent> BlockDetector::onSampleNoFlight(const BlockSample& s, bool released) {
    if (released && s.onGround) {
        outCandidate_ = s;
        return std::nullopt;
    }
    if (s.parkingBrake || !s.onGround) outCandidate_.reset();
    if (!outCandidate_) return std::nullopt;

    const BlockSample& c = *outCandidate_;
    const double moved = distanceMeters(c.latDeg, c.lonDeg, s.latDeg, s.lonDeg);
    if (moved < kBlockOutMinMoveM) return std::nullopt;

    BlockOut out;
    out.flightId = formatUtc(c.time);
    out.time = c.time;
    out.latDeg = c.latDeg;
    out.lonDeg = c.lonDeg;
    out.fuelLb = c.fuelLb;
    out.fuelGal = c.fuelGal;
    out.movedM = moved;
    out.atcId = c.atcId;
    out.title = c.title;

    open_ = out;
    outCandidate_.reset();
    airborne_ = false;
    landed_ = false;
    inCandidate_.reset();
    return out;
}

// Block in (D-032): after a landing, a set on the ground that stays set until every engine is off.
std::optional<BlockEvent> BlockDetector::onSampleInFlight(const BlockSample& s, bool released, bool set) {
    // SIM ON GROUND alone can flicker during taxi; also require a real height (D-042).
    if (!s.onGround && s.altAglFt >= kAirborneMinAglFt) airborne_ = true;
    if (airborne_ && s.onGround) landed_ = true;
    if (!landed_) return std::nullopt;

    if (set && s.onGround) inCandidate_ = s;
    else if (released || !s.onGround) inCandidate_.reset();
    if (!inCandidate_ || s.enginesRunning > 0) return std::nullopt;

    const BlockSample& c = *inCandidate_;
    BlockIn in;
    in.flightId = open_->flightId;
    in.time = c.time;
    in.latDeg = c.latDeg;
    in.lonDeg = c.lonDeg;
    in.fuelLb = c.fuelLb;
    in.fuelGal = c.fuelGal;
    in.shutdownTime = s.time;
    in.shutdownFuelLb = s.fuelLb;
    in.blockTimeS = std::chrono::duration_cast<std::chrono::seconds>(c.time - open_->time).count();

    open_.reset();
    inCandidate_.reset();
    airborne_ = false;
    landed_ = false;
    return in;
}

// Milestone 2 stopgap (D-044): nothing survives a lost session. Milestone 3 decides about resuming.
std::optional<FlightInterrupted> BlockDetector::onSessionEnded(std::string_view reason) {
    std::optional<FlightInterrupted> result;
    if (open_) result = FlightInterrupted{open_->flightId, std::string(reason)};
    *this = BlockDetector{};
    return result;
}

std::string_view eventType(const BlockEvent&) { return {}; }

nlohmann::json toJson(const BlockEvent&) { return {}; }

}  // namespace blocktime
