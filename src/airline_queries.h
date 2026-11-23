// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Read-only views of the airline database for the screens: the airplanes, the ledger and the cash (docs/database.md).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalog.h"
#include "dates.h"
#include "money.h"
#include "sqlite_db.h"

namespace blocktime {

struct AirplaneView {
    std::string registration, tailNumber /* "" = none */, typeCode, airframeName, fleet, locationIcao;
    int fuelOnBoardLb = 0;
    bool awaitingDelivery = false;
    std::optional<Date> returnedOn;  // set once the lease has ended
    std::string returnReason;        // "" while leased, "unpaid"
};

struct LedgerView {
    std::string postedAt;  // UTC instant, YYYY-MM-DDTHH:MM:SSZ
    std::string text;      // "Opening cash", "Lease N814SX, 22 Nov – 21 Dec"
    Cents amountCents = 0;
};

std::vector<AirplaneView> listAirplanes(db::Database& db);  // by registration
std::vector<LedgerView> listLedger(db::Database& db);       // newest first, then the latest written first

Cents cashCents(db::Database& db);                                // the sum of every ledger line
Cents cashCentsAsOf(db::Database& db, std::string_view instant);  // lines posted at or before `instant`
int airportCount(db::Database& db);

// The airplane's limits (D-100), or nullopt if it has none yet.
std::optional<AirplaneLimits> airplaneLimits(db::Database& db, std::int64_t airplaneId);

}  // namespace blocktime
