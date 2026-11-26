// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the dashboard's header shows (docs/dashboard.md, "The header" and "Milestone 12: how the header is built";
// D-151, D-180, D-182): the identity row, the route arc and its texts. The status lines, checklist and pill come from
// RecorderBridge. The flight is the planned one while there is one, kept once it settles (the summary), and looked
// up from the database when the app starts with a summary already showing. Live and the replay window each get one.

#pragma once

#include <functional>
#include <optional>

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

#include "planned_flight.h"
#include "planned_flight_display.h"
#include "recorder_bridge.h"
#include "route_progress.h"

class DashboardHeader : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")

    // A planned flight, an open flight or a summary.
    Q_PROPERTY(bool visible READ visible NOTIFY changed)
    Q_PROPERTY(QString airlineIcao READ airlineIcao NOTIFY changed)
    Q_PROPERTY(QString airlineName READ airlineName NOTIFY changed)
    Q_PROPERTY(QString flightNumber READ flightNumber NOTIFY changed)
    Q_PROPERTY(QString flightText READ flightText NOTIFY changed)  // "Flight 1231"
    Q_PROPERTY(QString citiesText READ citiesText NOTIFY changed)  // "San Diego to Las Vegas"
    // The plan's block time before block out (clockIsPlan), the running clock, then the final block time.
    Q_PROPERTY(QString clockText READ clockText NOTIFY changed)
    Q_PROPERTY(bool clockIsPlan READ clockIsPlan NOTIFY changed)
    Q_PROPERTY(QString originIcao READ originIcao NOTIFY changed)
    Q_PROPERTY(QString originName READ originName NOTIFY changed)
    // "Departing soon" until liftoff, then "Departed 1:22 PM" (block out; D-184)
    Q_PROPERTY(QString originLine READ originLine NOTIFY changed)
    Q_PROPERTY(QString destinationIcao READ destinationIcao NOTIFY changed)
    Q_PROPERTY(QString destinationName READ destinationName NOTIFY changed)
    // "Arriving ~2:27 PM" (the estimate), then at block in "Arrived 2:24 PM · Gate 4" (D-184)
    Q_PROPERTY(QString destinationLine READ destinationLine NOTIFY changed)
    Q_PROPERTY(QString distanceText READ distanceText NOTIFY changed)        // "127 NM flown · 103 NM to go"
    Q_PROPERTY(double progress READ progress NOTIFY changed)                 // 0..1 along the route
    // AltitudeArc::kPieces numbers: each piece's altitude as a share of the plan's cruise (0..1), -1 not flown.
    Q_PROPERTY(QVariantList arcShares READ arcShares NOTIFY changed)

public:
    using SettledFlight = std::function<std::optional<blocktime::PlannedFlightView>(const QString& logFlightId)>;

    DashboardHeader(RecorderBridge& bridge, PlannedFlightDisplay& planned, QString airlineIcao, QString airlineName,
                    SettledFlight settledFlight, QObject* parent = nullptr);

    bool visible() const;
    QString airlineIcao() const { return airlineIcao_; }
    QString airlineName() const { return airlineName_; }
    QString flightNumber() const;
    QString flightText() const;
    QString citiesText() const;
    QString clockText() const;
    bool clockIsPlan() const;
    QString originIcao() const;
    QString originName() const;
    QString originLine() const;
    QString destinationIcao() const;
    QString destinationName() const;
    QString destinationLine() const;
    QString distanceText() const;
    double progress() const { return progress_.share(); }
    QVariantList arcShares() const;

signals:
    void changed();

private:
    void refreshFlight();
    void resetFlight();  // block out and interrupts: the arc, the times and the lines start again
    void onSample();
    void onFlightEvent(const QString& type, const QVariantMap& data);
    QString blockInEstimateText() const;  // "~2:27 PM", or "" without an estimate
    void onBlockEvent(const QString& type);
    void updateTimer();
    int planBlockS() const;

    RecorderBridge& bridge_;
    PlannedFlightDisplay& planned_;
    QString airlineIcao_, airlineName_;
    SettledFlight settledFlight_;

    std::optional<blocktime::PlannedFlightView> flight_;
    std::string flightKey_;  // what makes it a different flight
    bool settledBefore_ = false;  // a summary read from the database at startup: no block events this session
    blocktime::RouteProgress progress_{{}};
    blocktime::AltitudeArc arc_;
    bool airborne_ = false;  // from liftoff to block in
    QDateTime liftoff_, blockIn_;
    QString parking_;
    QTimer timer_;  // once a second, only while a clock or an estimate moves
};
