// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "planned_flight_display.h"

#include <format>

#include <QClipboard>
#include <QGuiApplication>
#include <QVariantMap>

#include "money_lines_qml.h"
#include "new_flight_model.h"
#include "simbrief.h"

using namespace blocktime;

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }

QString hhmm(const std::string& instant) { return instant.size() >= 16 ? q(instant.substr(11, 5)) + "Z" : q(instant); }

}  // namespace

PlannedFlightDisplay::PlannedFlightDisplay(QString airlineIcao, bool readOnly, QObject* parent)
    : FlightSource(parent), airlineIcao_(std::move(airlineIcao)), readOnly_(readOnly) {}

void PlannedFlightDisplay::copyRoute() {
    if (flight_ && flight_->plan) QGuiApplication::clipboard()->setText(q(routeWithRunways(*flight_->plan)));
}

QString PlannedFlightDisplay::stateText() const {
    const QString s = state();
    if (s == "waitingForPlan") return "Waiting for plan";
    if (s == "waitingForSim") return "Waiting for sim";
    if (s == "simConnected") return "Sim connected";
    if (s == "flying") return "Recording";
    if (s == "replay") return "Replay";
    return {};
}

QString PlannedFlightDisplay::title() const {
    if (!flight_) return {};
    return airlineIcao_ + q(flight_->flightNumber) + " · " + q(flight_->origin) + " → " + q(flight_->destination);
}

QString PlannedFlightDisplay::airplaneText() const {
    return flight_ ? q(flight_->registration + " · " + flight_->typeCode) : QString();
}

QString PlannedFlightDisplay::kindText() const { return flight_ ? NewFlightModel::kindLabel(flight_->kind) : QString(); }

QString PlannedFlightDisplay::loadInText() const {
    if (!flight_) return {};
    QString text = "Load in at " + q(flight_->origin) + ", " + q(flight_->originName);
    if (!flight_->loadInParking.empty()) text += ", " + q(flight_->loadInParking);  // where it last parked (D-148)
    return text;
}

QString PlannedFlightDisplay::runwayText() const {
    if (!flight_) return {};
    if (flight_->originRunway.empty() && flight_->destinationRunway.empty()) return "No ATIS runways: SimBrief chose";
    const auto or_none = [](const std::string& r) { return r.empty() ? QStringLiteral("none") : q(r); };
    return "ATIS runways: " + or_none(flight_->originRunway) + " → " + or_none(flight_->destinationRunway);
}

QString PlannedFlightDisplay::route() const {
    return flight_ && flight_->plan ? q(routeWithRunways(*flight_->plan)) : QString();
}

QString PlannedFlightDisplay::planText() const {
    if (!flight_ || !flight_->plan) return {};
    const SimbriefPlan& p = *flight_->plan;
    return "Plan generated " + hhmm(p.generatedAt) + " · " +
           q(std::format("FL{} · {}h {:02}m · burn {} lb", p.cruiseAltitudeFt / 100, p.estBlockS / 3600,
                         p.estBlockS % 3600 / 60, formatNumber(p.enrouteBurnLb)));
}

QVariantList PlannedFlightDisplay::estimateIncome() const {
    return flight_ ? moneyLinesToQml(pricePreview(priceInput(*flight_), rates_).income) : QVariantList{};
}

QVariantList PlannedFlightDisplay::estimateCosts() const {
    return flight_ ? moneyLinesToQml(pricePreview(priceInput(*flight_), rates_).costs) : QVariantList{};
}

QString PlannedFlightDisplay::totalText() const {
    return flight_ ? q(formatMoneyFull(pricePreview(priceInput(*flight_), rates_).totalCents)) : QString();
}

QString PlannedFlightDisplay::fuelNote() const {
    return flight_ && !flight_->plan ? QStringLiteral("+ fuel") : QString();
}

QString PlannedFlightDisplay::warning() const {
    if (!flight_ || !nearMlw(*flight_)) return {};
    const int ldw = flight_->plan->estLandingWeightLb, mlw = flight_->limits.mlwLb;
    return q(std::format("Landing weight {} lb is {}% of MLW ({} lb).", formatNumber(ldw),
                         static_cast<long long>(ldw) * 100 / mlw, formatNumber(mlw)));
}
