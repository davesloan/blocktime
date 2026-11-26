// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recorder_bridge.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMetaObject>
#include <QRegularExpression>
#include <QTimeZone>
#include <QUrl>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <type_traits>
#include <variant>

#include "airspeed_display.h"
#include "event_log.h"
#include "phase_pill.h"
#include "telemetry_filter.h"

using namespace blocktime;

namespace {

QString pounds(double lb) { return QLocale(QLocale::English).toString(qRound64(lb)); }

QDateTime toQDateTime(TimePoint t) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time_since_epoch()).count();
    return QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC);
}

}  // namespace

RecorderBridge::RecorderBridge(QObject* parent) : QObject(parent) {}

void RecorderBridge::openLogFolder() const {
    if (!logDir_.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(logDir_));
}

double RecorderBridge::nowMs() const {
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now().time_since_epoch()).count());
}

QDateTime RecorderBridge::nowUtc() const { return toQDateTime(now()); }

void RecorderBridge::setReplay() { setConnection("replay"); }

void RecorderBridge::setConnection(const QString& c) {
    if (c == connection_) return;
    connection_ = c;
    emit connectionChanged();
}

void RecorderBridge::applyStarted(const QString& logDir) {
    logDir_ = logDir;
    emit logDirChanged();
    events_.add("info", "Recording to " + logDir, nowUtc());
}

void RecorderBridge::applyLine(const QString& line, bool error) {
    // Event lines start with the time ("20:30:11  BLOCK OUT ..."); the feed has its own time column.
    static const QRegularExpression timePrefix(R"(^\d\d:\d\d:\d\d  )");
    QString text = line;
    text.remove(timePrefix);
    QString kind = "info";
    if (error || text.startsWith("WARNING")) {
        kind = "warn";
    } else if (text.startsWith("NOT RECORDING")) {
        kind = "warn";
    } else if (text.startsWith("BLOCK ") || text.startsWith("FLIGHT INTERRUPTED")) {
        kind = "block";
    }
    events_.add(kind, text.simplified(), nowUtc());
}

void RecorderBridge::applyConnection(ConnectionState state) {
    switch (state) {
        case ConnectionState::WaitingForSim: setConnection("waiting"); break;
        case ConnectionState::SimStarting: setConnection("starting"); break;
        case ConnectionState::Connected: setConnection("connected"); break;
    }
    // Live values from a session that ended are stale.
    if (state != ConnectionState::Connected && hasSample_) {
        hasSample_ = false;
        emit sampleChanged();
    }
    if (state != ConnectionState::Connected && match_) {
        match_.reset();
        emit matchChanged();
    }
    if (state != ConnectionState::Connected) setReleaseRefused(false);
}

void RecorderBridge::applySample(const TelemetrySample& s) {
    lastSample_ = s;
    hasSample_ = true;
    altitudeFt_ = s.indicatedAltitudeFt;
    iasKt_ = s.indicatedAirspeedKt;
    gsKt_ = s.groundSpeedKt;
    vsFpm_ = s.verticalSpeedFpm;
    fuelLb_ = s.fuelTotalLb;
    parkingBrake_ = s.parkingBrake != 0;
    onGround_ = s.onGround != 0;
    enginesRunning_ = blocktime::enginesRunning(s);
    engineCount_ = std::clamp(s.numEngines, 0, 4);
    engineRunning_.clear();
    for (int i = 0; i < engineCount_; ++i) engineRunning_.append(s.engCombustion[i] != 0);
    showMach_ = blocktime::showMach(showMach_, s.mach);
    machText_ = QString::fromStdString(blocktime::machText(s.mach));
    aircraft_ = QString::fromUtf8(simString(s.title, sizeof(s.title)));
    atcId_ = QString::fromUtf8(simString(s.atcId, sizeof(s.atcId)));
    if (parkingBrake_) setReleaseRefused(false);
    emit sampleChanged();
    // The shutdown checklist after block in (D-135); the stages ignore it at any other time.
    const ShutdownChecks checks{enginesRunning_ == 0, parkingBrake_, s.lightBeacon == 0, s.lightStrobe == 0};
    const std::string iso = formatUtc(now());
    const bool stagesMoved = stages_.onShutdownChecks(checks, iso);
    if (status_.onShutdownChecks(checks, iso) || stagesMoved) emit stagesChanged();
}

