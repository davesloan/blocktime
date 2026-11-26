// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dashboard_header.h"

#include <cmath>
#include <format>

#include <QTimeZone>

#include <nlohmann/json.hpp>

#include "header_text.h"
#include "status_lines.h"

using namespace blocktime;

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }

// Dave's local time, "1:22 PM".
QString localText(const QDateTime& t) { return t.toLocalTime().toString("h:mm AP"); }

QDateTime fromIso(const QString& iso) {
    QDateTime t = QDateTime::fromString(iso, Qt::ISODateWithMs);
    return t.isValid() ? t : QDateTime::fromString(iso, Qt::ISODate);
}

std::string keyOf(const std::optional<PlannedFlightView>& f) {
    if (!f) return {};
    return f->origin + "|" + f->destination + "|" + f->flightNumber + "|" + f->plannedAt + "|" +
           (f->plan ? f->plan->requestId : std::string()) + "|" + std::to_string(f->route.size());
}

}  // namespace

DashboardHeader::DashboardHeader(RecorderBridge& bridge, PlannedFlightDisplay& planned, QString airlineIcao,
                                 QString airlineName, SettledFlight settledFlight, QObject* parent)
    : QObject(parent),
      bridge_(bridge),
      planned_(planned),
      airlineIcao_(std::move(airlineIcao)),
      airlineName_(std::move(airlineName)),
      settledFlight_(std::move(settledFlight)) {
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, &DashboardHeader::changed);
    connect(&planned_, &PlannedFlightDisplay::changed, this, &DashboardHeader::refreshFlight);
    connect(&bridge_, &RecorderBridge::sampleChanged, this, &DashboardHeader::onSample);
    connect(&bridge_, &RecorderBridge::flightEvent, this, &DashboardHeader::onFlightEvent);
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) { onBlockEvent(type); });
    refreshFlight();
}

void DashboardHeader::refreshFlight() {
    std::optional<PlannedFlightView> next = planned_.view();
    if (!next && planned_.state() == "summary") {
        // The flight just settled is the one already held; at startup it comes from the database.
        if (!flight_ && settledFlight_) next = settledFlight_(planned_.summaryFlightId());
        else next = flight_;
    }
    const std::string key = keyOf(next);
    if (key != flightKey_) {
        flightKey_ = key;
        flight_ = std::move(next);
        progress_ = RouteProgress(flight_ ? flight_->route : std::vector<RoutePoint>{});
        arc_.reset();
        airborne_ = false;
        liftoff_ = blockIn_ = QDateTime();
        liftoffRunway_.clear();
        arrivalGate_.clear();
        parking_.clear();
        StatusContext c;
        if (flight_) {
            c.destinationCity = flight_->destinationCity;
            c.departureRunway = flight_->originRunway;
            if (flight_->plan) {
                c.cruiseAltitudeFt = flight_->plan->cruiseAltitudeFt;
                c.transitionAltitudeFt = flight_->plan->originTransitionAltitudeFt;
                c.transitionLevelFt = flight_->plan->destinationTransitionLevelFt;
                if (c.departureRunway.empty()) c.departureRunway = flight_->plan->originRunway;
            }
        }
        bridge_.setStatusContext(c);
    }
    updateTimer();
    emit changed();
}

void DashboardHeader::resetFlight() {
    progress_.reset();
    arc_.reset();
    airborne_ = false;
    liftoff_ = blockIn_ = QDateTime();
    liftoffRunway_.clear();
    arrivalGate_.clear();
    parking_.clear();
}

void DashboardHeader::onSample() {
    if (!airborne_) return;
    const TelemetrySample& s = bridge_.lastSample();
    progress_.update(s.latitudeDeg, s.longitudeDeg);
    arc_.record(progress_.share(), bridge_.altitudeFt());
    emit changed();
}

void DashboardHeader::onFlightEvent(const QString& type, const QVariantMap& data) {
    if (type == "liftoff") {
        airborne_ = true;
        liftoff_ = fromIso(data.value("time").toString());
        liftoffRunway_ = data.value("runway").toString();
    } else if (type == "arrival_gate") {
        const nlohmann::json d{{"result", data.value("result").toString().toStdString()},
                               {"gate", data.value("gate").toString().toStdString()},
                               {"si_gate", data.value("si_gate").toString().toStdString()}};
        arrivalGate_ = q(arrivalGateName(d));
    } else {
        return;
    }
    emit changed();
}

void DashboardHeader::onBlockEvent(const QString& type) {
    if (type == "block_out" || type == "flight_interrupted") {
        resetFlight();
    } else if (type == "block_in") {
        progress_.finish();
        airborne_ = false;
        blockIn_ = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(bridge_.nowMs()), QTimeZone::UTC);
        parking_ = bridge_.blockInParking();
    }
    updateTimer();
    emit changed();
}

