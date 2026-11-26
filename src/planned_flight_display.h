// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What Live shows of the planned flight (design.md §13.2-13.3): its title, airplane, runways, route, plan and money
// estimate, and its state. Two kinds show it: PlannedFlightModel, the real one, which settles flights, and
// ReplayPlannedFlight, the replay's read-only copy of a settled flight (D-175). Live and PlannedFlightPanel take this
// base, so the replay shows the same panel without being able to change anything.

#pragma once

#include <optional>

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include "planned_flight.h"
#include "rates.h"

class PlannedFlightDisplay : public QObject {
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
    Q_PROPERTY(QString notRecording READ notRecording NOTIFY notRecordingChanged)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
    Q_PROPERTY(QString summaryFlightId READ summaryFlightId NOTIFY changed)
    // The replay's copy: Live hides Import, Open SimBrief again and Cancel flight.
    Q_PROPERTY(bool readOnly READ readOnly CONSTANT)

public:
    // "", "waitingForPlan", "waitingForSim", "simConnected", "flying", "summary", or "replay" (the replay before block
    // out).
    virtual QString state() const = 0;
    virtual QString stateText() const;
    virtual QString notRecording() const { return {}; }

    Q_INVOKABLE virtual void openDispatchPage() {}
    Q_INVOKABLE virtual void importPlan() {}
    Q_INVOKABLE virtual void cancelFlight() {}
    Q_INVOKABLE virtual void doneForNow() {}
    Q_INVOKABLE virtual void turnaround() {}
    Q_INVOKABLE void copyRoute();

    bool hasFlight() const { return flight_.has_value(); }
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
    QString notice() const { return notice_; }
    QString summaryFlightId() const { return summaryFlightId_; }
    bool readOnly() const { return readOnly_; }
    // The flight on show, for the dashboard header; empty once settled (the summary) or with nothing planned.
    const std::optional<blocktime::PlannedFlightView>& view() const { return flight_; }

signals:
    void changed();
    void notRecordingChanged();

protected:
    PlannedFlightDisplay(QString airlineIcao, bool readOnly, QObject* parent);

    QString airlineIcao_;
    blocktime::Rates rates_;
    std::optional<blocktime::PlannedFlightView> flight_;
    QString message_;
    bool busy_ = false;
    QString notice_;
    QString summaryFlightId_;

private:
    bool readOnly_ = false;
};
