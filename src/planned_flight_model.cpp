// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "planned_flight_model.h"

#include <chrono>
#include <format>

#include <QClipboard>
#include <QGuiApplication>
#include <QVariantMap>

#include "airline_queries.h"
#include "flight_settlement.h"
#include "new_flight_model.h"
#include "simbrief.h"

using namespace blocktime;

namespace {

std::string utcNowInstant() {
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

QString q(const std::string& s) { return QString::fromStdString(s); }

QString hhmm(const std::string& instant) { return instant.size() >= 16 ? q(instant.substr(11, 5)) + "Z" : q(instant); }

}  // namespace

PlannedFlightModel::PlannedFlightModel(db::Database& db, QString airlineIcao, Planner& planner, RecorderBridge& bridge,
                                       FlightTargetSlot& target, std::filesystem::path logDir,
                                       std::filesystem::path ratesFile, QObject* parent)
    : QObject(parent), db_(db), airlineIcao_(std::move(airlineIcao)), planner_(planner), bridge_(bridge),
      target_(target), logDir_(std::move(logDir)), ratesFile_(std::move(ratesFile)) {
    connect(&bridge_, &RecorderBridge::connectionChanged, this, &PlannedFlightModel::changed);
    connect(&bridge_, &RecorderBridge::connectionChanged, this, &PlannedFlightModel::notRecordingChanged);
    connect(&bridge_, &RecorderBridge::matchChanged, this, &PlannedFlightModel::notRecordingChanged);
    connect(&bridge_, &RecorderBridge::releaseRefusedChanged, this, &PlannedFlightModel::notRecordingChanged);
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) { onBlockEvent(type); });
    rates_ = loadRates(ratesFile_, nullptr).value_or(Rates{});
    flight_ = currentPlannedFlight(db_);
    const auto t = flight_ ? flightTarget(db_, *flight_) : std::nullopt;
    fuelOnRecordLb_ = t ? t->fuelOnRecordLb : 0;
    target_.set(t);
}

void PlannedFlightModel::reload(const QString& message) {
    rates_ = loadRates(ratesFile_, nullptr).value_or(Rates{});
    flight_ = currentPlannedFlight(db_);
    const auto t = flight_ ? flightTarget(db_, *flight_) : std::nullopt;
    fuelOnRecordLb_ = t ? t->fuelOnRecordLb : 0;
    target_.set(t);
    message_ = message;
    if (flight_) summaryFlightId_.clear();  // a new plan replaces the summary
    emit changed();
    emit flightChanged();
    emit notRecordingChanged();
}

void PlannedFlightModel::onBlockEvent(const QString& type) {
    if (type == "block_in") {
        checkFlown();
    } else if (type == "flight_interrupted" && flight_) {
        // Discarded as if it never happened; the plan stays for another try (D-072, D-108).
        notice_ = q(flight_->origin) + QString::fromUtf8("–") + q(flight_->destination) + " interrupted and discarded";
    } else if (type == "block_out") {
        notice_.clear();
    }
    emit changed();
    emit notRecordingChanged();
}

void PlannedFlightModel::checkFlown() {
    if (!flight_) flight_ = currentPlannedFlight(db_);
    if (!flight_) return;
    const std::string registration = flight_->registration;
    std::string error;
    // Ledger lines are permanent, so a flight is never settled at default rates: it waits until rates.yaml reads
    // (the next block event or start settles it).
    const auto rates = loadRates(ratesFile_, &error);
    if (!rates) {
        notice_ = "Flight not settled yet: " + q(error) + ". Fix rates.yaml and restart BlockTime.";
        emit changed();
        return;
    }
    const auto result = settleIfFlown(db_, logDir_, *rates, &error);
    if (!result) {
        if (!error.empty()) {
            notice_ = q(error);
            emit changed();
        }
        return;
    }
    notice_.clear();
    summaryRegistration_ = q(registration);
    reload();
    summaryFlightId_ = q(result->logFlightId);  // after reload(), which clears it only while a flight is planned
    emit changed();
    emit settled(summaryFlightId_);
}

void PlannedFlightModel::doneForNow() {
    summaryFlightId_.clear();
    emit changed();
    emit notRecordingChanged();
}

void PlannedFlightModel::turnaround() {
    const QString registration = summaryRegistration_;
    summaryFlightId_.clear();
    emit changed();
    emit notRecordingChanged();
    if (!registration.isEmpty()) emit turnaroundRequested(registration);
}

