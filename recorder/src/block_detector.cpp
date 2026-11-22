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

std::optional<BlockEvent> BlockDetector::onSampleInFlight(const BlockSample&, bool, bool) { return std::nullopt; }

std::optional<FlightInterrupted> BlockDetector::onSessionEnded(std::string_view) { return std::nullopt; }

std::string_view eventType(const BlockEvent&) { return {}; }

nlohmann::json toJson(const BlockEvent&) { return {}; }

}  // namespace blocktime
