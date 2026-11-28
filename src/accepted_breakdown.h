// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The accepted breakdown (D-222, D-251): the money lines and their workings as the dispatch report showed them when
// Dave accepted the flight, with each line's rate locked. Linked to the planned flight until settlement, then moved
// to the settled flight (docs/database.md).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "pricing.h"
#include "sqlite_db.h"

namespace blocktime {

struct AcceptedBreakdown {
    std::string acceptedAt;
    bool atBlockOut = false;  // D-250: accepted by block out, not by Accept
    Preview money;            // fuelIncluded is always true: Accept needs a plan
};

// In one transaction: deletes the planned flight's old breakdown if any, then stores this one and links it. A
// re-import after Accept calls it again (D-233).
bool acceptPlannedFlight(db::Database& db, std::int64_t plannedFlightId, const Preview& money,
                         std::string_view acceptedAt, bool atBlockOut, std::string* error);

// The same, inside the caller's transaction (a re-import stores the plan and the new breakdown together).
bool writeAcceptance(db::Database& db, std::int64_t plannedFlightId, const Preview& money, std::string_view acceptedAt,
                     bool atBlockOut, std::string* error);

std::optional<AcceptedBreakdown> plannedAcceptance(db::Database& db, std::int64_t plannedFlightId);
std::optional<AcceptedBreakdown> flightAcceptance(db::Database& db, std::int64_t flightId);

// Moves the planned flight's breakdown to the settled flight. Runs inside the caller's transaction (settlement). True
// with nothing to move.
bool moveAcceptanceToFlight(db::Database& db, std::int64_t plannedFlightId, std::int64_t flightId,
                            std::string* error);

// Deletes the planned flight's breakdown, if any (the link cascades, the breakdown doesn't). Runs inside the caller's
// transaction.
bool dropPlannedAcceptance(db::Database& db, std::int64_t plannedFlightId, std::string* error);

}  // namespace blocktime
