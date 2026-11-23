// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The Live page before departure (design.md §13.2-13.3): the planned flight, "Waiting for plan" until Dave imports
// the SimBrief plan with a button (D-101), then "Waiting for sim"; the route to copy, where to load in, the money
// estimate, and Cancel flight.

#pragma once

#include <optional>

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "planned_flight.h"
#include "planner.h"
#include "recorder_bridge.h"
#include "sqlite_db.h"

class PlannedFlightModel : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    Q_PROPERTY(bool hasFlight READ hasFlight NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString stateText READ stateText NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString airplaneText READ airplaneText NOTIFY changed)
    Q_PROPERTY(QString kindText READ kindText NOTIFY changed)
    Q_PROPERTY(QString loadInText READ loadInText NOTIFY changed)
    Q_PROPERTY(QString runwayText READ runwayText NOTIFY changed)
    Q_PROPERTY(QString route READ route NOTIFY changed)
    Q_PROPERTY(QString planText READ planText NOTIFY changed)
    Q_PROPERTY(QVariantList estimateLines READ estimateLines NOTIFY changed)
    Q_PROPERTY(QString totalText READ totalText NOTIFY changed)
    Q_PROPERTY(QString fuelNote READ fuelNote NOTIFY changed)
    Q_PROPERTY(QString warning READ warning NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)

public:
    PlannedFlightModel(blocktime::db::Database& db, QString airlineIcao, Planner& planner, RecorderBridge& bridge,
                       QObject* parent = nullptr);

    // Reads the planned flight again; `message` replaces the shown message (planning's note about the runways).
    Q_INVOKABLE void reload(const QString& message = {});
    Q_INVOKABLE void openDispatchPage();
    Q_INVOKABLE void importPlan();
    Q_INVOKABLE void cancelFlight();
    Q_INVOKABLE void copyRoute();

    bool hasFlight() const { return flight_.has_value(); }
    QString state() const;
    QString stateText() const;
    QString title() const;
    QString airplaneText() const;
    QString kindText() const;
    QString loadInText() const;
    QString runwayText() const;
    QString route() const;
    QString planText() const;
    QVariantList estimateLines() const;
    QString totalText() const;
    QString fuelNote() const;
    QString warning() const;
    QString message() const { return message_; }
    bool busy() const { return busy_; }

signals:
    void changed();
    void flightChanged();  // planned, imported or cancelled: the Fleet screen refreshes

private:
    blocktime::db::Database& db_;
    QString airlineIcao_;
    Planner& planner_;
    RecorderBridge& bridge_;
    std::optional<blocktime::PlannedFlightView> flight_;
    QString message_;
    bool busy_ = false;
};
