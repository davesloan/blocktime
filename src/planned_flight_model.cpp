// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "planned_flight_model.h"

#include <chrono>
#include <format>

#include <QClipboard>
#include <QGuiApplication>
#include <QVariantMap>

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
                                       QObject* parent)
    : QObject(parent), db_(db), airlineIcao_(std::move(airlineIcao)), planner_(planner), bridge_(bridge) {
    connect(&bridge_, &RecorderBridge::connectionChanged, this, &PlannedFlightModel::changed);
    flight_ = currentPlannedFlight(db_);
}

void PlannedFlightModel::reload(const QString& message) {
    flight_ = currentPlannedFlight(db_);
    message_ = message;
    emit changed();
    emit flightChanged();
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
    std::string error;
    if (!cancelPlannedFlight(db_, &error)) {
        message_ = q(error);
        emit changed();
        return;
    }
    reload();
}

void PlannedFlightModel::copyRoute() {
    if (flight_ && flight_->plan) QGuiApplication::clipboard()->setText(q(flight_->plan->route));
}

QString PlannedFlightModel::state() const {
    if (!flight_) return {};
    if (!flight_->plan) return "waitingForPlan";
    return bridge_.connection() == "connected" ? "simConnected" : "waitingForSim";
}

QString PlannedFlightModel::stateText() const {
    const QString s = state();
    if (s == "waitingForPlan") return "Waiting for plan";
    if (s == "waitingForSim") return "Waiting for sim";
    if (s == "simConnected") return "Sim connected";
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

QString PlannedFlightModel::route() const { return flight_ && flight_->plan ? q(flight_->plan->route) : QString(); }

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
    const Preview p = pricePreview(priceInput(*flight_));
    for (const auto* lines : {&p.income, &p.costs}) {
        for (const MoneyLine& m : *lines) {
            out << QVariantMap{{"label", q(m.label)}, {"amount", q(formatMoneyFull(m.cents))}};
        }
    }
    return out;
}

QString PlannedFlightModel::totalText() const {
    return flight_ ? q(formatMoneyFull(pricePreview(priceInput(*flight_)).totalCents)) : QString();
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
