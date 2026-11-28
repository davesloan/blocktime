// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "new_flight_model.h"

#include <chrono>
#include <format>
#include <random>

#include <QVariantMap>

#include "dates.h"
#include "money_lines_qml.h"
#include "simbrief.h"

using namespace blocktime;

namespace {

std::string utcNowInstant() {
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

QString cabinLine(const CabinLoad& c, FlightKind kind) {
    const QString n = QString::fromStdString(formatNumber(c.passengers));
    if (kind == FlightKind::Vip) return n + " VIP passengers";
    if (kind == FlightKind::Charter) return n + " charter passengers";
    return n + " " + QString::fromStdString(c.cabinClass);
}

QString holdName(const std::string& hold) {
    if (hold == "main_deck") return "Main deck";
    QString name = QString::fromStdString(hold);
    name[0] = name[0].toUpper();
    return name;
}

}  // namespace

NewFlightModel::NewFlightModel(db::Database& db, QString airlineIcao, std::filesystem::path ratesFile, Planner& planner,
                               QObject* parent)
    : QObject(parent), db_(db), airlineIcao_(std::move(airlineIcao)), ratesFile_(std::move(ratesFile)),
      planner_(planner) {}

bool NewFlightModel::open(const QString& registration) {
    const auto airplane = loadFlightAirplane(db_, registration.toStdString());
    if (!airplane) return false;
    // Day or night by the origin's local time now (D-104); the UTC hour if the zone isn't known.
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const int hour = localHour(airplane->originTimeZone, now)
                         .value_or(static_cast<int>(std::chrono::hh_mm_ss{now - std::chrono::floor<std::chrono::days>(now)}.hours().count()));
    auto engine = std::make_shared<std::mt19937_64>(std::random_device{}());
    Roll roll = [engine] { return std::uniform_real_distribution<double>(0.0, 1.0)(*engine); };
    std::string ratesError;
    const auto rates = loadRates(ratesFile_, &ratesError);
    ratesOk_ = rates.has_value();
    draft_.emplace(*airplane, rates.value_or(Rates{}), isDaytime(hour), std::move(roll));
    ++request_;
    destination_.clear();
    error_ = ratesOk_ ? QString() : QString::fromStdString(ratesError);
    busy_ = false;
    emit changed();
    return true;
}

void NewFlightModel::plan() {
    if (!canPlan()) return;
    busy_ = true;
    error_.clear();
    emit changed();
    // The flight as it is now, planned at the click (D-105); the runways are filled in when they arrive.
    const NewPlannedFlight clicked = draft_->toNew(utcNowInstant(), {});
    const std::string airframeId = draft_->airplane().simbriefAirframeId;
    const int request = ++request_;
    planner_.fetchRunways(QString::fromStdString(clicked.originIcao), QString::fromStdString(clicked.destinationIcao),
                          [this, clicked, airframeId, request](AtisRunways runways, QString note) {
        if (request != request_) return;  // cancelled, or New Flight reopened since
        busy_ = false;
        std::string error;
        NewPlannedFlight flight = clicked;
        flight.originRunway = runways.origin;
        flight.destinationRunway = runways.destination;
        if (!createPlannedFlight(db_, flight, &error)) {
            error_ = QString::fromStdString(error);
            emit changed();
            return;
        }
        DispatchRequest r;
        r.origin = flight.originIcao;
        r.destination = flight.destinationIcao;
        r.airlineIcao = airlineIcao_.toStdString();
        r.flightNumber = flight.flightNumber;
        r.airframeId = airframeId;
        r.originRunway = flight.originRunway;
        r.destinationRunway = flight.destinationRunway;
        for (const CabinLoad& c : flight.cabins) r.passengers += c.passengers;
        r.cargoLb = flight.cargoLb;
        planner_.openUrl(QString::fromStdString(dispatchUrl(r)));
        emit changed();
        emit planned(note);
    });
}

void NewFlightModel::cancel() {
    ++request_;
    if (busy_) {
        busy_ = false;
        emit changed();
    }
}

QString NewFlightModel::registration() const {
    return draft_ ? QString::fromStdString(draft_->airplane().registration) : QString();
}

QString NewFlightModel::airplaneText() const {
    if (!draft_) return {};
    return QString::fromStdString(draft_->airplane().registration + " · " + draft_->airplane().typeCode);
}

QString NewFlightModel::origin() const {
    if (!draft_) return {};
    return QString::fromStdString(draft_->airplane().originIcao + " · " + draft_->airplane().originName);
}

QStringList NewFlightModel::kinds() const {
    QStringList out;
    if (draft_) {
        for (FlightKind k : draft_->kinds()) out << QString::fromUtf8(kindCode(k).data(), static_cast<qsizetype>(kindCode(k).size()));
    }
    return out;
}

QStringList NewFlightModel::kindLabels() const {
    QStringList out;
    if (draft_) {
        for (FlightKind k : draft_->kinds()) out << kindLabel(k);
    }
    return out;
}

QString NewFlightModel::kindLabel(FlightKind kind) {
    const std::string_view label = blocktime::kindLabel(kind);
    return QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size()));
}