QString PlannedFlightModel::notRecording() const {
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

void PlannedFlightModel::openDispatchPage() {
    if (!flight_) return;
    DispatchRequest r;
    r.origin = flight_->origin;
    r.destination = flight_->destination;
    r.airlineIcao = airlineIcao_.toStdString();
    r.flightNumber = flight_->flightNumber;
    r.airframeId = flight_->airframeId;
    r.originRunway = flight_->originRunway;
    r.destinationRunway = flight_->destinationRunway;
    for (const CabinLoad& c : flight_->cabins) r.passengers += c.passengers;
    r.cargoLb = flight_->cargoLb;
    planner_.openUrl(q(dispatchUrl(r)));
}

void PlannedFlightModel::importPlan() {
    if (!flight_ || busy_) return;
    busy_ = true;
    emit changed();
    planner_.fetchLatestOfp([this](QByteArray body, QString networkError) {
        busy_ = false;
        if (!flight_) return;
        if (!networkError.isEmpty()) {
            message_ = networkError;
            emit changed();
            return;
        }
        std::string error;
        const auto plan = parseOfp(body.toStdString(), &error);
        if (!plan) {
            message_ = "SimBrief: " + q(error);
            emit changed();
            return;
        }
        const PlanKey key{flight_->origin, flight_->destination, flight_->airframeId, flight_->registration,
                          flight_->plannedAt};
        if (const auto mismatch = planMismatch(*plan, key)) {
            message_ = q(*mismatch);
            emit changed();
            return;
        }
        if (!storeSimbriefPlan(db_, flight_->id, *plan, body.toStdString(), utcNowInstant(), &error)) {
            message_ = "Couldn't store the plan: " + q(error);
            emit changed();
            return;
        }
        reload("Imported the plan generated " + hhmm(plan->generatedAt));
    });
}

void PlannedFlightModel::cancelFlight() {
    if (bridge_.flightOpen()) return;  // it's being flown: cancelling would lose it without a word
    std::string error;
    if (!cancelPlannedFlight(db_, &error)) {
        message_ = q(error);
        emit changed();
        return;
    }
    reload();
}

void PlannedFlightModel::copyRoute() {
    if (flight_ && flight_->plan) QGuiApplication::clipboard()->setText(q(routeWithRunways(*flight_->plan)));
}

QString PlannedFlightModel::state() const {
    if (!flight_) return summaryFlightId_.isEmpty() ? QString() : QStringLiteral("summary");
    if (bridge_.flightOpen()) return "flying";
    if (!flight_->plan) return "waitingForPlan";
    return bridge_.connection() == "connected" ? "simConnected" : "waitingForSim";
}

QString PlannedFlightModel::stateText() const {
    const QString s = state();
    if (s == "waitingForPlan") return "Waiting for plan";
    if (s == "waitingForSim") return "Waiting for sim";
    if (s == "simConnected") return "Sim connected";
    if (s == "flying") return "Recording";
    return {};
}

QString PlannedFlightModel::title() const {
    if (!flight_) return {};
    return airlineIcao_ + q(flight_->flightNumber) + " · " + q(flight_->origin) + " → " + q(flight_->destination);
}

QString PlannedFlightModel::airplaneText() const {
    return flight_ ? q(flight_->registration + " · " + flight_->typeCode) : QString();
}

QString PlannedFlightModel::kindText() const { return flight_ ? NewFlightModel::kindLabel(flight_->kind) : QString(); }

QString PlannedFlightModel::loadInText() const {
    return flight_ ? "Load in at " + q(flight_->origin) + ", " + q(flight_->originName) : QString();
}

QString PlannedFlightModel::runwayText() const {
    if (!flight_) return {};
    if (flight_->originRunway.empty() && flight_->destinationRunway.empty()) return "No ATIS runways: SimBrief chose";
    const auto or_none = [](const std::string& r) { return r.empty() ? QStringLiteral("none") : q(r); };
    return "ATIS runways: " + or_none(flight_->originRunway) + " → " + or_none(flight_->destinationRunway);
}

QString PlannedFlightModel::route() const {
    return flight_ && flight_->plan ? q(routeWithRunways(*flight_->plan)) : QString();
}

QString PlannedFlightModel::planText() const {
    if (!flight_ || !flight_->plan) return {};
    const SimbriefPlan& p = *flight_->plan;
    return "Plan generated " + hhmm(p.generatedAt) + " · " +
           q(std::format("FL{} · {}h {:02}m · burn {} lb", p.cruiseAltitudeFt / 100, p.estBlockS / 3600,
                         p.estBlockS % 3600 / 60, formatNumber(p.enrouteBurnLb)));
}

QVariantList PlannedFlightModel::estimateLines() const {
    QVariantList out;
    if (!flight_) return out;
    const Preview p = pricePreview(priceInput(*flight_), rates_);
    for (const auto* lines : {&p.income, &p.costs}) {
        for (const MoneyLine& m : *lines) {
            out << QVariantMap{{"label", q(m.label)}, {"amount", q(formatMoneyFull(m.cents))}};
        }
    }
    return out;
}

QString PlannedFlightModel::totalText() const {
    return flight_ ? q(formatMoneyFull(pricePreview(priceInput(*flight_), rates_).totalCents)) : QString();
}

QString PlannedFlightModel::fuelNote() const {
    return flight_ && !flight_->plan ? QStringLiteral("+ fuel") : QString();
}

QString PlannedFlightModel::warning() const {
    if (!flight_ || !nearMlw(*flight_)) return {};
    const int ldw = flight_->plan->estLandingWeightLb, mlw = flight_->limits.mlwLb;
    return q(std::format("Landing weight {} lb is {}% of MLW ({} lb).", formatNumber(ldw),
                         static_cast<long long>(ldw) * 100 / mlw, formatNumber(mlw)));
}
