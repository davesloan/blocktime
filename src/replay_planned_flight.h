// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The replay's planned flight (docs/replay.md, D-175): a settled flight as it was planned, shown on the replay window's
// Live. It follows the replay bridge (Replay, then flying, then the summary at block in) and changes nothing: no
// import, no cancel, no settling.

#pragma once

#include <optional>

#include <QString>
#include <QtQml/qqmlregistration.h>

#include "planned_flight.h"
#include "planned_flight_display.h"
#include "rates.h"
#include "recorder_bridge.h"

class ReplayPlannedFlight : public PlannedFlightDisplay {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the replay")

public:
    // `view` is the settled flight (flownFlightView); without one Live shows no planned flight.
    ReplayPlannedFlight(std::optional<blocktime::PlannedFlightView> view, QString airlineIcao, blocktime::Rates rates,
                        RecorderBridge& bridge, QString logFlightId, QObject* parent = nullptr);

    QString state() const override;
    QString stateText() const override { return QStringLiteral("Replay"); }

private:
    RecorderBridge& bridge_;
    QString logFlightId_;
};
