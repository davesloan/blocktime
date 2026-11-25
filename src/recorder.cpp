// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The recorder loop (D-053), run on its own thread by the app. Every status line goes to the observer, which shows it
// in the app's event feed.

#include "recorder.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <format>
#include <thread>
#include <type_traits>
#include <variant>

#include "cockpit_monitor.h"
#include "event_log.h"
#include "event_outbox.h"
#include "flight_tracker.h"
#include "gsx_watcher.h"
#include "match_reporter.h"
#include "open_flight.h"
#include "sim_connection.h"
#include "telemetry_filter.h"
#include "telemetry_json.h"

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
            } else if constexpr (std::is_same_v<T, BlockOutRefused>) {
                observer.onLine(std::format("{}  NOT RECORDING  {}", now, problemCode(e.match.problem)), false);
            } else {
                observer.onLine(std::format("{}  FLIGHT INTERRUPTED  ({})", now, e.reason), false);
            }
        },
        event);
    observer.onBlockEvent(event);
}

PhaseSample toPhaseSample(const TelemetrySample& s, TimePoint time, double activeS) {
    PhaseSample p;
    p.time = time;
    p.activeS = activeS;
    p.parkingBrake = s.parkingBrake != 0;
    p.onGround = s.onGround != 0;
    p.altFt = s.indicatedAltitudeFt;
    p.aglFt = s.altitudeAglFt;
    p.vsFpm = s.verticalSpeedFpm;
    p.groundSpeedKt = s.groundSpeedKt;
    p.lat = s.latitudeDeg;
    p.lon = s.longitudeDeg;
    p.iasKt = s.indicatedAirspeedKt;
    p.flapsOut = s.flapsTrailingPct > kFlapsOutPct;
    return p;
}

