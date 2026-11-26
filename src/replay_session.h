// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One replay (docs/replay.md, D-175): a settled flight's log lines fed to a fresh RecorderBridge at 1×, 2×, 3× or 10×,
// through the same apply* methods BridgeObserver calls, so everything from the bridge up is today's code. Reads the
// log only; nothing that writes or calls out is connected to its bridge.

#pragma once

#include <chrono>
#include <filesystem>
#include <optional>

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include "flight_replay.h"
#include "planned_flight.h"
#include "rates.h"
#include "recorder_bridge.h"
#include "dashboard_header.h"
#include "replay_planned_flight.h"

class ReplaySession : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(RecorderBridge* bridge READ bridge NOTIFY restarted)
    Q_PROPERTY(PlannedFlightDisplay* plannedFlight READ plannedFlight NOTIFY restarted)
    Q_PROPERTY(DashboardHeader* header READ header NOTIFY restarted)
    Q_PROPERTY(double speed READ speed NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(bool finished READ finished NOTIFY stateChanged)
    Q_PROPERTY(QString title READ title CONSTANT)   // "XLY1231 KLAS → KSAN · 24 Nov"
    Q_PROPERTY(QString error READ error CONSTANT)   // why there's nothing to play; "" normally

public:
    // `view` is the settled flight as planned (flownFlightView); nullopt shows no planned flight.
    ReplaySession(std::filesystem::path logDir, QString flightId, std::optional<blocktime::PlannedFlightView> view,
                  QString airlineIcao, blocktime::Rates rates, QString airlineName = {}, QObject* parent = nullptr);

    RecorderBridge* bridge() const { return bridge_; }
    PlannedFlightDisplay* plannedFlight() const { return planned_; }
    DashboardHeader* header() const { return header_; }
    double speed() const { return clock_.speed(); }
    bool paused() const { return clock_.paused(); }
    bool finished() const { return finished_; }
    QString title() const { return title_; }
    QString error() const { return error_; }

    Q_INVOKABLE void setSpeed(double speed);
    Q_INVOKABLE void setPaused(bool paused);
    // From the start again, with a fresh bridge and planned flight; plays at once.
    Q_INVOKABLE void restart();
    // Starts the timer (about every 50 ms, only while playing).
    void start();
    // Feeds every line up to `at` at once (`--at`: a phase, "block_out", "block_in" or "H:MM" after block out). False
    // when the flight has no such point.
    bool seek(const QString& at);
    // Advances the replay by `wall` times the speed and feeds the lines now due. The timer calls it.
    void tick(std::chrono::milliseconds wall);

signals:
    void restarted();
    void stateChanged();

private:
    void makeBridge();
    void feedUpTo(std::int64_t offsetMs);
    void feed(const blocktime::ReplayItem& item);
    void updateTimer();

    blocktime::ReplayTimeline timeline_;
    blocktime::ReplayClock clock_;
    std::size_t next_ = 0;
    std::optional<std::int64_t> feeding_;  // the offset of the line being fed: the bridge's clock while it is
    bool finished_ = false;
    std::optional<blocktime::PlannedFlightView> view_;
    QString flightId_, airlineIcao_, airlineName_, title_, error_;
    blocktime::Rates rates_;
    RecorderBridge* bridge_ = nullptr;
    ReplayPlannedFlight* planned_ = nullptr;
    DashboardHeader* header_ = nullptr;
    QTimer timer_;
    QElapsedTimer sinceTick_;
};
