// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dashboard_header.h"

#include <cmath>
#include <format>

#include <QTimeZone>

#include "header_text.h"
#include "status_lines.h"

using namespace blocktime;

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }

// Dave's local time, "1:22 PM".
QString localText(const QDateTime& t) { return t.toLocalTime().toString("h:mm AP"); }

QString localText(TimePoint t) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    return localText(QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC));
}

TimePoint toPoint(const QDateTime& t) { return TimePoint(std::chrono::milliseconds(t.toMSecsSinceEpoch())); }

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
    bool settledBefore = false;  // a summary from the database: this session saw none of its block events
    if (!next) {
        // No flight on show: settling (PlannedFlightModel reloads before it sets the summary), the summary itself, or
        // nothing planned. The flight held stays (visible() hides it); only a different flight replaces it. At startup
        // with a summary showing, it comes from the database.
        if (flight_ || planned_.state() != "summary" || !settledFlight_) {
            updateTimer();
            emit changed();
            return;
        }
        next = settledFlight_(planned_.summaryFlightId());
        settledBefore = next.has_value();
    }
    const std::string key = keyOf(next);
    if (key != flightKey_) {
        flightKey_ = key;
        flight_ = std::move(next);
        settledBefore_ = settledBefore;
        progress_ = RouteProgress(flight_ ? flight_->route : std::vector<RoutePoint>{});
        const int takeoffFuelLb = flight_ && flight_->brief ? flight_->brief->rampLb - flight_->brief->taxiLb : 0;
        timeline_ = flight_ ? PlanTimeline(flight_->planFixes, progress_, takeoffFuelLb) : PlanTimeline();
        arc_.reset();
        airborne_ = false;
        liftoff_ = touchdown_ = blockIn_ = QDateTime();
        sampleAt_.reset();
        parking_.clear();
        if (settledBefore_) progress_.finish();  // its summary: the whole route was flown
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
    settledBefore_ = false;
    progress_.reset();
    arc_.reset();
    airborne_ = false;
    liftoff_ = touchdown_ = blockIn_ = QDateTime();
    sampleAt_.reset();
    parking_.clear();
}

void DashboardHeader::onSample() {
    if (!airborne_) return;
    const TelemetrySample& s = bridge_.lastSample();
    progress_.update(s.latitudeDeg, s.longitudeDeg);
    sampleAt_ = bridge_.now();
    arc_.record(progress_.share(), bridge_.altitudeFt());
    emit changed();
}

void DashboardHeader::onFlightEvent(const QString& type, const QVariantMap& data) {
    if (type == "touchdown") {
        touchdown_ = fromIso(data.value("time").toString());
        emit changed();
        return;
    }
    if (type != "liftoff") return;
    airborne_ = true;
    // The recorder logs liftoff once per flight, so this only clears a touchdown in tests; a balked landing after
    // ground contact stays on the ground branch (rare, not handled).
    touchdown_ = QDateTime();
    liftoff_ = fromIso(data.value("time").toString());
    emit changed();
}