// A short feed line for the flight events worth seeing as they happen; "" for the rest.
std::string flightLine(const LogEvent& e) {
    const std::string now = utcTimestamp().substr(11, 8);
    if (e.type == "phase_changed") {
        std::string phase = e.data.value("phase", "");
        std::transform(phase.begin(), phase.end(), phase.begin(), [](unsigned char c) {
            return c == '_' ? ' ' : static_cast<char>(std::toupper(c));
        });
        return now + "  " + phase;
    }
    if (e.type == "liftoff") return now + "  LIFTOFF  " + e.data.value("runway", "");
    if (e.type == "lined_up") {
        return std::format("{}  LINED UP  runway {}  {:.1f} nm", now, e.data.value("runway", ""),
                           e.data.value("distance_nm", 0.0));
    }
    if (e.type == "gsx_service") {
        const std::string state = e.data.value("state", "");
        if (state != "requested" && state != "performing" && state != "completed") return "";
        return now + "  GSX " + e.data.value("service", "") + " " + state;
    }
    if (e.type == "touchdown") {
        return std::format("{}  TOUCHDOWN  {:.0f} fpm  {:.2f} g", now, e.data.value("vs_fpm", 0.0),
                           e.data.value("g", 0.0));
    }
    return "";
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

Recorder::Recorder(std::filesystem::path logDir, RecorderObserver& observer, const FlightTargetSlot& target,
                   AppEventQueue& appEvents, RecorderOptions options)
    : logDir_(std::move(logDir)), observer_(observer), target_(target), appEvents_(appEvents), options_(options) {}

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
        logBlockEvent(outbox, observer, FlightInterrupted{*id, "recorder_crash"});  // the tracker has no flight yet
    }

    TelemetryLogger telemetry(log, observer);
    const FlightTargetSlot& target = target_;
    BlockDetector detector([&target](const BlockSample& release) { return checkMatch(target.get(), release); });
    MatchReporter matches;  // the banner's answer (D-088); reset at each connect
    SimConnection* simPtr = nullptr;

    // One flight's phases, runways, touchdown and altimeter (milestone 7). Its events are logged, then shown.
    FlightTracker tracker;
    const auto writeFlightEvents = [&](const std::vector<LogEvent>& events) {
        for (const LogEvent& e : events) {
            log.write(e.type, e.data);
            observer.onFlightEvent(e.type, e.data);
            if (const std::string line = flightLine(e); !line.empty()) observer.onLine(line, false);
        }
    };
    // The cockpit state the debrief checks (D-159), written on change while connected; with the flight's id while one
    // is open. A new sim session starts it afresh.
    CockpitMonitor cockpit;
    const auto writeCockpit = [&] {
        auto data = cockpit.due(std::chrono::system_clock::now());
        if (!data) return;
        if (tracker.flightOpen()) (*data)["flight_id"] = tracker.flightId();
        log.write("cockpit", *data);
    };
    // Every block event goes through here, so the tracker opens and closes with the flight.
    const auto blockEvent = [&](BlockEvent e, const TelemetrySample* s) {
        // The spot it happened at, from the loaded airports' parking (D-148).
        if (auto* out = std::get_if<BlockOut>(&e)) out->parking = tracker.parkingAt(out->latDeg, out->lonDeg);
        if (auto* in = std::get_if<BlockIn>(&e)) in->parking = tracker.parkingAt(in->latDeg, in->lonDeg);
        logBlockEvent(outbox, observer, e);
        if (const auto* out = std::get_if<BlockOut>(&e)) {
            writeFlightEvents(tracker.onBlockOut(*out, target.get(), s ? s->indicatedAltitudeFt : 0));
            // The cockpit state at block out, with the flight's id (debrief, D-159).
            if (auto snap = cockpit.snapshot(std::chrono::system_clock::now())) {
                (*snap)["flight_id"] = tracker.flightId();
                log.write("cockpit", *snap);
            }
        } else if (const auto* in = std::get_if<BlockIn>(&e)) {
            writeFlightEvents(tracker.onBlockIn(*in, s ? s->indicatedAltitudeFt : 0));
        } else if (std::holds_alternative<FlightInterrupted>(e)) {
            tracker.onFlightEnded();
        }
        if (simPtr) simPtr->setFastSampling(tracker.wantsFastSampling());
    };
    // GSX's services, read whether or not a flight is open (D-073, D-138). A new sim session starts them afresh.
    GsxWatcher gsx;
    // The app's GsxTimeline drops the next leg's services when the sim session ends (D-138). These are logged by
    // SimConnection already; the observer is only told.
    const auto sessionEvent = [&](bool running) {
        gsx.reset();
        observer.onFlightEvent(running ? "sim_running" : "sim_stopped", {{"time", utcTimestamp()}});
    };
    const auto endSession = [&](std::string_view reason) {
        if (auto e = detector.onSessionEnded(reason)) blockEvent(*e, nullptr);
        sessionEvent(false);
    };
    // The airports last asked for, so a new planned flight asks again (D-126, D-147).
    std::vector<std::string> airportsAsked;
    const auto askAirports = [&](SimConnection& sim) {
        const auto t = target.get();
        if (!t) return;
        std::vector<std::string> want{t->originIcao, t->destinationIcao};
        if (want != airportsAsked && sim.requestAirports(want)) airportsAsked = std::move(want);
    };
    SimConnection sim(log, [&](const TelemetrySample& s, bool running) {
        // Only samples that pass the junk filter may start or end a flight (D-025).
        if (!telemetry.onSample(s, running)) {
            detector.onInvalidSample();
            return;
        }
        BlockSample b = toBlockSample(s, std::chrono::system_clock::now());
        b.aircraftFolder = aircraftFolder(simPtr->aircraftPath());
        cockpit.onPlain({s.lightLanding != 0, s.seatBeltSign != 0, s.gearExtended, simPtr->paused()}, b.time);
        // The banner's answer: the same check a release would get, reported when it changes (D-088).
        if (!detector.flightOpen()) {
            if (auto m = matches.onSample(target.get(), b)) observer.onMatch(*m);
        }
        if (auto e = detector.onSample(b)) {
            blockEvent(*e, &s);
            // The probe looks at the airports again after a flight, when the arrival's parking matters (D-116).
            if (options_.probe && std::holds_alternative<BlockIn>(*e)) {
                if (const auto t = target.get()) simPtr->requestProbeFacilities({t->originIcao, t->destinationIcao});
            }
        }
        if (tracker.flightOpen()) {
            writeFlightEvents(tracker.onSample(toPhaseSample(s, b.time, simPtr->activeSeconds())));
            simPtr->setFastSampling(tracker.wantsFastSampling());
        }
    }, [&](bool running) {
        // Leaving the flight (main menu, restart, quit) ends it; it is never resumed (D-046, D-047).
        if (!running) {
            if (auto e = detector.onSimStopped()) blockEvent(*e, nullptr);
        }
        sessionEvent(running);
    }, options_.probe);

    simPtr = &sim;
    sim.setFlightCallbacks(
        [&](const FastSample& f) {
            writeFlightEvents(tracker.onFastSample(f));
            sim.setFastSampling(tracker.wantsFastSampling());
        },
        [&](const AltimeterSample& a) { writeFlightEvents(tracker.onAltimeter(a)); },
        [&](AirportFacilities a) {
            // The runways are logged (D-126); the spot list is the scenery's static data and isn't (D-147).
            log.write("airport_runways", toJson(AirportRunways{a.icao, a.runways}));
            observer.onAirport(a);
            tracker.setAirport(std::move(a));
        },
        [&](const GsxValues& v) { writeFlightEvents(gsx.onValues(v, std::chrono::system_clock::now())); });
    sim.setCockpitCallback([&](const PmdgCockpit& p) { cockpit.onPmdg(p, std::chrono::system_clock::now()); });
    if (options_.probe) observer.onLine("Probe on: logging GSX, PMDG and airport data (D-116).", false);
    auto waitingSince = std::chrono::steady_clock::now();
    bool announcedWaiting = false;
    bool announcedConnected = false;

    while (!stop) {
        retry.tick();
        // The app's events (D-127): logged first, then told back, so Live sees e.g. the arrival gate (D-147).
        for (const LogEvent& e : appEvents_.drain()) {
            log.write(e.type, e.data);
            if (e.type == "arrival_gate") {
                observer.onLine(utcTimestamp().substr(11, 8) + "  ARRIVAL GATE  " + e.data.value("gate", std::string()) +
                                    "  (" + e.data.value("result", std::string()) + ")",
                                false);
            }
            observer.onFlightEvent(e.type, e.data);
        }
        if (!sim.isOpen()) {
            if (sim.open()) {
                observer.onLine("Sim found, waiting for it to finish starting...", false);
                observer.onConnection(ConnectionState::SimStarting);
                announcedWaiting = false;
                announcedConnected = false;
                telemetry.reset();
                cockpit.reset();
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
        writeCockpit();
        if (sim.handshakeComplete() && !announcedConnected) {
            matches.reset();  // report afresh in this session
            airportsAsked.clear();
            observer.onLine("Connected to sim.", false);
            if (options_.probe) {
                if (const auto t = target.get()) sim.requestProbeFacilities({t->originIcao, t->destinationIcao});
            }
            observer.onConnection(ConnectionState::Connected);
            announcedConnected = true;
        }
        if (announcedConnected) askAirports(sim);
        // A plan made or cancelled while the airplane sits still: no sample comes, so check the last one again.
        if (announcedConnected && !detector.flightOpen()) {
            if (auto m = matches.recheck(target.get())) observer.onMatch(*m);
        }
        if (!alive) {
            observer.onLine(std::string(announcedConnected ? "Disconnected (" : "Gave up waiting for sim (") +
                                sim.lostReason() + ").",
                            false);
            const std::string reason = sim.lostReason();
            sim.close(reason);
            endSession(reason);
            waitingSince = std::chrono::steady_clock::now();
            continue;
        }
        std::this_thread::sleep_for(100ms);
    }

    sim.close("recorder_exit");
    endSession("recorder_exit");
    for (const LogEvent& e : appEvents_.drain()) log.write(e.type, e.data);
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
