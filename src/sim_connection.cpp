// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sim_connection.h"

#include <windows.h>

#include <tlhelp32.h>

#include <SimConnect.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "telemetry_filter.h"

namespace blocktime {

namespace {

using Clock = std::chrono::steady_clock;

// After this many failed SimConnect calls in a row, end the session and reconnect (D-028).
constexpr int kMaxConsecutiveErrors = 10;
// Ask the sim for its "Sim" state this often once connected.
constexpr auto kHeartbeatInterval = std::chrono::seconds(5);
// After this long with no messages, log sim_unresponsive (the session is kept; D-028).
constexpr auto kSilenceWarning = std::chrono::seconds(30);
// Only used when FlightSimulator.exe can't be found: how long to wait for OPEN before retrying.
constexpr auto kHandshakeTimeoutNoProcess = std::chrono::minutes(5);
// MSFS 2020 process name (assumed the same for Steam and MS Store installs; unverified for Steam).
constexpr wchar_t kSimExeName[] = L"FlightSimulator.exe";

enum DefinitionId : SIMCONNECT_DATA_DEFINITION_ID { DEF_TELEMETRY = 1 };

enum RequestId : SIMCONNECT_DATA_REQUEST_ID {
    REQ_TELEMETRY = 1,        // periodic, once a second, only on change
    REQ_SNAPSHOT = 2,         // one-off full sample (after sim start / aircraft load / unpause)
    REQ_SIM_STATE = 10,       // RequestSystemState("Sim")
    REQ_AIRCRAFT_LOADED = 11  // RequestSystemState("AircraftLoaded")
};

enum EventId : SIMCONNECT_CLIENT_EVENT_ID { EVT_SIM = 1, EVT_PAUSE = 2, EVT_AIRCRAFT_LOADED = 3 };

constexpr char kSnapshotLabel[] = "snapshot";

SIMCONNECT_DATATYPE toSimConnect(VarType t) {
    switch (t) {
        case VarType::Float64: return SIMCONNECT_DATATYPE_FLOAT64;
        case VarType::Int32: return SIMCONNECT_DATATYPE_INT32;
        case VarType::String256: return SIMCONNECT_DATATYPE_STRING256;
        case VarType::String64: return SIMCONNECT_DATATYPE_STRING64;
    }
    return SIMCONNECT_DATATYPE_INVALID;
}

const char* exceptionName(DWORD e) {
    switch (e) {
        case SIMCONNECT_EXCEPTION_NONE: return "NONE";
        case SIMCONNECT_EXCEPTION_ERROR: return "ERROR";
        case SIMCONNECT_EXCEPTION_SIZE_MISMATCH: return "SIZE_MISMATCH";
        case SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID: return "UNRECOGNIZED_ID";
        case SIMCONNECT_EXCEPTION_UNOPENED: return "UNOPENED";
        case SIMCONNECT_EXCEPTION_VERSION_MISMATCH: return "VERSION_MISMATCH";
        case SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED: return "NAME_UNRECOGNIZED";
        case SIMCONNECT_EXCEPTION_DATA_ERROR: return "DATA_ERROR";
        case SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE: return "INVALID_DATA_TYPE";
        case SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE: return "INVALID_DATA_SIZE";
        case SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS: return "OUT_OF_BOUNDS";
        case SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION: return "WEATHER_UNABLE_TO_GET_OBSERVATION";
        default: return "OTHER";
    }
}

HANDLE h(void* p) { return static_cast<HANDLE>(p); }

// Finds a running FlightSimulator.exe and opens it for "has it exited?" checks. Returns nullptr if not found.
HANDLE openSimProcess() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return nullptr;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    HANDLE proc = nullptr;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, kSimExeName) == 0) {
            proc = OpenProcess(SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (proc != nullptr) break;
        }
    }
    CloseHandle(snap);
    return proc;
}