QString NewFlightModel::kind() const {
    if (!draft_) return {};
    const auto code = kindCode(draft_->kind());
    return QString::fromUtf8(code.data(), static_cast<qsizetype>(code.size()));
}

void NewFlightModel::setKind(const QString& code) {
    const auto kind = parseKind(code.toStdString());
    if (!draft_ || !kind) return;
    for (FlightKind k : draft_->kinds()) {
        if (k == *kind) {
            draft_->setKind(k);
            emit changed();
        }
    }
}

bool NewFlightModel::bellyCargoAvailable() const { return draft_ && draft_->bellyCargoAvailable(); }
bool NewFlightModel::bellyCargo() const { return draft_ && draft_->bellyCargo(); }

void NewFlightModel::setBellyCargo(bool on) {
    if (!draft_) return;
    draft_->setBellyCargo(on);
    emit changed();
}

void NewFlightModel::setDestination(const QString& typed) {
    if (!draft_) return;
    destination_ = typed;
    draft_->setDestination(checkDestination(db_, draft_->airplane(), typed.toStdString()));
    emit changed();
}

QString NewFlightModel::destinationMessage() const {
    if (!draft_ || destination_.trimmed().isEmpty()) return {};
    return QString::fromStdString(draft_->destination().message);
}

QString NewFlightModel::flightNumber() const { return draft_ ? QString::fromStdString(draft_->flightNumber()) : QString(); }

void NewFlightModel::setFlightNumber(const QString& number) {
    if (!draft_) return;
    draft_->setFlightNumber(number.trimmed().toStdString());
    emit changed();
}

QStringList NewFlightModel::loadLines() const {
    if (!draft_) return {};
    if (draft_->kind() == FlightKind::Delivery) return {"Delivery flight: no passengers or cargo"};
    const LoadRoll l = draft_->loads();
    QStringList out;
    for (const CabinLoad& c : l.cabins) out << cabinLine(c, draft_->kind());
    const int bags = bagsLb(l.cabins, draft_->airplane().limits);
    if (bags > 0) out << "Bags " + QString::fromStdString(formatNumber(bags)) + " lb";
    if (l.cargoLb > 0) out << "Cargo " + QString::fromStdString(formatNumber(l.cargoLb)) + " lb";
    QStringList holds;
    for (const HoldLoad& h : draft_->holds()) {
        holds << holdName(h.hold) + " " + QString::fromStdString(formatNumber(h.weightLb)) + " lb";
    }
    if (bags + l.cargoLb > 0) out << holds.join(" · ");
    return out;
}

QVariantList NewFlightModel::incomeLines() const {
    const auto p = draft_ ? draft_->preview() : std::nullopt;
    return p ? moneyLinesToQml(p->income) : QVariantList{};
}

QVariantList NewFlightModel::costLines() const {
    const auto p = draft_ ? draft_->preview() : std::nullopt;
    return p ? moneyLinesToQml(p->costs) : QVariantList{};
}

QString NewFlightModel::totalText() const {
    const auto p = draft_ ? draft_->preview() : std::nullopt;
    return p ? QString::fromStdString(formatMoneyFull(p->totalCents)) : QString();
}

QString NewFlightModel::fuelNote() const {
    const auto p = draft_ ? draft_->preview() : std::nullopt;
    return p && !p->fuelIncluded ? QStringLiteral("+ fuel") : QString();
}

QString NewFlightModel::endsUpText() const {
    if (!draft_ || !draft_->destination().ok) return {};
    return QString::fromStdString(draft_->airplane().registration + " ends up at " + draft_->destination().icao);
}

bool NewFlightModel::canPlan() const { return draft_ && ratesOk_ && draft_->canPlan() && !busy_; }