void DashboardHeader::updateTimer() {
    const bool moving = visible() && (bridge_.flightOpen() || (clockIsPlan() && planBlockS() > 0));
    if (moving && !timer_.isActive()) timer_.start();
    if (!moving) timer_.stop();
}

int DashboardHeader::planBlockS() const { return flight_ && flight_->plan ? flight_->plan->estBlockS : 0; }

bool DashboardHeader::visible() const {
    return flight_.has_value() && (planned_.hasFlight() || bridge_.flightOpen() || planned_.state() == "summary");
}

QString DashboardHeader::flightNumber() const { return flight_ ? q(flight_->flightNumber) : QString(); }
QString DashboardHeader::flightText() const { return flight_ ? "Flight " + q(flight_->flightNumber) : QString(); }

QString DashboardHeader::citiesText() const {
    return flight_ ? q(flight_->originCity) + " to " + q(flight_->destinationCity) : QString();
}

bool DashboardHeader::clockIsPlan() const { return !bridge_.flightOpen() && !blockIn_.isValid(); }

QString DashboardHeader::clockText() const {
    const QDateTime out = bridge_.blockOutTime();
    if (bridge_.flightOpen() && out.isValid()) {
        const qint64 s = (static_cast<qint64>(bridge_.nowMs()) - out.toMSecsSinceEpoch()) / 1000;
        return q(durationText(std::chrono::seconds(s), true));
    }
    if (blockIn_.isValid() && out.isValid()) return q(durationText(std::chrono::seconds(out.secsTo(blockIn_)), false));
    return planBlockS() > 0 ? q(durationText(std::chrono::seconds(planBlockS()), false)) : QStringLiteral("--:--");
}

QString DashboardHeader::blockOutText() const {
    const QDateTime out = bridge_.blockOutTime();
    return (bridge_.flightOpen() || blockIn_.isValid()) && out.isValid() ? localText(out) : QStringLiteral("--:--");
}

QString DashboardHeader::blockInText() const {
    if (blockIn_.isValid()) return localText(blockIn_);
    std::optional<TimePoint> out;
    if (bridge_.flightOpen() && bridge_.blockOutTime().isValid()) {
        out = TimePoint(std::chrono::milliseconds(bridge_.blockOutTime().toMSecsSinceEpoch()));
    }
    const auto estimate = blockInEstimate(out, bridge_.now(), planBlockS());
    if (!estimate) return QStringLiteral("--:--");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(estimate->time_since_epoch()).count();
    return "~" + localText(QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC));
}

QString DashboardHeader::originIcao() const { return flight_ ? q(flight_->origin) : QString(); }
QString DashboardHeader::originName() const { return flight_ ? q(flight_->originName) : QString(); }

QString DashboardHeader::originLine() const {
    if (!flight_) return {};
    if (liftoff_.isValid()) {
        QString line = "Off " + localText(liftoff_);
        if (!liftoffRunway_.isEmpty()) line += QString::fromUtf8(" · rwy ") + liftoffRunway_;
        return line;
    }
    QString line = q(flight_->registration) + QString::fromUtf8(" · ") + q(flight_->typeCode);
    if (!flight_->loadInParking.empty()) line += QString::fromUtf8(" · ") + q(flight_->loadInParking);
    return line;
}

QString DashboardHeader::destinationIcao() const { return flight_ ? q(flight_->destination) : QString(); }
QString DashboardHeader::destinationName() const { return flight_ ? q(flight_->destinationName) : QString(); }
QString DashboardHeader::destinationLine() const { return !parking_.isEmpty() ? parking_ : arrivalGate_; }

QString DashboardHeader::markerText() const {
    if (!bridge_.hasSample()) return {};
    const int alt = static_cast<int>(std::lround(bridge_.altitudeFt()));
    const int ta = effectiveTransitionFt(flight_ && flight_->plan ? flight_->plan->originTransitionAltitudeFt : 0);
    const int tl = effectiveTransitionFt(flight_ && flight_->plan ? flight_->plan->destinationTransitionLevelFt : 0);
    const bool fl = isFlightLevel(bridge_.phaseCode(), alt, ta, tl);
    return q(altitudeText(alt, fl)) + QString::fromUtf8(" · ") + QString::number(std::lround(bridge_.gsKt())) + " kt";
}

QString DashboardHeader::distanceText() const {
    if (progress_.totalNm() <= 0) return {};
    return QString::fromUtf8("%1 NM flown · %2 NM to go")
        .arg(std::lround(progress_.flownNm()))
        .arg(std::lround(progress_.toGoNm()));
}

QVariantList DashboardHeader::arcShares() const {
    const int cruise = flight_ && flight_->plan ? flight_->plan->cruiseAltitudeFt : 0;
    QVariantList out;
    for (const double peak : arc_.peaks()) out.append(peak < 0 ? -1.0 : altitudeShare(peak, cruise));
    return out;
}
