// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "your_flight_model.h"

#include <chrono>
#include <format>
#include <iostream>

#include <QVariantMap>

#include "airline_queries.h"
#include "flight_settlement.h"

using namespace blocktime;

namespace {


QString q(const std::string& s) { return QString::fromStdString(s); }


}  // namespace

YourFlightModel::YourFlightModel(db::Database& db, RecorderBridge& bridge, FlightTargetSlot& target,
                                 std::filesystem::path logDir, std::filesystem::path ratesFile, QObject* parent)
    : FlightSource(parent), db_(db), bridge_(bridge), target_(target), logDir_(std::move(logDir)),
      ratesFile_(std::move(ratesFile)), settlement_(db_, logDir_, ratesFile_) {
    connect(&bridge_, &RecorderBridge::connectionChanged, this, &YourFlightModel::changed);
    connect(&bridge_, &RecorderBridge::connectionChanged, this, &YourFlightModel::notRecordingChanged);
    connect(&bridge_, &RecorderBridge::matchChanged, this, &YourFlightModel::notRecordingChanged);
    connect(&bridge_, &RecorderBridge::releaseRefusedChanged, this, &YourFlightModel::notRecordingChanged);
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) { onBlockEvent(type); });
    // Deboarding ends after the flight is settled; its row goes on that flight then (D-137).
    connect(&bridge_, &RecorderBridge::flightEvent, this, [this](const QString& type, const QVariantMap& data) {
        const QString state = data.value("state").toString();
        if (type == "gsx_service" && data.value("service").toString() == "deboarding" &&
            (state == "completed" || state == "available")) {
            recordDeboardings(db_, logDir_);
        }
    });
    flight_ = currentPlannedFlight(db_);
    const auto t = flight_ ? flightTarget(db_, *flight_) : std::nullopt;
    fuelOnRecordLb_ = t ? t->fuelOnRecordLb : 0;
    target_.set(t);
}

void YourFlightModel::reload(const QString& message) {
    flight_ = currentPlannedFlight(db_);
    const auto t = flight_ ? flightTarget(db_, *flight_) : std::nullopt;
    fuelOnRecordLb_ = t ? t->fuelOnRecordLb : 0;
    target_.set(t);
    message_ = message;
    if (flight_ && !summaryFlightId_.isEmpty()) {  // a new plan replaces the summary
        summaryFlightId_.clear();
        bridge_.showNextLeg();
    }
    emit changed();
    emit flightChanged();
    emit notRecordingChanged();
}

void YourFlightModel::onBlockEvent(const QString& type) {
    if (type == "block_in") {
        settle(true);  // pending until it settles: the block_in line may not be in the log yet (D-049)
    } else if (type == "flight_interrupted" && flight_) {
        // Discarded as if it never happened; the plan stays for another try (D-072, D-108).
        notice_ = q(flight_->origin) + QString::fromUtf8("–") + q(flight_->destination) + " interrupted and discarded";
    } else if (type == "block_out") {
        notice_.clear();
    }
    emit changed();
    emit notRecordingChanged();
}

void YourFlightModel::retryPendingSettle() {
    if (settlement_.pending()) checkFlown();
}

void YourFlightModel::checkFlown() { settle(false); }

void YourFlightModel::settle(bool blockInSeen) {
    if (!flight_) flight_ = currentPlannedFlight(db_);
    auto outcome = settlement_.check(blockInSeen);
    if (outcome.kind == SettlementCoordinator::Outcome::Kind::Waiting) {
        notice_ = "Flown, waiting to settle: " + q(outcome.waitingWhy);
        emit changed();
        return;
    }
    if (outcome.kind != SettlementCoordinator::Outcome::Kind::Settled) return;
    notice_.clear();
    settledLines_ = std::move(outcome.lines);
    summaryRegistration_ = q(outcome.registration);
    reload();
    summaryFlightId_ = q(outcome.settled->logFlightId);  // after reload(), which clears it only while a flight is planned
    emit changed();
    emit settled(summaryFlightId_);
}

