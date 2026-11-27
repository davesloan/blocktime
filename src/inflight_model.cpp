// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inflight_model.h"

#include <algorithm>
#include <cmath>
#include <format>

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include "atc_message.h"
#include "cockpit_reading.h"
#include "debrief_measure.h"
#include "debrief_score.h"
#include "event_log.h"
#include "inflight_readouts.h"
#include "money.h"
#include "pricing.h"

using namespace blocktime;

namespace {

constexpr double kInHgToHpa = 33.8639;
constexpr double kFuelScaleStepLb = 6000;  // the fuel gauge's scale: the block-out fuel rounded up to this (D-201)
constexpr double kGroundSpeedFaceKt = 40;  // D-196

QString q(const std::string& s) { return QString::fromStdString(s); }

nlohmann::json jsonOf(const QVariantMap& data) {
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact);
    return nlohmann::json::parse(json.toStdString(), nullptr, false);
}

// "2026-11-25T18:57:56Z" → "10:57 AM" in the PC's time zone.
QString localTime(const std::string& iso) {
    const QDateTime t = QDateTime::fromString(q(iso), Qt::ISODateWithMs);
    return t.isValid() ? t.toLocalTime().toString("h:mm AP") : QString();
}

double number(const nlohmann::json& j, const char* key) {
    return j.contains(key) && j[key].is_number() ? j[key].get<double>() : 0.0;
}

struct LampLook {
    const char* name;
    const char* lens;
};
constexpr LampLook kLamps[] = {{"BEACON", "red"},     {"STROBE", "white"}, {"LANDING", "white"}, {"SEAT BELTS", "amber"},
                               {"APU", "amber"},      {"ALT STD", "green"}, {"GEAR DN", "green"}};

const char* phaseTitleOf(PanelPhase p) {
    switch (p) {
        case PanelPhase::Departure: return "DEPARTURE";
        case PanelPhase::Climb: return "CLIMB";
        case PanelPhase::Cruise: return "CRUISE";
        case PanelPhase::Arrival: return "ARRIVAL";
        case PanelPhase::ShortFinal: return "SHORT FINAL";
        case PanelPhase::Landed: return "LANDED";
    }
    return "";
}

}  // namespace

InFlightModel::InFlightModel(RecorderBridge& bridge, PlannedFlightDisplay& planned, int vipAttendantsTimes,
                             QObject* parent)
    : QObject(parent), bridge_(bridge), planned_(planned), vipAttendantsTimes_(vipAttendantsTimes) {
    flash_.setInterval(1000);  // 1 s lit, 1 s dark (D-195)
    connect(&flash_, &QTimer::timeout, this, [this] {
        flashOn_ = !flashOn_;
        emit flashChanged();
    });
    connect(&planned_, &PlannedFlightDisplay::changed, this, &InFlightModel::refreshFlight);
    connect(&bridge_, &RecorderBridge::flightEvent, this, &InFlightModel::onFlightEvent);
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) { onBlockEvent(type); });
    connect(&bridge_, &RecorderBridge::sampleChanged, this, &InFlightModel::onSample);
    connect(&bridge_, &RecorderBridge::flightChanged, this, &InFlightModel::rebuild);
    refreshFlight();
    // The first fill isn't a change: animate comes on once the event loop runs, unless a replay's burst began.
    QTimer::singleShot(0, this, [this] {
        if (catchingUp_) return;
        animate_ = true;
        emit animateChanged();
    });
}

void InFlightModel::beginCatchUp() {
    catchingUp_ = true;
    if (animate_) {
        animate_ = false;
        emit animateChanged();
    }
}

void InFlightModel::endCatchUp() {
    // Queued after the burst's own queued calls, which run first (one thread's queued calls run in order).
    QTimer::singleShot(0, this, [this] {
        catchingUp_ = false;
        animate_ = true;
        emit animateChanged();
    });
}

void InFlightModel::setFlashInterval(std::chrono::milliseconds interval) { flash_.setInterval(interval); }

void InFlightModel::refreshFlight() {
    const auto& view = planned_.view();
    if (!view) {  // block in (the summary takes over) or nothing planned: keep what we had until the next flight
        hasFlight_ = false;
        rebuild();
        return;
    }
    const bool newFlight = !hasFlight_ || view->plannedAt != flight_.plannedAt || view->origin != flight_.origin;
    flight_ = *view;
    hasFlight_ = true;
    if (newFlight) {
        board_.reset(familyOf(flight_.family));
        face_.reset();
        phase_ = "at_gate";
        onFinal_ = touchedDown_ = false;
        cockpit_.reset();
        altimeter_.reset();
        destinationAtis_.reset();
        latestAtis_.reset();
        touchdown_.reset();
        stars_.reset();
        gate_.clear();
    }
    log_.setFlight(flight_.origin, flight_.plannedAt);
    const PlanBrief brief = flight_.brief.value_or(PlanBrief{});
    card_.setFlight(flight_.plannedAt, RoutePlan{brief.sid, brief.sidTransition});
    rebuild();
}