// Lower-cases and trims an aircraft.cfg path to the part from "simobjects\" on, so the absolute path from the
// AircraftLoaded event and the relative one from RequestSystemState compare equal.
std::string normalizeAircraftPath(const std::string& raw) {
    std::string s = raw;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::replace(s.begin(), s.end(), '/', '\\');
    if (const auto pos = s.find("simobjects\\"); pos != std::string::npos) s.erase(0, pos);
    return s;
}

// Names for the Pause_EX1 bits, as listed in the SimConnect SDK and confirmed in the sim on 2026-11-22 (D-035):
// 1 = full pause (main menu, loading screens), 4 = active pause, 8 = sim pause (Escape menu).
// Bit 2 is the FSX legacy pause, documented as unused. Unknown bits are named "bit_<value>" so nothing is lost.
std::vector<std::string> pauseKinds(unsigned long flags) {
    struct Bit { unsigned long mask; const char* name; };
    static constexpr Bit kBits[] = {{1, "full"}, {2, "legacy"}, {4, "active"}, {8, "sim"}};
    std::vector<std::string> kinds;
    unsigned long rest = flags;
    for (const auto& b : kBits) {
        if (flags & b.mask) kinds.emplace_back(b.name);
        rest &= ~b.mask;
    }
    for (unsigned long mask = 1; rest != 0; mask <<= 1) {
        if (rest & mask) {
            kinds.push_back("bit_" + std::to_string(mask));
            rest &= ~mask;
        }
    }
    return kinds;
}

long long secondsSince(Clock::time_point t) {
    return std::chrono::duration_cast<std::chrono::seconds>(Clock::now() - t).count();
}

}  // namespace

SimConnection::SimConnection(EventLog& log, SampleCallback onSample, SimStateCallback onSimState, bool probe)
    : log_(log), onSample_(std::move(onSample)), onSimState_(std::move(onSimState)) {
    if (probe) probe_.emplace(log_);
}

void SimConnection::requestProbeFacilities(const std::vector<std::string>& icaos) {
    if (probe_ && handle_ != nullptr && handshake_) probe_->requestFacilities(handle_, icaos);
}

SimConnection::~SimConnection() { close("recorder_exit"); }

void SimConnection::markLost(const char* reason) {
    if (lost_) return;
    lost_ = true;
    lostReason_ = reason;
}

bool SimConnection::check(long hr, const char* label) {
    if (SUCCEEDED(hr)) {
        consecutiveErrors_ = 0;
        DWORD sendId = 0;
        if (SUCCEEDED(SimConnect_GetLastSentPacketID(h(handle_), &sendId))) {
            sent_[sentNext_] = {sendId, label};
            sentNext_ = (sentNext_ + 1) % sent_.size();
        }
        return true;
    }
    ++consecutiveErrors_;
    std::cerr << std::format("SimConnect call failed: {} (hr=0x{:08X}, {} in a row)\n", label,
                             static_cast<unsigned long>(hr), consecutiveErrors_);
    if (consecutiveErrors_ >= kMaxConsecutiveErrors) markLost("too_many_errors");
    return false;
}

const char* SimConnection::labelForSendId(unsigned long sendId) const {
    for (const auto& c : sent_) {
        if (c.label != nullptr && c.sendId == sendId) return c.label;
    }
    return nullptr;
}

bool SimConnection::open() {
    if (handle_ != nullptr) return true;
    HANDLE hs = nullptr;
    // No window, no event handle, config index 0 (same as Little Navmap). Fails quietly when MSFS isn't up.
    if (FAILED(SimConnect_Open(&hs, "BlockTime Recorder", nullptr, 0, nullptr, 0)) || hs == nullptr) return false;
    handle_ = hs;
    simProcess_ = openSimProcess();
    handshake_ = false;
    connectedLogged_ = false;
    simRunning_ = -1;
    pauseFlags_ = -1;
    lastAircraft_.clear();
    consecutiveErrors_ = 0;
    lost_ = false;
    lostReason_.clear();
    unresponsive_ = false;
    sent_ = {};
    sentNext_ = 0;
    lastMessage_ = lastHeartbeat_ = Clock::now();

    if (!setup()) {
        close("setup_failed");
        return false;
    }
    return true;
}

