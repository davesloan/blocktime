// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "recorder_bridge.h"

#include <QDesktopServices>
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

void RecorderBridge::setConnection(const QString& c) {
    if (c == connection_) return;
    connection_ = c;
    emit connectionChanged();
}

void RecorderBridge::applyStarted(const QString& logDir) {
    logDir_ = logDir;
    emit logDirChanged();
    events_.add("info", "Recording to " + logDir);
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
    events_.add(kind, text.simplified());
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
}

void RecorderBridge::applySample(const TelemetrySample& s) {
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
    aircraft_ = QString::fromUtf8(simString(s.title, sizeof(s.title)));
    atcId_ = QString::fromUtf8(simString(s.atcId, sizeof(s.atcId)));
    emit sampleChanged();
}

void RecorderBridge::applyBlockEvent(const BlockEvent& event) {
    std::visit(
        [this](const auto& e) {
            using T = std::decay_t<decltype(e)>;
            if constexpr (std::is_same_v<T, BlockOut>) {
                flightOpen_ = true;
                blockOutTime_ = toQDateTime(e.time);
                blockOutFuelLb_ = e.fuelLb;
            } else if constexpr (std::is_same_v<T, BlockIn>) {
                flightOpen_ = false;
                lastFlightSummary_ = QString("Block in %1:%2 · %3 → %4 lb")
                                         .arg(e.blockTimeS / 3600)
                                         .arg(e.blockTimeS / 60 % 60, 2, 10, QChar('0'))
                                         .arg(pounds(blockOutFuelLb_), pounds(e.fuelLb));
            } else if constexpr (std::is_same_v<T, BlockOutRefused>) {
                // No flight opened; the feed line says why (D-088).
            } else {
                // A recorder_crash at startup closes a flight this app never saw open; the summary still applies.
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

void RecorderBridge::applyMatch(const MatchResult& m) {
    if (match_ && *match_ == m) return;
    match_ = m;
    emit matchChanged();
}

void RecorderBridge::setBlocked() {
    setConnection("blocked");
    events_.add("warn", "BlockTime is already recording in another window, so this one isn't. Close the other "
                        "one, then restart this one.");
}

void RecorderBridge::applyStopped(int result) {
    if (result != 0) events_.add("warn", "Couldn't write to the log folder, so nothing is being recorded.");
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