void RecorderBridge::showNextLeg() {
    if (!stages_.showingFlight()) return;
    stages_.showNextLeg();
    status_.showNextLeg();
    emit stagesChanged();
}

void RecorderBridge::applyBlockEvent(const BlockEvent& event) {
    const nlohmann::json data = toJson(event);
    std::visit(
        [this, &data](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, BlockOut>) {
                feedStages("block_out", data);  // a fresh flight; its pushback follows (D-128)
                emit stagesChanged();
                setReleaseRefused(false);
                flightOpen_ = true;
                blockOutTime_ = toQDateTime(e.time);
                blockOutFuelLb_ = e.fuelLb;
            } else if constexpr (std::is_same_v<T, BlockIn>) {
                feedStages("block_in", data);  // the flight stays on Live until the summary goes (D-134)
                emit stagesChanged();
                flightOpen_ = false;
                lastFlightSummary_ = QString("Block in %1:%2 · %3 → %4 lb")
                                         .arg(e.blockTimeS / 3600)
                                         .arg(e.blockTimeS / 60 % 60, 2, 10, QChar('0'))
                                         .arg(pounds(blockOutFuelLb_), pounds(e.fuelLb));
            } else if constexpr (std::is_same_v<T, BlockOutRefused>) {
                // No flight opened; the feed line says why (D-088).
                setReleaseRefused(true);
            } else {
                // A recorder_crash at startup closes a flight this app never saw open; the summary still applies.
                feedStages("flight_interrupted", data);
                emit stagesChanged();
                flightOpen_ = false;
                lastFlightSummary_ = QString("Interrupted (%1)").arg(QString::fromStdString(e.reason));
            }
        },
        event);
    const QString type = QString::fromUtf8(std::string(eventType(event)));
    const QString id = std::visit(
        [](const auto& e) {
            if constexpr (requires { e.flightId; }) return QString::fromStdString(e.flightId);
            else return QString();
        },
        event);
    if (!std::holds_alternative<BlockOutRefused>(event)) emit flightChanged();
    emit blockEvent(type, id);
}

void RecorderBridge::setReleaseRefused(bool refused) {
    if (refused == releaseRefused_) return;
    releaseRefused_ = refused;
    emit releaseRefusedChanged();
}

void RecorderBridge::applyMatch(const MatchResult& m) {
    if (match_ && *match_ == m) return;
    match_ = m;
    emit matchChanged();
}

void RecorderBridge::setBlocked() {
    setConnection("blocked");
    events_.add("warn", "BlockTime is already recording in another window, so this one isn't. Close the other "
                        "one, then restart this one.",
                nowUtc());
}

void RecorderBridge::applyAirport(blocktime::AirportFacilities airport) {
    const QString icao = QString::fromStdString(airport.icao);
    airports_[airport.icao] = std::move(airport);
    emit airportLoaded(icao);
}

const blocktime::AirportFacilities* RecorderBridge::airport(const QString& icao) const {
    const auto it = airports_.find(icao.toStdString());
    return it == airports_.end() ? nullptr : &it->second;
}

void RecorderBridge::applyFlightEvent(const QString& type, const QVariantMap& data) {
    const QByteArray json = QJsonDocument(QJsonObject::fromVariantMap(data)).toJson(QJsonDocument::Compact);
    feedStages(type.toStdString(), nlohmann::json::parse(json.toStdString(), nullptr, false));
    emit stagesChanged();
    emit flightEvent(type, data);
}

void RecorderBridge::feedStages(std::string_view type, const nlohmann::json& data) {
    stages_.onEvent(type, data);
    status_.onEvent(type, data);
}

void RecorderBridge::setStatusContext(const blocktime::StatusContext& context) {
    status_.setContext(context);
    emit stagesChanged();
}

