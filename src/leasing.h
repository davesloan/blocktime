// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Milestone 4's airline processes (docs/database.md): types from the catalog (D-093), fleets and starting cash from
// airline.yaml, leasing new airplanes and charging lease months in date order (D-082, D-083, D-096).

#pragma once

#include <string>
#include <vector>

#include "airline_config.h"
#include "catalog.h"
#include "dates.h"
#include "sqlite_db.h"

namespace blocktime {

// Inserts or updates every catalog type by code. Types are reference data (D-093).
bool syncCatalogTypes(db::Database& db, const Catalog& catalog, std::string* error);
// Inserts fleets that aren't in the database yet. Never deletes one.
bool syncFleets(db::Database& db, const AirlineConfig& config, std::string* error);
// Posts the starting cash, rounded to $5, at the start of founded_on, unless it's already posted.
bool postOpeningCash(db::Database& db, const AirlineConfig& config, std::string* error);

struct LeaseOutcome {
    int leased = 0, monthsCharged = 0, returned = 0;  // monthsCharged includes first months
    std::vector<std::string> notices;                 // for the screens, in event order
    std::string error;                                // a database failure; settling stopped there
};

// Leases config airplanes not yet in the database whose lease has started, and charges every lease month that has
// started and isn't charged, all in date order (ties by registration), one transaction per month. A month that
// costs more than the cash on its date isn't charged: a new airplane isn't leased, a leased one is returned.
LeaseOutcome settleLeases(db::Database& db, const Catalog& catalog, const AirlineConfig& config, Date today);

// One line per leased airplane whose config entry changed (the database keeps the leased values, D-082) or that's
// no longer in the config.
std::vector<std::string> configDriftWarnings(db::Database& db, const AirlineConfig& config);

}  // namespace blocktime
