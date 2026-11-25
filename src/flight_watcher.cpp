// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_watcher.h"

#include <QPointer>

#include "event_log.h"

using namespace std::chrono_literals;

FlightWatcher::FlightWatcher(Planner& planner, RecorderBridge& bridge, const blocktime::FlightTargetSlot& target,
                             blocktime::AppEventQueue& queue, QObject* parent)
    : QObject(parent), planner_(planner), bridge_(bridge), target_(target), queue_(queue) {
    setIntervals(60s, 10min);
    setGateInterval(30s);
    connect(&atcTimer_, &QTimer::timeout, this, &FlightWatcher::fetchAtc);
    connect(&gateTimer_, &QTimer::timeout, this, &FlightWatcher::pollGate);
    connect(&originTimer_, &QTimer::timeout, this, [this] { fetchAtis(origin_, QStringLiteral("origin")); });
    connect(&destinationTimer_, &QTimer::timeout, this,
            [this] { fetchAtis(destination_, QStringLiteral("destination")); });
    connect(&bridge, &RecorderBridge::blockEvent, this, &FlightWatcher::onBlockEvent);
    connect(&bridge, &RecorderBridge::flightEvent, this, &FlightWatcher::onFlightEvent);
}

void FlightWatcher::setIntervals(std::chrono::milliseconds atc, std::chrono::milliseconds atis) {
    atcTimer_.setInterval(atc);
    originTimer_.setInterval(atis);
    destinationTimer_.setInterval(atis);
}

void FlightWatcher::setGateInterval(std::chrono::milliseconds gate) { gateTimer_.setInterval(gate); }

void FlightWatcher::setGateContext(GateContextFor gateContext) { gateContext_ = std::move(gateContext); }

void FlightWatcher::onBlockEvent(const QString& type, const QString& flightId) {
    if (type == "block_out") {
        stopAll();
        flightId_ = flightId;
        const auto t = target_.get();
        origin_ = t ? QString::fromStdString(t->originIcao) : QString();
        destination_ = t ? QString::fromStdString(t->destinationIcao) : QString();
        comms_.reset();
        atis_.reset();
        destinationDone_ = false;
        fetchAtc();
        atcTimer_.start();
        gateDone_ = !gateContext_ || destination_.isEmpty();
        if (!gateDone_) {
            pollGate();
            gateTimer_.start();
        }
        if (!origin_.isEmpty()) {
            fetchAtis(origin_, QStringLiteral("origin"));
            originTimer_.start();
        }
    } else if (type == "block_in") {
        stopAll();
        if (!flightId_.isEmpty()) fetchAtc();  // the last exchanges, written as the flight ends
    } else if (type == "flight_interrupted") {
        stopAll();
        flightId_.clear();
    }
}

void FlightWatcher::onFlightEvent(const QString& type, const QVariantMap& data) {
    if (flightId_.isEmpty()) return;
    const QString phase = data.value("phase").toString();
    // liftoff and touchdown are the cues; the phase changes cover a flight where either wasn't logged.
    if (type == "liftoff" || (type == "phase_changed" && phase == "climb")) {
        originTimer_.stop();
    } else if (type == "phase_changed" && (phase == "descent" || phase == "approach") && !destination_.isEmpty() &&
               !destinationTimer_.isActive() && !destinationDone_) {
        // Top of descent, or Approach straight from Cruise when no descent was confirmed (D-124).
        fetchAtis(destination_, QStringLiteral("destination"));
        destinationTimer_.start();
    } else if (type == "touchdown" || (type == "phase_changed" && phase == "landing")) {
        destinationTimer_.stop();
        destinationDone_ = true;
    }
}

void FlightWatcher::fetchAtc() {
    planner_.fetchCommsHistory([self = QPointer(this), id = flightId_](QByteArray body, QString error) {
        if (!self || !error.isEmpty()) return;
        const auto history = blocktime::parseCommsHistory(body.toStdString());
        if (!history) return;
        for (auto& entry : self->comms_.toLog(*history)) {
            self->queue_.push("atc_exchange", {{"flight_id", id.toStdString()},
                                               {"si_flight_id", history->flightId},
                                               {"entry", std::move(entry)}});
        }
    });
}

void FlightWatcher::fetchAtis(const QString& icao, const QString& role) {
    planner_.fetchWx(icao, [self = QPointer(this), id = flightId_, icao, role](QByteArray body, QString error) {
        if (!self || !error.isEmpty()) return;
        const auto a = blocktime::parseAtis(body.toStdString(), icao.toStdString());
        if (!a || !self->atis_.isNew(*a)) return;
        self->queue_.push("atis", {{"flight_id", id.toStdString()},
                                   {"icao", a->icao},
                                   {"role", role.toStdString()},
                                   {"letter", a->letter},
                                   {"text", a->text},
                                   {"metar", a->metar},
                                   {"altimeter", a->altimeter},
                                   {"departing", a->departing},
                                   {"arriving", a->arriving}});
    });
}

// SayIntentions' own gate first, then BlockTime's pick, then assignGate and the read-back (D-141, D-147, D-155). Every
// failure before the send is quiet: SayIntentions may not be running, or hasn't picked yet.
void FlightWatcher::pollGate() {
    if (gateDone_ || flightId_.isEmpty()) return;
    planner_.fetchSiFlight([self = QPointer(this), id = flightId_](QByteArray body, QString error) {
        if (!self || !error.isEmpty() || self->gateDone_ || self->flightId_ != id) return;
        const auto si = blocktime::parseSiFlight(body.toStdString());
        const blocktime::AirportFacilities* airport = self->bridge_.airport(self->destination_);
        const auto context = self->gateContext_ ? self->gateContext_(self->destination_) : std::nullopt;
        if (!si || !airport || !context) return;
        auto plan = blocktime::planGate(*si, self->destination_.toStdString(), *airport, *context);
        if (!plan) return;
        self->gateDone_ = true;
        self->gateTimer_.stop();
        if (!plan->send) {
            self->logGate(std::move(plan->event));
            return;
        }
        const QString shortName = QString::fromStdString(plan->shortName);
        self->planner_.assignGate(
            shortName, self->destination_, [self, plan = std::move(*plan)](QByteArray answer, QString assignError) {
                if (!self) return;
                if (assignError.isEmpty() && !blocktime::parseAssignedGate(answer.toStdString())) {
                    assignError = QStringLiteral("SayIntentions gave no assigned_gate_name");
                }
                if (!assignError.isEmpty()) {
                    self->logGate(blocktime::finishSend(plan, assignError.toStdString(), std::nullopt));
                    return;
                }
                self->planner_.fetchParking([self, plan](QByteArray parking, QString) {
                    if (!self) return;
                    self->logGate(blocktime::finishSend(plan, "", blocktime::parseParking(parking.toStdString())));
                });
            });
    });
}

void FlightWatcher::logGate(nlohmann::json event) {
    event["flight_id"] = flightId_.toStdString();
    event["time"] = blocktime::utcTimestamp();
    queue_.push("arrival_gate", std::move(event));
}

void FlightWatcher::stopAll() {
    gateTimer_.stop();
    atcTimer_.stop();
    originTimer_.stop();
    destinationTimer_.stop();
}
