// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Milestone 1: connect to MSFS, read telemetry, log it. See docs/recorder.md and docs/progress.md.

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <format>
#include <iostream>
#include <string>
#include <thread>

#include "event_log.h"
#include "sim_connection.h"
#include "telemetry_filter.h"

using namespace blocktime;
using namespace std::chrono_literals;

namespace {

std::atomic<bool> g_stop{false};

BOOL WINAPI onConsoleCtrl(DWORD type) {
    g_stop = true;
    // For window close / logoff / shutdown Windows kills us when this returns; give main a moment to close cleanly.
    if (type == CTRL_CLOSE_EVENT || type == CTRL_LOGOFF_EVENT || type == CTRL_SHUTDOWN_EVENT) Sleep(1500);
    return TRUE;
}

// Reconnect back-off (D-024): 10 s, 20 s after 30 min of waiting, 40 s after 60 min.
std::chrono::seconds retryInterval(std::chrono::steady_clock::duration waited) {
    if (waited >= 60min) return 40s;
    if (waited >= 30min) return 20s;
    return 10s;
}

// Sleeps in small steps so Ctrl+C stays responsive.
void sleepFor(std::chrono::milliseconds total) {
    const auto until = std::chrono::steady_clock::now() + total;
    while (!g_stop && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(100ms);
}

void printUsage() {
    std::cout << "BlockTime Recorder " BLOCKTIME_VERSION "\n"
                 "Usage: blocktime-recorder [--log-dir <folder>]\n"
                 "  --log-dir   where events-YYYY-MM-DD.jsonl files go\n"
                 "              (default: %LOCALAPPDATA%\\BlockTime\\events)\n";
}

int engineCount(const TelemetrySample& s) { return std::clamp(s.numEngines, 0, 4); }

nlohmann::json engRunning(const TelemetrySample& s) {
    nlohmann::json a = nlohmann::json::array();
    for (int i = 0; i < engineCount(s); ++i) a.push_back(s.engCombustion[i] != 0);
    return a;
}

double totalFuelFlow(const TelemetrySample& s) {
    double sum = 0;
    for (int i = 0; i < engineCount(s); ++i) sum += s.engFuelFlowPph[i];
    return sum;
}

std::string engString(const TelemetrySample& s) {
    std::string out;
    for (int i = 0; i < engineCount(s); ++i) out += s.engCombustion[i] ? '1' : '0';
    return out.empty() ? "-" : out;
}

nlohmann::json toJson(const TelemetrySample& s) {
    return {
        {"parking_brake", s.parkingBrake != 0},
        {"on_ground", s.onGround != 0},
        {"fuel_lb", s.fuelTotalLb},
        {"fuel_gal", s.fuelTotalGal},
        {"lat", s.latitudeDeg},
        {"lon", s.longitudeDeg},
        {"alt_ft", s.altitudeFt},
        {"alt_agl_ft", s.altitudeAglFt},
        {"alt_ind_ft", s.indicatedAltitudeFt},
        {"vs_fpm", s.verticalSpeedFpm},
        {"gs_kt", s.groundSpeedKt},
        {"ias_kt", s.indicatedAirspeedKt},
        {"engines", s.numEngines},
        {"eng_running", engRunning(s)},
        {"fuel_flow_pph", totalFuelFlow(s)},
        {"title", std::string(simString(s.title, sizeof(s.title)))},
        {"atc_id", std::string(simString(s.atcId, sizeof(s.atcId)))},
    };
}

// Turns raw samples into telemetry / telemetry_invalid events, logging invalid only on change.
class TelemetryLogger {
public:
    explicit TelemetryLogger(EventLog& log) : log_(log) {}

    void onSample(const TelemetrySample& s, bool simRunning) {
        const SampleProblem p = checkSample(s, simRunning);
        if (p != SampleProblem::None) {
            ++skipped_;
            if (p != lastProblem_) {
                log_.write("telemetry_invalid", {{"reason", toString(p)}});
                std::cout << std::format("{}  telemetry_invalid  {}\n", utcTimestamp().substr(11, 8), toString(p));
            }
            lastProblem_ = p;
            hasLast_ = false;  // always log the first good sample after an invalid stretch
            return;
        }
        // Skip a sample identical to the last one logged (snapshot + periodic often coincide; D-029).
        if (hasLast_ && std::memcmp(&s, &last_, sizeof(s)) == 0) return;
        last_ = s;
        hasLast_ = true;
        nlohmann::json data = toJson(s);
        if (skipped_ > 0) {
            data["skipped_invalid"] = skipped_;
            skipped_ = 0;
        }
        lastProblem_ = SampleProblem::None;
        log_.write("telemetry", data);
        std::cout << std::format("{}  {:<8} brk={} gnd={} fuel={:.0f}lb ff={:.0f} eng={} alt={:.0f}ft agl={:.0f}"
                                 " vs={:.0f} gs={:.0f} ias={:.0f} {:.5f},{:.5f}\n",
                                 utcTimestamp().substr(11, 8), simString(s.atcId, sizeof(s.atcId)),
                                 s.parkingBrake ? "SET" : "off", s.onGround ? 1 : 0, s.fuelTotalLb, totalFuelFlow(s),
                                 engString(s), s.altitudeFt, s.altitudeAglFt, s.verticalSpeedFpm, s.groundSpeedKt,
                                 s.indicatedAirspeedKt, s.latitudeDeg, s.longitudeDeg);
    }

    void reset() {
        lastProblem_ = SampleProblem::None;
        skipped_ = 0;
        hasLast_ = false;
    }

private:
    EventLog& log_;
    SampleProblem lastProblem_ = SampleProblem::None;
    long long skipped_ = 0;
    TelemetrySample last_{};
    bool hasLast_ = false;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
    std::filesystem::path logDir = EventLog::defaultDir();
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (a == L"--log-dir" && i + 1 < argc) {
            logDir = argv[++i];
        } else if (a == L"--help" || a == L"-h" || a == L"/?") {
            printUsage();
            return 0;
        } else {
            std::wcerr << L"Unknown argument: " << a << L"\n";
            printUsage();
            return 2;
        }
    }

    SetConsoleOutputCP(CP_UTF8);
    std::cout << std::unitbuf;  // keep console/redirected output current even if we're killed
    SetConsoleCtrlHandler(onConsoleCtrl, TRUE);

    // Only one recorder at a time; a second one would write every event twice (D-037). The handle is held until
    // the process exits.
    if (CreateMutexW(nullptr, FALSE, L"Local\\BlockTimeRecorder") != nullptr && GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cerr << "BlockTime Recorder is already running. Close the other window first.\n";
        return 3;
    }

    EventLog log(logDir);
    if (!log.write("recorder_started", {{"version", BLOCKTIME_VERSION}})) return 1;
    std::cout << "BlockTime Recorder " BLOCKTIME_VERSION "\nLogging to " << log.dir().string()
              << "\nPress Ctrl+C to stop.\n";

    TelemetryLogger telemetry(log);
    SimConnection sim(log, [&](const TelemetrySample& s, bool running) { telemetry.onSample(s, running); });

    auto waitingSince = std::chrono::steady_clock::now();
    bool announcedWaiting = false;
    bool announcedConnected = false;

    while (!g_stop) {
        if (!sim.isOpen()) {
            if (sim.open()) {
                std::cout << "Sim found, waiting for it to finish starting...\n";
                announcedWaiting = false;
                announcedConnected = false;
                telemetry.reset();
            } else {
                if (!announcedWaiting) {
                    std::cout << "Waiting for sim...\n";
                    announcedWaiting = true;
                }
                sleepFor(retryInterval(std::chrono::steady_clock::now() - waitingSince));
                continue;
            }
        }

        const bool alive = sim.pump();
        if (sim.handshakeComplete() && !announcedConnected) {
            std::cout << "Connected to sim.\n";
            announcedConnected = true;
        }
        if (!alive) {
            std::cout << (announcedConnected ? "Disconnected (" : "Gave up waiting for sim (") << sim.lostReason()
                      << ").\n";
            sim.close(sim.lostReason());
            waitingSince = std::chrono::steady_clock::now();
            continue;
        }
        std::this_thread::sleep_for(100ms);
    }

    sim.close("recorder_exit");
    log.write("recorder_stopped");
    std::cout << "Stopped.\n";
    return 0;
}