bool SimConnection::setup() {
    bool ok = true;
    for (const auto& v : kTelemetryVars) {
        ok &= check(SimConnect_AddToDataDefinition(h(handle_), DEF_TELEMETRY, v.name, v.units, toSimConnect(v.type),
                                                   v.epsilon),
                    v.name);
    }
    ok &= check(SimConnect_SubscribeToSystemEvent(h(handle_), EVT_SIM, "Sim"), "subscribe Sim");
    ok &= check(SimConnect_SubscribeToSystemEvent(h(handle_), EVT_PAUSE, "Pause_EX1"), "subscribe Pause_EX1");
    ok &= check(SimConnect_SubscribeToSystemEvent(h(handle_), EVT_AIRCRAFT_LOADED, "AircraftLoaded"),
                "subscribe AircraftLoaded");
    ok &= check(SimConnect_RequestDataOnSimObject(h(handle_), REQ_TELEMETRY, DEF_TELEMETRY, SIMCONNECT_OBJECT_ID_USER,
                                                  SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED),
                "telemetry request");
    ok &= check(SimConnect_RequestSystemState(h(handle_), REQ_SIM_STATE, "Sim"), "state Sim");
    ok &= check(SimConnect_RequestSystemState(h(handle_), REQ_AIRCRAFT_LOADED, "AircraftLoaded"),
                "state AircraftLoaded");
    // The probe's requests don't decide whether the session works: a failure is logged, not fatal (D-116).
    if (probe_ && !probe_->setup(handle_)) log_.write("warning", {{"message", "probe setup failed in part"}});
    return ok;
}

void SimConnection::requestSnapshot() {
    if (handle_ == nullptr) return;
    check(SimConnect_RequestDataOnSimObject(h(handle_), REQ_SNAPSHOT, DEF_TELEMETRY, SIMCONNECT_OBJECT_ID_USER,
                                            SIMCONNECT_PERIOD_ONCE),
          kSnapshotLabel);
}

bool SimConnection::simProcessGone() const {
    return simProcess_ != nullptr && WaitForSingleObject(h(simProcess_), 0) == WAIT_OBJECT_0;
}

void SimConnection::heartbeat() {
    const auto now = Clock::now();
    // Before the handshake MSFS is still starting: don't poke it, just wait for OPEN. Without a process to
    // watch we can't tell "still starting" from "died while starting", so give up after a generous wait.
    if (!handshake_) {
        if (simProcess_ == nullptr && now - lastMessage_ >= kHandshakeTimeoutNoProcess) markLost("handshake_timeout");
        return;
    }
    if (now - lastHeartbeat_ >= kHeartbeatInterval) {
        lastHeartbeat_ = now;
        check(SimConnect_RequestSystemState(h(handle_), REQ_SIM_STATE, "Sim"), "heartbeat");
    }
    if (!unresponsive_ && now - lastMessage_ >= kSilenceWarning) {
        unresponsive_ = true;
        log_.write("sim_unresponsive", {{"silent_s", secondsSince(lastMessage_)}});
    }
}

bool SimConnection::pump() {
    if (handle_ == nullptr) return false;
    if (probe_) probe_->tick();
    // Drain the queue before checking the process, so messages that already arrived aren't dropped.
    const bool processGone = simProcessGone();
    // GetNextDispatch returns E_FAIL when the queue is empty, so failures here are not counted as errors.
    for (int i = 0; i < 1000 && !lost_; ++i) {
        SIMCONNECT_RECV* msg = nullptr;
        DWORD size = 0;
        if (FAILED(SimConnect_GetNextDispatch(h(handle_), &msg, &size)) || msg == nullptr) break;
        if (unresponsive_) {
            unresponsive_ = false;
            log_.write("sim_responsive", {{"silent_s", secondsSince(lastMessage_)}});
        }
        lastMessage_ = Clock::now();
        handleMessage(msg, size);
    }
    if (processGone && !lost_) markLost("sim_process_gone");
    if (!lost_) heartbeat();
    return !lost_;
}

