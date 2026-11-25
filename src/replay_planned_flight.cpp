// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "replay_planned_flight.h"

ReplayPlannedFlight::ReplayPlannedFlight(std::optional<blocktime::PlannedFlightView> view, QString airlineIcao,
                                         blocktime::Rates rates, RecorderBridge& bridge, QString logFlightId,
                                         QObject* parent)
    : PlannedFlightDisplay(std::move(airlineIcao), true, parent), bridge_(bridge), logFlightId_(std::move(logFlightId)) {
    flight_ = std::move(view);
    rates_ = rates;
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) {
        // At block in Live shows the flight's summary, as a real flight does once settled (D-134).
        if (type == "block_in") {
            flight_.reset();
            summaryFlightId_ = logFlightId_;
        }
        emit changed();
    });
}

QString ReplayPlannedFlight::state() const {
    if (!summaryFlightId_.isEmpty()) return QStringLiteral("summary");
    return bridge_.flightOpen() ? QStringLiteral("flying") : QStringLiteral("replay");
}
