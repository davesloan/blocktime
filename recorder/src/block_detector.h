// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Decides when a flight starts (block out, D-031) and ends (block in, D-032) from the parking brake.
// Pure logic: it is fed one valid telemetry sample at a time and returns at most one event per sample.
// The rules are in docs/recorder.md ("Flight lifecycle") and D-042. State is in memory only: a flight is
// never resumed (D-046).

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include <nlohmann/json.hpp>

#include "sim_vars.h"

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

// The fields of a telemetry sample that block detection needs.
struct BlockSample {
    TimePoint time{};
    bool parkingBrake = false;
    bool onGround = false;
    double latDeg = 0, lonDeg = 0, altAglFt = 0, groundSpeedKt = 0, fuelLb = 0, fuelGal = 0;
    int enginesRunning = 0;  // engines with combustion, out of clamp(NUMBER OF ENGINES, 0, 4)
    std::string atcId, title;
};

BlockSample toBlockSample(const TelemetrySample& s, TimePoint time);

struct BlockOut {
    std::string flightId;  // formatUtc(time)
    TimePoint time{};      // time of the brake release
    double latDeg = 0, lonDeg = 0, fuelLb = 0, fuelGal = 0;  // at the release
    double movedM = 0;     // distance from the release point when it was confirmed
    std::string atcId, title;
};

struct BlockIn {
    std::string flightId;
    TimePoint time{};      // time of the brake set
    double latDeg = 0, lonDeg = 0, fuelLb = 0, fuelGal = 0;  // at the set
    TimePoint shutdownTime{};  // when the last engine stopped (== time if they were already off)
    double shutdownFuelLb = 0;
    long long blockTimeS = 0;  // block in - block out, whole seconds
};

struct FlightInterrupted {
    std::string flightId;
    std::string reason;  // the sim session's lost reason, "recorder_exit" or "sim_stopped"
};

using BlockEvent = std::variant<BlockOut, BlockIn, FlightInterrupted>;

inline constexpr double kBlockOutMinMoveM = 10.0;   // D-031
inline constexpr double kAirborneMinAglFt = 100.0;  // D-042: well above the 8-14 ft PMDG reads on the ground

class BlockDetector {
public:
    std::optional<BlockEvent> onSample(const BlockSample& s);

    // The junk filter rejected a sample (main menu, loading screen). With no flight open, the next valid sample
    // only sets a new brake baseline, so loading a flight with the brake off isn't a release (D-045).
    void onInvalidSample();

    // The sim session ended. Reports an open flight as interrupted and forgets all state (D-044).
    std::optional<FlightInterrupted> onSessionEnded(std::string_view reason);

    // The sim's Sim state changed to stopped: Dave left the flight (main menu, restart, quit). Same as a lost
    // session, with reason "sim_stopped"; a flight is never resumed (D-046, D-047).
    std::optional<FlightInterrupted> onSimStopped();

    bool flightOpen() const { return open_.has_value(); }

private:
    std::optional<BlockEvent> onSampleNoFlight(const BlockSample& s, bool released);
    std::optional<BlockEvent> onSampleInFlight(const BlockSample& s, bool released, bool set);

    std::optional<bool> lastBrake_;          // brake state in the previous sample; empty = no baseline yet
    std::optional<BlockSample> outCandidate_;  // the release that may become block out
    std::optional<BlockOut> open_;           // the open flight
    bool airborne_ = false;
    bool landed_ = false;
    std::optional<BlockSample> inCandidate_;   // the set that may become block in
};

// Event type name for the log: "block_out", "block_in" or "flight_interrupted".
std::string_view eventType(const BlockEvent& e);

// The event's "data" object for the log (fields in docs/recorder.md, D-043).
nlohmann::json toJson(const BlockEvent& e);

}  // namespace blocktime
