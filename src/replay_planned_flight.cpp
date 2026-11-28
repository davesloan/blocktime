// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "replay_planned_flight.h"

ReplayPlannedFlight::ReplayPlannedFlight(std::optional<blocktime::PlannedFlightView> view, RecorderBridge& bridge,
                                         QString logFlightId, QObject* parent)
    : FlightSource(parent), bridge_(bridge), logFlightId_(std::move(logFlightId)), flight_(std::move(view)) {
    connect(&bridge_, &RecorderBridge::blockEvent, this, [this](const QString& type) {
        // At block in Your Flight shows the flight's summary, as a real flight does once settled (D-134).
        if (type == "block_in") {
            flight_.reset();
            summaryFlightId_ = logFlightId_;
        }
        emit changed();
    });
}


// A settled flight replayed: accepted before block out, flying, then the arrival view and the summary.
blocktime::YourFlightStage ReplayPlannedFlight::stage() const {
    blocktime::StageInputs in;
    in.planned = in.planImported = in.accepted = flight_.has_value();
    in.flightOpen = bridge_.flightOpen();
    in.summaryShowing = !summaryFlightId_.isEmpty();
    in.finishable = finishable();
    return blocktime::yourFlightStage(in);
}