void InFlightModel::onFlightEvent(const QString& qtype, const QVariantMap& data) {
    const std::string type = qtype.toStdString();
    const nlohmann::json d = jsonOf(data);
    const auto now = bridge_.now();
    const std::string time = d.is_object() ? d.value("time", std::string()) : std::string();
    board_.onEvent(type, d, now);
    log_.onEvent(type, d);
    if (type == "atc_exchange") {
        if (const auto m = atcMessage(d)) card_.onMessage(*m);
    } else if (type == "atis") {
        latestAtis_ = d;
        if (d.value("role", "") == "destination") destinationAtis_ = d;
    } else if (type == "cockpit") {
        cockpit_ = d;
    } else if (type == "altimeter") {
        altimeter_ = d;
    } else if (type == "phase_changed") {
        phase_ = d.value("phase", phase_);
        if (phase_ == "go_around") {
            onFinal_ = false;
            face_.onGoAround();
            card_.onGoAround(time);
        }
    } else if (type == "flight_marker") {
        if (d.value("marker", "") == "final_approach") {
            onFinal_ = true;
            face_.onFinalApproach();
        }
    } else if (type == "touchdown") {
        touchedDown_ = true;
        touchdown_ = d;
        card_.onTouchdown(time);
    } else if (type == "bounces" && touchdown_) {
        std::optional<int> vref;
        if (cockpit_ && cockpit_->value("pmdg", false) && number(*cockpit_, "vref_kt") > 0) {
            vref = static_cast<int>(number(*cockpit_, "vref_kt"));
        }
        stars_ = landingStars(landingFacts(*touchdown_, d.value("bounces", 0), vref, nullptr), familyOf(flight_.family));
    } else if (type == "arrival_gate") {
        gate_ = d.value("gate", "");
    } else {
        return;  // nothing the panel shows
    }
    rebuild();
}

void InFlightModel::onBlockEvent(const QString& type) {
    const auto now = bridge_.now();
    log_.onEvent(type.toStdString(), nlohmann::json::object());
    if (type == "block_out") {
        board_.onBlockOut(now);
        face_.reset();
        phase_ = "pushback";
        onFinal_ = touchedDown_ = false;
    } else if (type == "block_in") {
        board_.onBlockIn(now);
    } else if (type == "flight_interrupted") {
        // The same planned flight is flown again (D-108): the attempt's windows go, the cockpit's state stays.
        board_.reset(familyOf(flight_.family));
        if (cockpit_) board_.onEvent("cockpit", *cockpit_, now);
        if (altimeter_) board_.onEvent("altimeter", *altimeter_, now);
        face_.reset();
        phase_ = "at_gate";
        onFinal_ = touchedDown_ = false;
        touchdown_.reset();
        stars_.reset();
        gate_.clear();
        destinationAtis_.reset();
    }
    rebuild();
}

void InFlightModel::onSample() {
    board_.onSample(bridge_.lastSample(), bridge_.now());
    face_.onSample(bridge_.lastSample().altitudeAglFt);
    rebuild();
}

bool InFlightModel::visible() const { return bridge_.flightOpen(); }

QString InFlightModel::speedFace() const {
    const TelemetrySample& s = bridge_.lastSample();
    return s.onGround != 0 && s.groundSpeedKt < kGroundSpeedFaceKt ? QStringLiteral("gs") : QStringLiteral("ias");
}

double InFlightModel::speedKt() const {
    return speedFace() == "gs" ? bridge_.gsKt() : bridge_.iasKt();
}

QString InFlightModel::machText() const { return bridge_.showMach() ? bridge_.machText() : QString(); }

double InFlightModel::vrefBugKt() const {
    const bool approaching = phase_ == "approach" || phase_ == "go_around" || onFinal_;
    if (!approaching || touchedDown_ || !cockpit_) return -1;
    const double vref = number(*cockpit_, "vref_kt");
    return vref > 0 ? vref : -1;
}

QString InFlightModel::altimeterSetting() const {
    if (!altimeter_) return {};
    if (altimetersStd(*altimeter_)) return QStringLiteral("STD");
    const double mb = number(*altimeter_, "capt_mb");
    // In the unit of the latest ATIS: inches in the US ("30.09"), hectopascals elsewhere ("1013").
    bool inches = true;
    if (latestAtis_ && latestAtis_->contains("altimeter")) {
        const std::string a = latestAtis_->value("altimeter", "");
        inches = a.empty() || std::atof(a.c_str()) < 100;
    }
    return inches ? QString::number(mb / kInHgToHpa, 'f', 2) : QString::number(std::lround(mb));
}

