// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Collects what milestone 10's analysis needs from SayIntentions while a flight is open (D-123): the ATC log about
// once a minute and once more at block in, the origin's ATIS every 10 minutes from block out to liftoff, the
// destination's from top of descent to touchdown. What's new goes onto the AppEventQueue, which the recorder writes
// to the event log (D-127). A failed call logs nothing and is simply tried again at the next poll.

#pragma once

#include <chrono>

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include "app_event_queue.h"
#include "atc_log.h"
#include "flight_match.h"
#include "planner.h"
#include "recorder_bridge.h"

class FlightWatcher : public QObject {
    Q_OBJECT

public:
    FlightWatcher(Planner& planner, RecorderBridge& bridge, const blocktime::FlightTargetSlot& target,
                  blocktime::AppEventQueue& queue, QObject* parent = nullptr);

    // 60 s and 10 min in the app; tests use a few milliseconds.
    void setIntervals(std::chrono::milliseconds atc, std::chrono::milliseconds atis);

private:
    void onBlockEvent(const QString& type, const QString& flightId);
    void onFlightEvent(const QString& type, const QVariantMap& data);
    void fetchAtc();
    void fetchAtis(const QString& icao, const QString& role);
    void stopAll();

    Planner& planner_;
    const blocktime::FlightTargetSlot& target_;
    blocktime::AppEventQueue& queue_;
    QTimer atcTimer_, originTimer_, destinationTimer_;
    QString flightId_, origin_, destination_;
    blocktime::CommsDedupe comms_;
    blocktime::AtisDedupe atis_;
};
