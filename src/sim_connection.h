// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thin wrapper around one SimConnect session. Single-threaded: the caller calls pump() regularly
// (D-023). Connection-level events (sim_connected, sim_running, paused, ...) are written to the
// EventLog here; telemetry samples are handed to the caller through the onSample callback.
//
// Session lifecycle (D-028):
//   open()  -> pipe to MSFS exists, but MSFS may still be starting and not answering yet
//   OPEN    -> handshake complete: sim_connected is logged
//   lost    -> QUIT, FlightSimulator.exe exited, or too many failed calls; sim_disconnected is logged
//              only if sim_connected was logged for this session
// Silence alone never ends a session; it only logs sim_unresponsive / sim_responsive.

#pragma once

#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <optional>
#include <vector>
#include <string>

#include "active_clock.h"
#include "event_log.h"
#include "flight_tracker.h"
#include "gsx_watcher.h"
#include "runway_match.h"
#include "sim_probe.h"
#include "sim_vars.h"
#include "touchdown_capture.h"

namespace blocktime {

class SimConnection {
public:
    using SampleCallback = std::function<void(const TelemetrySample&, bool simRunning)>;
    // Called when the sim's "Sim" state changes, right after sim_running / sim_stopped is logged.
    using SimStateCallback = std::function<void(bool running)>;
    // Milestone 7's requests (D-123, D-125, D-126): sim-frame touchdown samples, the altimeter settings, an airport's
    // runways.
    using FastSampleCallback = std::function<void(const FastSample&)>;
    using AltimeterCallback = std::function<void(const AltimeterSample&)>;
    using RunwaysCallback = std::function<void(AirportRunways)>;
    using GsxCallback = std::function<void(const GsxValues&)>;  // milestone 8 (D-138)

    // `probe`: also run the --probe data requests (D-116, sim_probe.h).
    SimConnection(EventLog& log, SampleCallback onSample, SimStateCallback onSimState = {}, bool probe = false);
    ~SimConnection();
    SimConnection(const SimConnection&) = delete;
    SimConnection& operator=(const SimConnection&) = delete;

    // Tries to open SimConnect. Returns false (quietly) if the sim isn't running.
    bool open();

    // Drains all pending messages. Returns false if the session was lost; the caller should then
    // close() and go back to retrying.
    bool pump();

    // Closes the handle. Logs sim_disconnected (with the reason) only if sim_connected was logged.
    void close(const std::string& reason);

    bool isOpen() const { return handle_ != nullptr; }
    bool handshakeComplete() const { return handshake_; }
    bool simRunning() const { return simRunning_ == 1; }
    bool paused() const { return pauseFlags_ > 0; }
    const std::string& lostReason() const { return lostReason_; }
    // The loaded aircraft.cfg's path, lower-cased from "simobjects\" on; "" until the sim says (D-112).
    const std::string& aircraftPath() const { return lastAircraft_; }
    // With --probe: logs these airports' runways and parking spots from the sim's facility data.
    void requestProbeFacilities(const std::vector<std::string>& icaos);

    void setFlightCallbacks(FastSampleCallback onFast, AltimeterCallback onAltimeter, RunwaysCallback onRunways,
                            GsxCallback onGsx);
    // Asks for each airport's runways; each list goes to the runways callback when it's complete. False (nothing
    // sent) before the handshake.
    bool requestRunways(const std::vector<std::string>& icaos);
    // Sim-frame touchdown samples on or off. Only calls SimConnect when the state changes.
    void setFastSampling(bool on);
    // Seconds that count toward phase holds: every message received adds at most 2 s (D-058).
    double activeSeconds() const { return clock_.seconds(); }

private:
    bool setup();
    void requestSnapshot();
    void heartbeat();
    bool check(long hr, const char* label);
    void markLost(const char* reason);
    const char* labelForSendId(unsigned long sendId) const;
    void handleMessage(const void* msg, unsigned long size);
    void logAircraft(const std::string& rawPath);
    void setSimRunning(bool running);
    void setPauseFlags(unsigned long flags);
    bool simProcessGone() const;

    void* handle_ = nullptr;         // SimConnect HANDLE (void* keeps windows.h out of this header)
    void* simProcess_ = nullptr;     // process HANDLE of FlightSimulator.exe, if found
    EventLog& log_;
    SampleCallback onSample_;
    SimStateCallback onSimState_;
    std::optional<SimProbe> probe_;
    FastSampleCallback onFast_;
    AltimeterCallback onAltimeter_;
    RunwaysCallback onRunways_;
    GsxCallback onGsx_;
    ActiveClock clock_;
    bool fastOn_ = false;
    double baroMb_[2] = {0, 0};
    bool baroStd_[2] = {false, false};
    std::map<unsigned long, AirportRunways> runwayRequests_;  // by request id, until the data ends
    unsigned long nextRunwayRequest_ = 0;

    bool handshake_ = false;         // SIMCONNECT_RECV_ID_OPEN received
    bool connectedLogged_ = false;   // sim_connected written for this session
    int simRunning_ = -1;            // -1 unknown, 0 no, 1 yes
    long long pauseFlags_ = -1;      // raw Pause_EX1 flags; -1 unknown
    std::string lastAircraft_;       // normalized aircraft.cfg path last logged
    int consecutiveErrors_ = 0;
    bool lost_ = false;
    std::string lostReason_;
    bool unresponsive_ = false;

    struct SentCall {
        unsigned long sendId = 0;
        const char* label = nullptr;
    };
    std::array<SentCall, 32> sent_{};  // ring buffer: SimConnect send ID -> which call it was
    std::size_t sentNext_ = 0;

    std::chrono::steady_clock::time_point lastMessage_{};
    std::chrono::steady_clock::time_point lastHeartbeat_{};
};

}  // namespace blocktime