void SimConnection::handleMessage(const void* raw, unsigned long size) {
    const auto* msg = static_cast<const SIMCONNECT_RECV*>(raw);
    switch (msg->dwID) {
        case SIMCONNECT_RECV_ID_OPEN: {
            const auto* o = static_cast<const SIMCONNECT_RECV_OPEN*>(msg);
            handshake_ = true;
            connectedLogged_ = log_.write(
                "sim_connected",
                {{"app_name", std::string(simString(o->szApplicationName, sizeof(o->szApplicationName)))},
                 {"app_version", std::format("{}.{}.{}.{}", o->dwApplicationVersionMajor, o->dwApplicationVersionMinor,
                                             o->dwApplicationBuildMajor, o->dwApplicationBuildMinor)},
                 {"simconnect_version", std::format("{}.{}.{}.{}", o->dwSimConnectVersionMajor,
                                                    o->dwSimConnectVersionMinor, o->dwSimConnectBuildMajor,
                                                    o->dwSimConnectBuildMinor)},
                 {"recorder_version", BLOCKTIME_VERSION},
                 {"process_watch", simProcess_ != nullptr}});
            if (simProcess_ == nullptr) {
                log_.write("warning", {{"message", "FlightSimulator.exe not found; crash detection relies on "
                                                   "SimConnect errors only"}});
            }
            break;
        }
        case SIMCONNECT_RECV_ID_QUIT:
            markLost("sim_quit");
            break;

        case SIMCONNECT_RECV_ID_EXCEPTION: {
            const auto* e = static_cast<const SIMCONNECT_RECV_EXCEPTION*>(msg);
            const char* call = labelForSendId(e->dwSendID);
            // A snapshot sent before the user aircraft exists fails with UNRECOGNIZED_ID; harmless.
            const bool ignored =
                e->dwException == SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION ||
                (e->dwException == SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID && call != nullptr &&
                 std::strcmp(call, kSnapshotLabel) == 0);
            log_.write("simconnect_exception", {{"code", e->dwException},
                                                {"name", exceptionName(e->dwException)},
                                                {"call", call != nullptr ? call : "unknown"},
                                                {"send_id", e->dwSendID},
                                                {"index", e->dwIndex},
                                                {"ignored", ignored}});
            break;
        }
        case SIMCONNECT_RECV_ID_EVENT: {
            const auto* e = static_cast<const SIMCONNECT_RECV_EVENT*>(msg);
            if (e->uEventID == EVT_SIM) setSimRunning(e->dwData == 1);
            else if (e->uEventID == EVT_PAUSE) setPauseFlags(e->dwData);
            break;
        }
        case SIMCONNECT_RECV_ID_EVENT_FILENAME: {
            const auto* e = static_cast<const SIMCONNECT_RECV_EVENT_FILENAME*>(msg);
            if (e->uEventID == EVT_AIRCRAFT_LOADED) {
                logAircraft(std::string(simString(e->szFileName, sizeof(e->szFileName))));
                requestSnapshot();
            }
            break;
        }
        case SIMCONNECT_RECV_ID_SYSTEM_STATE: {
            const auto* s = static_cast<const SIMCONNECT_RECV_SYSTEM_STATE*>(msg);
            if (s->dwRequestID == REQ_SIM_STATE) {
                setSimRunning(s->dwInteger == 1);
            } else if (s->dwRequestID == REQ_AIRCRAFT_LOADED) {
                logAircraft(std::string(simString(s->szString, sizeof(s->szString))));
            }
            break;
        }
        case SIMCONNECT_RECV_ID_SIMOBJECT_DATA: {
            const auto* d = static_cast<const SIMCONNECT_RECV_SIMOBJECT_DATA*>(msg);
            if (probe_ && SimProbe::owns(d->dwRequestID)) {
                const std::size_t offset = offsetof(SIMCONNECT_RECV_SIMOBJECT_DATA, dwData);
                if (size > offset) probe_->onSimObjectData(d->dwRequestID, reinterpret_cast<const char*>(d) + offset,
                                                           size - offset);
                break;
            }
            if (d->dwRequestID != REQ_TELEMETRY && d->dwRequestID != REQ_SNAPSHOT) break;
            const std::size_t offset = offsetof(SIMCONNECT_RECV_SIMOBJECT_DATA, dwData);
            if (size < offset + sizeof(TelemetrySample)) {
                log_.write("simconnect_exception", {{"name", "SHORT_TELEMETRY_PACKET"}, {"size", size}});
                break;
            }
            TelemetrySample sample{};
            std::memcpy(&sample, reinterpret_cast<const char*>(d) + offset, sizeof(sample));
            if (onSample_) onSample_(sample, simRunning());
            break;
        }
        case SIMCONNECT_RECV_ID_CLIENT_DATA: {
            const auto* d = static_cast<const SIMCONNECT_RECV_CLIENT_DATA*>(msg);
            const std::size_t offset = offsetof(SIMCONNECT_RECV_CLIENT_DATA, dwData);
            if (probe_ && size > offset) {
                probe_->onClientData(d->dwRequestID, reinterpret_cast<const char*>(d) + offset, size - offset);
            }
            break;
        }
        case SIMCONNECT_RECV_ID_FACILITY_DATA:
            if (probe_) probe_->onFacilityData(msg);
            break;
        case SIMCONNECT_RECV_ID_FACILITY_DATA_END:
            if (probe_) probe_->onFacilityDataEnd(static_cast<const SIMCONNECT_RECV_FACILITY_DATA_END*>(msg)->RequestId);
            break;
        default:
            break;
    }
}

