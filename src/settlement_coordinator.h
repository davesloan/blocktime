// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// When to settle the planned flight (D-113, review 3.2): after block in, once the log has the flown pair and
// rates.yaml reads. A flight that was flown but can't be settled yet stays pending, and the app's minute tick asks
// again (review 1.2). Plain C++, so Your Flight's model only shows the outcome.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "flight_events.h"
#include "flight_settlement.h"
#include "sqlite_db.h"

namespace blocktime {

class SettlementCoordinator {
public:
    SettlementCoordinator(db::Database& db, std::filesystem::path logDir, std::filesystem::path ratesFile);

    struct Outcome {
        enum class Kind { Nothing, Waiting, Settled } kind = Kind::Nothing;
        std::string waitingWhy;  // "the block in isn't in the log yet. BlockTime tries again every minute."
        std::optional<SettledFlight> settled;
        std::vector<LogLine> lines;  // the settled flight's log lines, for Claude's review (read once, review 5.1)
        std::string registration;    // the settled flight's airplane
    };

    // Settles the planned flight if the log shows it flown. `blockInSeen` marks it pending until it settles: the
    // block_in line may not be in the log yet (D-049).
    Outcome check(bool blockInSeen);
    bool pending() const { return pending_; }

private:
    db::Database& db_;
    std::filesystem::path logDir_, ratesFile_;
    bool pending_ = false;
};

}  // namespace blocktime