namespace {

const char* const kStates[] = {"upcoming", "current", "done"};

// "2026-11-25T18:55:00Z" → "10:55 AM" in Dave's time zone; "" for none.
QString localTime(const std::string& iso) {
    if (iso.empty()) return {};
    const QDateTime t = QDateTime::fromString(QString::fromStdString(iso), Qt::ISODateWithMs);
    return t.isValid() ? t.toLocalTime().toString("h:mm AP") : QString();
}

}  // namespace

QVariantList RecorderBridge::statusLines() const {
    QVariantList out;
    for (const StatusLine& l : status_.lines()) {
        out.append(QVariantMap{{"text", QString::fromStdString(l.text)}, {"time", localTime(l.startIso)}});
    }
    return out;
}

QVariantList RecorderBridge::checklist() const {
    QVariantList out;
    for (const ChecklistItem& c : status_.checklist()) {
        out.append(QVariantMap{{"text", QString::fromStdString(c.text)}, {"done", c.done}});
    }
    return out;
}

QVariantList RecorderBridge::pill() const {
    QVariantList out;
    for (const PillSection& p : pillSections(stages_.view(), stages_.phase())) {
        QVariantList divisions;
        for (const StageState s : p.divisions) divisions.append(QString(kStates[static_cast<int>(s)]));
        out.append(QVariantMap{{"name", QString::fromStdString(p.name)},
                               {"state", kStates[static_cast<int>(p.state)]},
                               {"divisions", divisions}});
    }
    return out;
}

QVariantList RecorderBridge::stages() const {
    QVariantList out;
    for (const StageView& v : stages_.view()) {
        QVariantList lines;
        for (const StageLine& l : v.lines) {
            QVariantList checks;
            for (const ChecklistItem& c : l.checks) {
                checks.append(QVariantMap{{"text", QString::fromStdString(c.text)}, {"done", c.done}});
            }
            lines.append(QVariantMap{{"text", QString::fromStdString(l.text)},
                                     {"start", QString::fromStdString(l.start)},
                                     {"end", QString::fromStdString(l.end)},
                                     {"running", l.running},
                                     {"checks", checks}});
        }
        out.append(QVariantMap{{"name", QString::fromStdString(v.name)},
                               {"state", kStates[static_cast<int>(v.state)]},
                               {"lines", lines}});
    }
    return out;
}

void RecorderBridge::applyStopped(int result) {
    if (result != 0) events_.add("warn", "Couldn't write to the log folder, so nothing is being recorded.", nowUtc());
}

void BridgeObserver::onStarted(const std::filesystem::path& logDir) {
    const QString dir = QString::fromStdWString(logDir.wstring());
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, dir] { b->applyStarted(dir); }, Qt::QueuedConnection);
}

void BridgeObserver::onLine(const std::string& line, bool error) {
    // Error lines (LOST: events, D-049) also go to std::cerr at once, which the app sends to its error log (D-067):
    // they come while the app shuts down, when the posted copy below is never shown.
    if (error) std::cerr << line << std::endl;
    const QString text = QString::fromUtf8(line);
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, text, error] { b->applyLine(text, error); },
                              Qt::QueuedConnection);
}

void BridgeObserver::onConnection(ConnectionState state) {
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, state] { b->applyConnection(state); }, Qt::QueuedConnection);
}

void BridgeObserver::onTelemetry(const TelemetrySample& s) {
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, s] { b->applySample(s); }, Qt::QueuedConnection);
}

void BridgeObserver::onBlockEvent(const BlockEvent& e) {
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, e] { b->applyBlockEvent(e); }, Qt::QueuedConnection);
}

void BridgeObserver::onMatch(const MatchResult& m) {
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, m] { b->applyMatch(m); }, Qt::QueuedConnection);
}

void BridgeObserver::onAirport(const blocktime::AirportFacilities& airport) {
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, airport] { b->applyAirport(airport); }, Qt::QueuedConnection);
}

void BridgeObserver::onFlightEvent(const std::string& type, const nlohmann::json& data) {
    const QString t = QString::fromStdString(type);
    const QVariantMap d = QJsonDocument::fromJson(QByteArray::fromStdString(data.dump())).object().toVariantMap();
    QMetaObject::invokeMethod(&bridge_, [b = &bridge_, t, d] { b->applyFlightEvent(t, d); }, Qt::QueuedConnection);
}