void SimConnection::logAircraft(const std::string& rawPath) {
    if (rawPath.empty()) return;
    std::string norm = normalizeAircraftPath(rawPath);
    if (norm == lastAircraft_) return;
    lastAircraft_ = std::move(norm);
    log_.write("aircraft_loaded", {{"path", rawPath}});
}

void SimConnection::setSimRunning(bool running) {
    const int v = running ? 1 : 0;
    if (simRunning_ == v) return;
    simRunning_ = v;
    log_.write(running ? "sim_running" : "sim_stopped");
    if (onSimState_) onSimState_(running);
    if (running) {
        check(SimConnect_RequestSystemState(h(handle_), REQ_AIRCRAFT_LOADED, "AircraftLoaded"), "state AircraftLoaded");
        requestSnapshot();
    }
}

void SimConnection::setPauseFlags(unsigned long flags) {
    const auto v = static_cast<long long>(flags);
    if (pauseFlags_ == v) return;
    const bool first = pauseFlags_ == -1;
    const bool wasPaused = pauseFlags_ > 0;
    pauseFlags_ = v;
    // Raw Pause_EX1 flags are logged alongside their names (D-030, D-035).
    if (v != 0) {
        log_.write("paused", {{"flags", v}, {"kinds", pauseKinds(flags)}});
    } else if (wasPaused && !first) {
        log_.write("unpaused");
        requestSnapshot();
    }
}

void SimConnection::close(const std::string& reason) {
    if (handle_ == nullptr) return;
    SimConnect_Close(h(handle_));
    handle_ = nullptr;
    if (simProcess_ != nullptr) {
        CloseHandle(h(simProcess_));
        simProcess_ = nullptr;
    }
    if (connectedLogged_) log_.write("sim_disconnected", {{"reason", reason}});
    connectedLogged_ = false;
    handshake_ = false;
}

}  // namespace blocktime
