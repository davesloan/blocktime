// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The replayer (docs/replay.md, D-175): a settled flight's log lines, from the lead-in to block in, as a timeline the
// replay window plays at 1×, 2×, 3× or 10×. Each line keeps what the recorder handed the bridge on the day; the sim's
// paused stretches are taken out. Plain C++, read-only.

#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "block_detector.h"
#include "flight_events.h"
#include "sim_vars.h"

namespace blocktime {

// The day files a flight's lines are in: from the day before its block out (the lead-in can cross UTC midnight) on.
std::vector<std::filesystem::path> replayLogFiles(const std::filesystem::path& dir, std::string_view flightId);
// Whether the flight's block-out day is still in the log (the files are kept 30 days).
bool replayLogAvailable(const std::filesystem::path& dir, std::string_view flightId);

// What a line becomes in the replay: a sample, a block event, a flight event, or nothing (lines only the log got).
enum class ReplayRoute { Sample, Block, Flight, Skip };
ReplayRoute replayRoute(std::string_view type);

struct ReplayItem {
    std::int64_t offsetMs = 0;  // from the first line, the paused stretches taken out
    TimePoint ts{};             // when it was logged
    ReplayRoute route = ReplayRoute::Skip;
    std::string type;
    nlohmann::json data;        // null for samples, to save memory: the sample holds them
    TelemetrySample sample{};   // samples only
};

class ReplayTimeline {
public:
    ReplayTimeline() = default;
    // The flight's lines in log order, as loadFlightEvents gives them. Skip lines are dropped, and so is deboarding
    // before block out (the previous flight's); `paused` / `unpaused` shape the offsets first: a line inside a paused
    // stretch gets the stretch's start.
    explicit ReplayTimeline(std::vector<LogLine> lines);

    const std::vector<ReplayItem>& items() const { return items_; }
    bool empty() const { return items_.empty(); }
    std::int64_t durationMs() const { return items_.empty() ? 0 : items_.back().offsetMs; }
    // The flight's own time at an offset: the first line's time, plus the offset, plus the paused stretches before it.
    TimePoint timeAt(std::int64_t offsetMs) const;
    // Where `at` is: "block_out", "block_in", a phase ("climb", matched against phase_changed), or "H:MM" after
    // block out. nullopt when the flight doesn't have it.
    std::optional<std::int64_t> offsetOf(std::string_view at) const;

private:
    struct Pause {
        std::int64_t offsetMs;  // where it was skipped
        std::chrono::system_clock::duration length;
    };
    std::vector<ReplayItem> items_;
    std::vector<Pause> pauses_;
    TimePoint start_{};
};

// The flight's timeline from the log in `dir`, from 30 minutes before block out, or the previous flight's block in if
// that's later; empty when its lines aren't there (the log is gone).
ReplayTimeline loadReplay(const std::filesystem::path& dir, std::string_view flightId);

// The replay's position: advanced by the wall time since the last tick times the speed.
class ReplayClock {
public:
    void setSpeed(double speed) { speed_ = speed; }
    double speed() const { return speed_; }
    void setPaused(bool paused) { paused_ = paused; }
    bool paused() const { return paused_; }
    std::int64_t advance(std::chrono::milliseconds wall);
    void seek(std::int64_t offsetMs) { offset_ = offsetMs; }
    std::int64_t offset() const { return offset_; }

private:
    std::int64_t offset_ = 0;
    double speed_ = 1;
    bool paused_ = false;
};

}  // namespace blocktime
