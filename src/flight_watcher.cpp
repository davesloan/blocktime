// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_watcher.h"

#include <QPointer>

using namespace std::chrono_literals;

FlightWatcher::FlightWatcher(Planner& planner, RecorderBridge& bridge, const blocktime::FlightTargetSlot& target,
                             blocktime::AppEventQueue& queue, QObject* parent)
    : QObject(parent), planner_(planner), target_(target), queue_(queue) {
    setIntervals(60s, 10min);
    connect(&atcTimer_, &QTimer::timeout, this, &FlightWatcher::fetchAtc);
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

void FlightWatcher::stopAll() {
    atcTimer_.stop();
    originTimer_.stop();
    destinationTimer_.stop();
}
