// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The recorder loop (D-053), run on its own thread by the app. Every status line goes to the observer, which shows it
// in the app's event feed.

#include "recorder.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <format>
#include <thread>
#include <type_traits>
#include <variant>

#include "event_log.h"
#include "event_outbox.h"
#include "open_flight.h"
#include "sim_connection.h"
#include "telemetry_filter.h"

namespace blocktime {

using namespace std::chrono_literals;

namespace {

// Reconnect back-off (D-024): 10 s, 20 s after 30 min of waiting, 40 s after 60 min.
std::chrono::seconds retryInterval(std::chrono::steady_clock::duration waited) {
    if (waited >= 60min) return 40s;
    if (waited >= 30min) return 20s;
    return 10s;
}

// Sleeps in small steps so a stop request stays responsive.
void sleepFor(std::chrono::milliseconds total, const std::atomic<bool>& stop) {
    const auto until = std::chrono::steady_clock::now() + total;
    while (!stop && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(100ms);
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
    TelemetryLogger(EventLog& log, RecorderObserver& observer) : log_(log), observer_(observer) {}

    // Logs the sample (or why it was rejected). Returns whether it is valid, i.e. safe for block detection.
    bool onSample(const TelemetrySample& s, bool simRunning) {
        const SampleProblem p = checkSample(s, simRunning);
        if (p != SampleProblem::None) {
            ++skipped_;
            if (p != lastProblem_) {
                log_.write("telemetry_invalid", {{"reason", toString(p)}});
                observer_.onLine(std::format("{}  telemetry_invalid  {}", utcTimestamp().substr(11, 8), toString(p)),
                                 false);
            }
            lastProblem_ = p;
            hasLast_ = false;  // always log the first good sample after an invalid stretch
            return false;
        }
        // Skip a sample identical to the last one logged (snapshot + periodic often coincide; D-029).
        if (hasLast_ && std::memcmp(&s, &last_, sizeof(s)) == 0) return true;
        last_ = s;
        hasLast_ = true;
        nlohmann::json data = toJson(s);
        if (skipped_ > 0) {
            data["skipped_invalid"] = skipped_;
            skipped_ = 0;
        }
        lastProblem_ = SampleProblem::None;
        log_.write("telemetry", data);
        observer_.onTelemetry(s);
        return true;
    }

    void reset() {
        lastProblem_ = SampleProblem::None;
        skipped_ = 0;
        hasLast_ = false;
    }

private:
    EventLog& log_;
    RecorderObserver& observer_;
    SampleProblem lastProblem_ = SampleProblem::None;
    long long skipped_ = 0;
    TelemetrySample last_{};
    bool hasLast_ = false;
};

std::string hhmmss(TimePoint t) { return formatUtc(t).substr(11, 8); }

// Writes a block event to the log through the outbox, then reports a short line for it. If the write fails, the
// event is held and retried, and the line shows its data so it is never lost unseen (D-049).
void logBlockEvent(EventOutbox& outbox, RecorderObserver& observer, const BlockEvent& event) {
    const nlohmann::json data = toJson(event);
    if (!outbox.send(std::string(eventType(event)), data)) {
        observer.onLine("WARNING: " + std::string(eventType(event)) +
                            " could not be written to the log; retrying every 5 s. " +
                            data.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace),
                        false);
    }
    const std::string now = utcTimestamp().substr(11, 8);
    std::visit(
        [&](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, BlockOut>) {
                observer.onLine(std::format("{}  BLOCK OUT  {}  released {}  fuel {:.0f} lb", now, e.atcId,
                                            hhmmss(e.time), e.fuelLb),
                                false);
            } else if constexpr (std::is_same_v<T, BlockIn>) {
                observer.onLine(std::format("{}  BLOCK IN  brake set {}  fuel {:.0f} lb  block {}:{:02}", now,
                                            hhmmss(e.time), e.fuelLb, e.blockTimeS / 3600, e.blockTimeS / 60 % 60),
                                false);
            } else {
                observer.onLine(std::format("{}  FLIGHT INTERRUPTED  ({})", now, e.reason), false);
            }
        },
        event);
    observer.onBlockEvent(event);
}

void endSession(EventOutbox& outbox, RecorderObserver& observer, BlockDetector& detector, std::string_view reason) {
    if (auto e = detector.onSessionEnded(reason)) logBlockEvent(outbox, observer, *e);
}

// Retries held block events at most every 5 s (D-049), so a failing disk isn't hammered every 100 ms.
class HeldEventRetry {
public:
    HeldEventRetry(EventOutbox& outbox, RecorderObserver& observer) : outbox_(outbox), observer_(observer) {}
    void tick() {
        if (outbox_.pending() == 0) return;
        const auto now = std::chrono::steady_clock::now();
        if (now - lastTry_ < 5s) return;
        lastTry_ = now;
        if (outbox_.flush()) observer_.onLine("Held events written to the log.", false);
    }

private:
    EventOutbox& outbox_;
    RecorderObserver& observer_;
    std::chrono::steady_clock::time_point lastTry_ = std::chrono::steady_clock::now();
};

}  // namespace