double InFlightModel::fuelScaleLb() const {
    const double atBlockOut = bridge_.blockOutFuelLb() > 0 ? bridge_.blockOutFuelLb() : bridge_.fuelLb();
    return std::max(kFuelScaleStepLb, std::ceil(atBlockOut / kFuelScaleStepLb) * kFuelScaleStepLb);
}

double InFlightModel::lowFuelLb() const { return flight_.brief ? flight_.brief->reserveLb : 0; }

QString InFlightModel::fuelText() const {
    return q(formatNumber(std::llround(bridge_.fuelLb() / 10.0) * 10));
}

void InFlightModel::rebuild() {
    const auto now = bridge_.now();

    // The lamps, and the flash while one is wrong (the one continuous animation, D-195).
    QVariantList lamps;
    bool wrong = false;
    for (const LampState& s : board_.lamps(now)) {
        const LampLook& look = kLamps[static_cast<int>(s.lamp)];
        lamps.append(QVariantMap{{"name", look.name}, {"lens", look.lens}, {"known", s.known}, {"lit", s.lit},
                                 {"wrong", s.wrong}});
        wrong = wrong || s.wrong;
    }
    lamps_ = lamps;
    if (wrong && !flash_.isActive()) {
        flash_.start();
    } else if (!wrong && flash_.isActive()) {
        flash_.stop();
        flashOn_ = true;
        emit flashChanged();
    }

    // The ATC card: on the ground before the takeoff roll, airborne, or after touchdown.
    const bool beforeRoll = phase_ == "at_gate" || phase_ == "pushback" || phase_ == "taxi_out";
    const CardStage stage = touchedDown_ ? CardStage::Arrival : beforeRoll ? CardStage::Departure : CardStage::Airborne;
    QVariantList cardSlots;
    for (const AtcSlot& s : card_.slotsFor(stage)) {
        if (stage == CardStage::Departure && s.title == "TAXI") cardSlots.append(QVariantMap{{"spacer", true}});  // SPEED's place
        cardSlots.append(QVariantMap{{"title", q(s.title)},
                                 {"value", q(s.value)},
                                 {"aside", s.aside.empty() ? localTime(s.time) : q(s.aside)},
                                 {"latest", s.latest},
                                 {"spacer", false}});
    }
    atcSlots_ = cardSlots;

    // The phase's readouts, SOULS and FUEL REMAINING first (D-194, D-203).
    InFlightFacts f;
    if (hasFlight_) {
        f.departure = departureView(flight_, log_);
        f.clearance = clearanceView(flight_, log_);
        f.cruiseLevelFt = flight_.plan ? flight_.plan->cruiseAltitudeFt : 0;
        f.plan = flight_.brief.value_or(PlanBrief{});
        for (const CabinLoad& c : flight_.cabins) f.passengers += c.passengers;
        const double blockHours = flight_.plan ? flight_.plan->estBlockS / 3600.0 : 0;
        f.crew = crewFor(flight_.role, flight_.kind == FlightKind::Delivery, flight_.seatsInstalled, blockHours,
                         vipAttendantsTimes_);
    }
    f.destinationAtis = destinationAtis_ ? &*destinationAtis_ : nullptr;
    f.atcStar = card_.star();
    f.atcRunway = card_.approachRunway();
    f.towerWind = card_.landingWind();
    f.fuelLb = static_cast<int>(std::lround(bridge_.fuelLb()));
    if (cockpit_) {
        f.vrefKt = static_cast<int>(number(*cockpit_, "vref_kt"));
        f.landingFlaps = static_cast<int>(number(*cockpit_, "landing_flaps"));
    }
    f.stars = stars_;
    if (touchdown_) f.touchdownFpm = static_cast<int>(std::lround(std::abs(number(*touchdown_, "vs_fpm"))));
    f.gate = gate_;
    const PanelPhase phase = panelPhase(phase_, onFinal_, touchedDown_);
    phaseTitle_ = phaseTitleOf(phase);
    QVariantList readouts;
    for (const ReadoutView& r : inFlightReadouts(phase, f)) {
        readouts.append(QVariantMap{{"title", q(r.title)},
                                    {"value", q(r.value)},
                                    {"unit", q(r.unit)},
                                    {"tag", q(r.tag)},
                                    {"flagged", r.flagged},
                                    {"words", r.words},
                                    {"wide", r.wide},
                                    {"ticks", r.ticks},
                                    {"stars", r.stars}});
    }
    readouts_ = readouts;
    emit changed();
}
