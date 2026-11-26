// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preflight_model.h"

#include <cctype>
#include <format>

#include <QJsonDocument>
#include <QJsonObject>

#include "clearance.h"
#include "money.h"

using namespace blocktime;

namespace {

QString pounds(int lb) { return QString::fromStdString(formatNumber(lb) + " lb"); }

// "economy" → "Economy", "main_deck" → "Main deck".
QString label(std::string s) {
    for (char& c : s) {
        if (c == '_') c = ' ';
    }
    if (!s.empty()) s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    return QString::fromStdString(s);
}

}  // namespace

PreFlightModel::PreFlightModel(RecorderBridge& bridge, PlannedFlightDisplay& planned, QObject* parent)
    : QObject(parent), bridge_(bridge), planned_(planned) {
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, &PreFlightModel::countdownChanged);
    connect(&planned_, &PlannedFlightDisplay::changed, this, &PreFlightModel::refreshFlight);
    connect(&bridge_, &RecorderBridge::flightEvent, this, [this](const QString& type, const QVariantMap& data) {
        if (type != "atc_exchange" && type != "atis" && type != "liftoff") return;
        const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact);
        log_.onEvent(type.toStdString(), nlohmann::json::parse(json.toStdString(), nullptr, false));
        rebuild();
    });
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) {
        log_.onEvent(type.toStdString(), nlohmann::json::object());
        rebuild();
    });
    refreshFlight();
}

void PreFlightModel::refreshFlight() {
    // At block in (or settling) the view empties: the panel hides, and the log keeps what it had until the next flight.
    const auto& view = planned_.view();
    hasFlight_ = view.has_value();
    if (view) {
        flight_ = *view;
        log_.setFlight(flight_.origin, flight_.plannedAt);
    }
    rebuild();
}

void PreFlightModel::rebuild() {
    sheet_ = hasFlight_ ? loadSheet(flight_) : LoadSheet{};
    departure_ = hasFlight_ ? departureView(flight_, log_) : DepartureView{};
    clearance_ = hasFlight_ ? clearanceView(flight_, log_) : ClearanceView{};
    updateTimer();
    emit changed();
    emit countdownChanged();
}

void PreFlightModel::updateTimer() {
    if (countdownVisible() && !timer_.isActive()) {
        timer_.start();
    } else if (!countdownVisible()) {
        timer_.stop();
    }
}

// Until block out: the in-flight panel takes over there (D-194; it was liftoff, D-188).
bool PreFlightModel::visible() const { return hasFlight_ && !log_.blockedOut(); }

QString PreFlightModel::blockFuelText() const {
    return sheet_.hasPlan && sheet_.blockFuelLb > 0 ? pounds(sheet_.blockFuelLb) : QString();
}

QString PreFlightModel::zfwText() const { return sheet_.hasPlan && sheet_.zfwLb > 0 ? pounds(sheet_.zfwLb) : QString(); }

QVariantList PreFlightModel::passengers() const {
    QVariantList out;
    for (const CabinLoad& c : sheet_.passengers) {
        out << QVariantMap{{"label", sheet_.passengers.size() == 1 ? QStringLiteral("Passengers") : label(c.cabinClass)},
                           {"value", QString::fromStdString(formatNumber(c.passengers))}};
    }
    return out;
}

QString PreFlightModel::cargoText() const { return sheet_.hasPlan ? pounds(sheet_.cargoLb) : QString(); }

QVariantList PreFlightModel::holds() const {
    QVariantList out;
    for (const HoldLoad& h : sheet_.holds) out << QVariantMap{{"label", label(h.hold)}, {"value", pounds(h.weightLb)}};
    return out;
}

QString PreFlightModel::cruiseText() const {
    if (!sheet_.hasPlan || sheet_.cruiseAltitudeFt <= 0) return {};
    const int transition = sheet_.transitionAltitudeFt > 0 ? sheet_.transitionAltitudeFt : 18000;
    if (sheet_.cruiseAltitudeFt < transition) return QString::fromStdString(formatNumber(sheet_.cruiseAltitudeFt) + " ft");
    return QStringLiteral("FL%1").arg(sheet_.cruiseAltitudeFt / 100, 3, 10, QChar('0'));
}

QString PreFlightModel::transitionText() const {
    return sheet_.hasPlan && sheet_.transitionAltitudeFt > 0
               ? QString::fromStdString(formatNumber(sheet_.transitionAltitudeFt))
               : QString();
}

QString PreFlightModel::runwayTag() const { return q(departure_.runwayTag); }

QString PreFlightModel::headingText() const {
    return departure_.headingDeg ? QString::fromStdString(std::format("{:03}°", *departure_.headingDeg)) : QString();
}

QString PreFlightModel::sidTag() const {
    if (clearance_.sid.empty()) return {};
    if (clearance_.sidChanged) {
        const std::string planSid = flight_.brief ? flight_.brief->sid : std::string();
        return planSid.empty() ? QStringLiteral("CHANGED") : "PLAN " + q(planSid);
    }
    return clearance_.received ? QStringLiteral("CLEARED") : QStringLiteral("PLAN");
}

bool PreFlightModel::countdownVisible() const {
    return visible() && clearance_.received && log_.clearedAt().has_value() && !log_.blockedOut();
}

QString PreFlightModel::countdownText() const {
    if (!countdownVisible()) return {};
    return q(clearanceCountdown(*log_.clearedAt(), bridge_.now()).text);
}

bool PreFlightModel::countdownUrgent() const {
    return countdownVisible() && clearanceCountdown(*log_.clearedAt(), bridge_.now()).urgent;
}
