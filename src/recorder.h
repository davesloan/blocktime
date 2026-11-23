// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The recorder's main loop: connect to MSFS, log telemetry, detect block out / block in, end every flight (D-031,
// D-032, D-046..D-049). It knows nothing about consoles or windows: everything a user should see goes to a
// RecorderObserver, so the loop has no UI code in it (D-053).

#pragma once

#include <atomic>
#include <filesystem>
#include <string>

#include "block_detector.h"
#include "flight_match.h"
#include "sim_vars.h"

namespace blocktime {

enum class ConnectionState { WaitingForSim, SimStarting, Connected };

// Called on the thread that runs Recorder::run(). An implementation that hands things to another thread must copy
// what it is given.
class RecorderObserver {
public:
    virtual ~RecorderObserver() = default;
    // recorder_started was written; logging goes to logDir.
    virtual void onStarted(const std::filesystem::path& logDir) = 0;
    // A status or warning line. error = it is a LOST: line (an event that could never be written, D-049).
    virtual void onLine(const std::string& line, bool error) = 0;
    virtual void onConnection(ConnectionState state) = 0;
    // A valid telemetry sample, after it was logged.
    virtual void onTelemetry(const TelemetrySample& s) = 0;
    // A block event, after the outbox has taken it (written, or held for retry).
    virtual void onBlockEvent(const BlockEvent& e) = 0;
    // The matching check's answer for the current sample changed, while no flight is open (D-088). Also sent once
    // after each connect. The app shows "Not recording" from it.
    virtual void onMatch(const MatchResult& /*match*/) {}
};

struct RecorderOptions {
    bool probe = false;  // --probe: log what milestones 7-10 need to learn (D-116, sim_probe.h)
};

class Recorder {
public:
    // `target` is the planned flight, set by the app; every brake release is checked against it (D-088).
    Recorder(std::filesystem::path logDir, RecorderObserver& observer, const FlightTargetSlot& target,
             RecorderOptions options = {});

    // Runs until `stop` is true, then closes cleanly: ends the sim session (an open flight is interrupted with
    // reason recorder_exit), flushes held block events and writes recorder_stopped. Returns 0, or 1 if
    // recorder_started couldn't be written.
    int run(const std::atomic<bool>& stop);

    const std::filesystem::path& logDir() const { return logDir_; }

private:
    std::filesystem::path logDir_;
    RecorderObserver& observer_;
    const FlightTargetSlot& target_;
    RecorderOptions options_;
};

// Engines with combustion, out of clamp(NUMBER OF ENGINES, 0, 4).
int enginesRunning(const TelemetrySample& s);

}  // namespace blocktime