void DashboardHeader::onBlockEvent(const QString& type) {
    if (type == "block_out" || type == "flight_interrupted") {
        resetFlight();
    } else if (type == "block_in") {
        progress_.finish();
        airborne_ = false;
        blockIn_ = bridge_.blockInTime();  // the block in event's time: the brake set
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

bool DashboardHeader::clockIsPlan() const {
    return !bridge_.flightOpen() && !blockIn_.isValid() && !settledBefore_;
}

QString DashboardHeader::clockText() const {
    const QDateTime out = bridge_.blockOutTime();
    if (bridge_.flightOpen() && out.isValid()) {
        const qint64 s = (static_cast<qint64>(bridge_.nowMs()) - out.toMSecsSinceEpoch()) / 1000;
        return q(durationText(std::chrono::seconds(s), true));
    }
    if (blockIn_.isValid() && out.isValid()) return q(durationText(std::chrono::seconds(out.secsTo(blockIn_)), false));
    if (settledBefore_) return QStringLiteral("--:--");  // no block events this session
    return planBlockS() > 0 ? q(durationText(std::chrono::seconds(planBlockS()), false)) : QStringLiteral("--:--");
}

std::optional<TimePoint> DashboardHeader::blockOutPoint() const {
    const QDateTime out = bridge_.blockOutTime();
    if (!out.isValid() || !(bridge_.flightOpen() || blockIn_.isValid())) return std::nullopt;
    return toPoint(out);
}

Predictions DashboardHeader::predictions() const {
    if (!flight_ || settledBefore_) return {};
    FlightNow f{bridge_.now()};
    f.blockOut = blockOutPoint();
    if (liftoff_.isValid()) f.liftoff = toPoint(liftoff_);
    if (touchdown_.isValid()) f.touchdown = toPoint(touchdown_);
    if (f.liftoff && !f.touchdown && sampleAt_ && *sampleAt_ >= *f.liftoff) f.now = *sampleAt_;
    f.flownNm = progress_.flownNm();
    f.fuelLb = static_cast<int>(std::lround(bridge_.fuelLb()));
    f.phase = bridge_.phaseCode();
    return predict(timeline_, TaxiTimes{flight_->taxiOutS, flight_->taxiInS}, f);
}

std::optional<TimePoint> DashboardHeader::arrivalEstimate() const {
    if (blockIn_.isValid()) return toPoint(blockIn_);
    if (!timeline_.empty()) return predictions().arrival;
    return blockInEstimate(blockOutPoint(), bridge_.now(), planBlockS());
}

std::optional<TimePoint> DashboardHeader::punctualityReference() const {
    if (!flight_ || settledBefore_) return std::nullopt;
    if (!timeline_.empty()) return predictions().reference;
    const auto out = blockOutPoint();
    if (!out || planBlockS() <= 0) return std::nullopt;
    return *out + std::chrono::seconds(planBlockS());
}

QString DashboardHeader::originIcao() const { return flight_ ? q(flight_->origin) : QString(); }
QString DashboardHeader::originName() const { return flight_ ? q(flight_->originName) : QString(); }

QString DashboardHeader::originLine() const {
    if (!flight_) return {};
    if (settledBefore_) return QStringLiteral("Departed");  // no block events this session
    if (!liftoff_.isValid() && !blockIn_.isValid()) return QStringLiteral("Departing soon");
    const QDateTime out = bridge_.blockOutTime();
    return out.isValid() ? "Departed " + localText(out) : QStringLiteral("Departed");
}

QString DashboardHeader::destinationIcao() const { return flight_ ? q(flight_->destination) : QString(); }
QString DashboardHeader::destinationName() const { return flight_ ? q(flight_->destinationName) : QString(); }
QString DashboardHeader::destinationLine() const {
    if (!flight_) return {};
    if (settledBefore_) return QStringLiteral("Arrived");
    if (blockIn_.isValid()) {
        QString line = "Arrived " + localText(blockIn_);
        if (!parking_.isEmpty()) line += QString::fromUtf8(" · ") + parking_;
        return line;
    }
    const auto estimate = arrivalEstimate();
    return estimate ? "Arrival " + localText(*estimate) : QString();
}

QString DashboardHeader::punctualityText() const {
    const auto reference = punctualityReference(), arrival = arrivalEstimate();
    if (!reference || !arrival) return {};
    return q(blocktime::punctualityText(blocktime::punctuality(*arrival, *reference), blockIn_.isValid()));
}

QString DashboardHeader::punctuality() const {
    const auto reference = punctualityReference(), arrival = arrivalEstimate();
    if (!reference || !arrival) return {};
    switch (blocktime::punctuality(*arrival, *reference).kind) {
        case Punctuality::OnTime: return QStringLiteral("on_time");
        case Punctuality::Early: return QStringLiteral("early");
        case Punctuality::Delayed: return QStringLiteral("delayed");
    }
    return {};
}

double DashboardHeader::todShare() const {
    const auto tod = timeline_.fixNm("TOD");
    return tod && progress_.totalNm() > 0 ? *tod / progress_.totalNm() : -1.0;
}

QString DashboardHeader::distanceText() const {
    if (progress_.totalNm() <= 0) return {};
    const QString line = QString::fromUtf8("%1 NM flown · %2 NM to go")
                             .arg(std::lround(progress_.flownNm()))
                             .arg(std::lround(progress_.toGoNm()));
    if (!airborne_ || touchdown_.isValid()) return line;
    // D-210: feet below the transition, a flight level above it; indicated airspeed, in Mach by D-140's rule.
    const int altFt = static_cast<int>(std::lround(bridge_.altitudeFt()));
    const SimbriefPlan* plan = flight_ && flight_->plan ? &*flight_->plan : nullptr;
    const bool level = isFlightLevel(bridge_.phaseCode(), altFt,
                                     effectiveTransitionFt(plan ? plan->originTransitionAltitudeFt : 0),
                                     effectiveTransitionFt(plan ? plan->destinationTransitionLevelFt : 0));
    const QString speed =
        bridge_.showMach() ? bridge_.machText() : QString("%1 KT").arg(std::lround(bridge_.iasKt()));
    return line + QString::fromUtf8(" · ") + q(altitudeText(altFt, level)).toUpper() + QString::fromUtf8(" · ") +
           speed;
}

QVariantList DashboardHeader::arcShares() const {
    const int cruise = flight_ && flight_->plan ? flight_->plan->cruiseAltitudeFt : 0;
    QVariantList out;
    for (const double peak : arc_.peaks()) out.append(peak < 0 ? -1.0 : altitudeShare(peak, cruise));
    return out;
}