void YourFlightModel::doneForNow() {
    summaryFlightId_.clear();
    bridge_.showNextLeg();
    emit changed();
    emit notRecordingChanged();
}

void YourFlightModel::turnaround() {
    const QString registration = summaryRegistration_;
    summaryFlightId_.clear();
    bridge_.showNextLeg();
    emit changed();
    emit notRecordingChanged();
    if (!registration.isEmpty()) emit turnaroundRequested(registration);
}

QString YourFlightModel::notRecording() const {
    const auto& match = bridge_.match();
    if (!match || bridge_.connection() != "connected" || bridge_.flightOpen()) return {};
    switch (match->problem) {
        case MatchProblem::None:
            // The sim matches now, but the release that came before it was refused: only a new one opens the flight.
            if (bridge_.releaseRefused() && !bridge_.parkingBrake()) {
                return "Not recording yet: set the parking brake, then release it. The last release came before the "
                       "sim matched this flight.";
            }
            return {};
        case MatchProblem::NoPlannedFlight:
            // Right after a flight is settled nothing is planned; that's the summary's moment, not a warning's.
            if (!summaryFlightId_.isEmpty()) return {};
            if (flight_) return "Not recording: import the SimBrief plan on Your Flight first.";  // D-250
            return "Not recording: no flight is planned. Plan one from Fleet.";
        case MatchProblem::WrongAircraftType: {
            if (!flight_) return {};
            QString loaded = q(match->aircraftFolder);
            db::Statement types(db_, "SELECT code, sim_folder FROM aircraft_type");
            while (types.next()) {
                if (folderMatches(match->aircraftFolder, types.text(1))) loaded = q(types.text(0));
            }
            if (loaded.isEmpty()) loaded = "aircraft the sim hasn't named yet";
            return "Not recording: you're in a " + loaded + "; this flight expects the " + q(flight_->typeCode) + " " +
                   q(flight_->registration) + ".";
        }
        case MatchProblem::WrongAirport: {
            if (!flight_) return {};
            const auto nearest = nearestAirport(db_, match->lat, match->lon);
            const QString origin = q(flight_->origin);
            if (nearest && nearest->icao != flight_->origin && nearest->distanceM <= 5000) {
                return "Not recording: you're at " + q(nearest->icao) + "; load in at " + origin + ".";
            }
            return "Not recording: you're " + QString::number(match->distanceM / 1000.0, 'f', 1) + " km from " + origin +
                   "; load in at " + origin + ".";
        }
        case MatchProblem::FuelBelowRecord:
            if (!flight_) return {};
            return "Not recording: load at least " + q(formatNumber(match->minFuelLb)) + " lb of fuel (" +
                   q(flight_->registration) + " had " + q(formatNumber(fuelOnRecordLb_)) +
                   " lb at its last block in).";
    }
    return {};
}

void YourFlightModel::cancelFlight() {
    if (bridge_.flightOpen()) return;  // it's being flown: cancelling would lose it without a word
    // A flown flight that hasn't settled yet would never settle once its planned flight is gone (review 1.2).
    if (settlement_.pending() || findCurrentFlown(db_, logDir_, false)) {
        message_ = "This flight was flown and is waiting to settle, so it can't be cancelled.";
        emit changed();
        return;
    }
    std::string error;
    if (!cancelPlannedFlight(db_, &error)) {
        message_ = q(error);
        emit changed();
        return;
    }
    reload();
}

blocktime::YourFlightStage YourFlightModel::stage() const {
    StageInputs in;
    in.formOpen = formOpen_;
    in.planned = flight_.has_value();
    in.planImported = flight_ && flight_->plan.has_value();
    in.accepted = flight_ && flight_->acceptedAt.has_value();
    in.flightOpen = bridge_.flightOpen();
    in.settlePending = settlement_.pending();
    in.summaryShowing = !summaryFlightId_.isEmpty();
    in.finishable = finishable();
    return yourFlightStage(in);
}

void YourFlightModel::setFormOpen(bool open) {
    if (open == formOpen_) return;
    formOpen_ = open;
    emit changed();
}
