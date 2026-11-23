// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline as the app sees it: one call opens the database, reads the config files, syncs and settles the
// leases, and then answers the screens' questions. Anything that goes wrong becomes a notice for the screens; the
// session still shows what the database holds.

#pragma once

#include <filesystem>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "airline_config.h"
#include "airline_queries.h"
#include "catalog.h"
#include "dates.h"
#include "money.h"
#include "sqlite_db.h"

namespace blocktime {

struct AirlinePaths {
    std::filesystem::path database, catalog, airlineConfig;
};

AirlinePaths defaultAirlinePaths();  // all in %LOCALAPPDATA%\BlockTime

class AirlineSession {
public:
    // Opens the database, reads both config files and, if they're valid, syncs types and fleets, posts the opening
    // cash and settles the leases up to `today`. Config drift warnings go to `warnings` (errors.log in the app).
    static AirlineSession start(const AirlinePaths& paths, Date today, std::ostream& warnings);

    // Settles the leases again (the minute tick), with the config read at start.
    void tick(Date today);

    // The notices from start, plus those of the latest settle.
    const std::vector<std::string>& notices() const { return notices_; }

    std::vector<AirplaneView> airplanes();
    std::vector<LedgerView> ledger();
    Cents cash();
    std::string airlineName() const;  // "" without a valid airline.yaml

private:
    void settle(Date today);

    std::optional<db::Database> db_;
    std::optional<Catalog> catalog_;
    std::optional<AirlineConfig> config_;
    std::vector<std::string> startNotices_;
    std::vector<std::string> notices_;
};

}  // namespace blocktime
