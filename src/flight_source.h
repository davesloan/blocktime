// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The flight the dashboard's panels show (review 3.1, 3.2): its planned view and Your Flight's stage. Your Flight's
// model is the real one; the replay has a read-only one of a settled flight (D-175). The header, Pre-Flight and the
// in-flight panel read only this, so they show the same in both.

#pragma once

#include <optional>

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

#include "planned_flight.h"
#include "your_flight_stage.h"

class FlightSource : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the app")
    // "empty", "form", "dispatched", "imported", "accepted", "flying", "arrived", "summary" (your_flight_stage.h).
    Q_PROPERTY(QString stage READ stageText NOTIFY changed)

public:
    // The flight on show; empty once settled (the summary) or with nothing planned.
    virtual const std::optional<blocktime::PlannedFlightView>& view() const = 0;
    virtual blocktime::YourFlightStage stage() const = 0;
    QString stageText() const { return QString::fromUtf8(blocktime::stageName(stage())); }
    // The settled flight whose summary is held; "" with none.
    virtual QString summaryFlightId() const = 0;

    // After block in until Settle Up (the in-flight panel's arrival view): the in-flight model sets it.
    bool finishable() const { return finishable_; }
    void setFinishable(bool on);

signals:
    void changed();

protected:
    explicit FlightSource(QObject* parent) : QObject(parent) {}

private:
    bool finishable_ = false;
};
