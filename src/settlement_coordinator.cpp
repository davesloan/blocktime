// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settlement_coordinator.h"

#include <iostream>
#include <utility>

#include "accepted_breakdown.h"
#include "rates.h"

namespace blocktime {

SettlementCoordinator::SettlementCoordinator(db::Database& db, std::filesystem::path logDir,
                                             std::filesystem::path ratesFile)
    : db_(db), logDir_(std::move(logDir)), ratesFile_(std::move(ratesFile)) {}

SettlementCoordinator::Outcome SettlementCoordinator::check(bool blockInSeen) {
    if (blockInSeen) pending_ = true;
    recordDeboardings(db_, logDir_);  // one that ended while BlockTime was closed (D-137)
    Outcome out;
    const auto waiting = [&](std::string why) {
        pending_ = true;
        out.kind = Outcome::Kind::Waiting;
        out.waitingWhy = std::move(why);
        return out;
    };
    if (!currentPlannedFlight(db_)) {
        pending_ = false;
        return out;
    }
    auto current = findCurrentFlown(db_, logDir_, false);
    if (!current) {
        // Not in the log yet: a held block_in line (D-049) is written later, and the minute tick tries again.
        if (pending_) return waiting("the block in isn't in the log yet. BlockTime tries again every minute.");
        return out;
    }
    std::string error;
    // Ledger lines are permanent, so a flight is never settled at default rates: it waits until rates.yaml reads. An
    // accepted flight doesn't need them: its rates were locked at Accept (D-222).
    std::optional<Rates> rates = Rates{};
    if (!plannedAcceptance(db_, current->planned.id)) {
        rates = loadRates(ratesFile_, &error);
        if (!rates) return waiting(error + ". Fix rates.yaml; BlockTime tries again every minute.");
    }
    current->flown.lines = loadFlightEvents(logDir_, current->flown.logFlightId);  // the slow part, only now
    auto result = settleFlight(db_, current->planned, current->flown, *rates, &error);
    if (!result) {
        std::cerr << error << std::endl;  // errors.log (D-067)
        return waiting(error + ". BlockTime tries again every minute.");
    }
    if (!result->debriefError.empty()) {
        std::cerr << "Settled " << result->logFlightId << " without its debrief: " << result->debriefError << std::endl;
    }
    pending_ = false;
    out.kind = Outcome::Kind::Settled;
    out.registration = current->planned.registration;
    out.lines = std::move(current->flown.lines);
    out.settled = std::move(result);
    return out;
}

}  // namespace blocktime
