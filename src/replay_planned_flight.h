// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The replay's planned flight (docs/replay.md, D-175): a settled flight as it was planned, shown on the replay window's
// Your Flight. It follows the replay bridge (accepted, then flying, the arrival view and the summary at block in)
// and changes nothing.

#pragma once

#include <optional>

#include <QString>
#include <QtQml/qqmlregistration.h>

#include "planned_flight.h"
#include "flight_source.h"
#include "recorder_bridge.h"

class ReplayPlannedFlight : public FlightSource {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the replay")

public:
    // `view` is the settled flight (flownFlightView); without one Your Flight shows no flight.
    ReplayPlannedFlight(std::optional<blocktime::PlannedFlightView> view, RecorderBridge& bridge, QString logFlightId,
                        QObject* parent = nullptr);

    const std::optional<blocktime::PlannedFlightView>& view() const override { return flight_; }
    blocktime::YourFlightStage stage() const override;
    QString summaryFlightId() const override { return summaryFlightId_; }

private:
    RecorderBridge& bridge_;
    QString logFlightId_;
    std::optional<blocktime::PlannedFlightView> flight_;
    QString summaryFlightId_;
};