int enginesRunning(const TelemetrySample& s) {
    int n = 0;
    for (int i = 0; i < engineCount(s); ++i) n += s.engCombustion[i] != 0 ? 1 : 0;
    return n;
}

Recorder::Recorder(std::filesystem::path logDir, RecorderObserver& observer)
    : logDir_(std::move(logDir)), observer_(observer) {}

int Recorder::run(const std::atomic<bool>& stop) {
    RecorderObserver& observer = observer_;
    EventLog log(logDir_);
    if (!log.write("recorder_started", {{"version", BLOCKTIME_VERSION}})) return 1;
    EventOutbox outbox([&log](std::string_view type, const nlohmann::json& data) { return log.write(type, data); });
    HeldEventRetry retry(outbox, observer);
    observer.onStarted(log.dir());

    // A recorder that was killed (or a power cut) left its open flight without an end; close it now (D-048).
    // The caller holds the single-instance mutex, so this can't close a flight another recorder is still recording.
    if (auto id = findOpenFlight(log.dir())) {
        observer.onLine("Previous flight " + *id +
                            " was left open (the recorder didn't close cleanly); logged as interrupted.",
                        false);
        logBlockEvent(outbox, observer, FlightInterrupted{*id, "recorder_crash"});
    }

    TelemetryLogger telemetry(log, observer);
    BlockDetector detector;
    SimConnection sim(log, [&](const TelemetrySample& s, bool running) {
        // Only samples that pass the junk filter may start or end a flight (D-025).
        if (!telemetry.onSample(s, running)) {
            detector.onInvalidSample();
            return;
        }
        if (auto e = detector.onSample(toBlockSample(s, std::chrono::system_clock::now()))) {
            logBlockEvent(outbox, observer, *e);
        }
    }, [&](bool running) {
        // Leaving the flight (main menu, restart, quit) ends it; it is never resumed (D-046, D-047).
        if (!running) {
            if (auto e = detector.onSimStopped()) logBlockEvent(outbox, observer, *e);
        }
    });

    auto waitingSince = std::chrono::steady_clock::now();
    bool announcedWaiting = false;
    bool announcedConnected = false;

    while (!stop) {
        retry.tick();
        if (!sim.isOpen()) {
            if (sim.open()) {
                observer.onLine("Sim found, waiting for it to finish starting...", false);
                observer.onConnection(ConnectionState::SimStarting);
                announcedWaiting = false;
                announcedConnected = false;
                telemetry.reset();
            } else {
                if (!announcedWaiting) {
                    observer.onLine("Waiting for sim...", false);
                    observer.onConnection(ConnectionState::WaitingForSim);
                    announcedWaiting = true;
                }
                sleepFor(retryInterval(std::chrono::steady_clock::now() - waitingSince), stop);
                continue;
            }
        }

        const bool alive = sim.pump();
        if (sim.handshakeComplete() && !announcedConnected) {
            observer.onLine("Connected to sim.", false);
            observer.onConnection(ConnectionState::Connected);
            announcedConnected = true;
        }
        if (!alive) {
            observer.onLine(std::string(announcedConnected ? "Disconnected (" : "Gave up waiting for sim (") +
                                sim.lostReason() + ").",
                            false);
            const std::string reason = sim.lostReason();
            sim.close(reason);
            endSession(outbox, observer, detector, reason);
            waitingSince = std::chrono::steady_clock::now();
            continue;
        }
        std::this_thread::sleep_for(100ms);
    }

    sim.close("recorder_exit");
    endSession(outbox, observer, detector, "recorder_exit");
    // Last chance for held block events; anything still held is reported so it can be recovered by hand (D-049).
    if (!outbox.flush()) {
        for (const auto& [type, data] : outbox.held()) {
            observer.onLine("LOST: " + type + " " +
                                data.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace),
                            true);
        }
    }
    log.write("recorder_stopped");
    observer.onLine("Stopped.", false);
    return 0;
}

}  // namespace blocktime
