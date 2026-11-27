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
#include <string>
#include <vector>

#include "airline_config.h"
#include "airline_queries.h"
#include "catalog.h"
#include "gate_picker.h"
#include "loads.h"
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
    // Opens the database, reads the aircraft files and airline.yaml and, if they're valid, syncs the types, posts the
    // opening cash and settles the leases up to `today`.
    static AirlineSession start(const AirlinePaths& paths, Date today);
    // start() without settling the leases. The app settles a flown flight first, then calls tick(): a flight that
    // blocked in before a lease month must be in the cash that month is checked against (review 1.4).
    static AirlineSession open(const AirlinePaths& paths);

    // Settles the leases again (the minute tick), with the config read at start. True if anything changed: a
    // charge, a return, or the notices.
    bool tick(Date today);

    // The notices from start, plus those of the latest settle.
    const std::vector<std::string>& notices() const { return notices_; }

    std::vector<AirplaneView> airplanes();
    std::vector<LedgerView> ledger();
    Cents cash();
    double hoursFlown();  // Dave's, from the settled flights (D-114)
    std::string airlineName() const;  // "" without a valid airline.yaml
    std::string airlineIcao() const;  // "" without a valid airline.yaml
    std::string airlineCallsign() const;  // "" without a valid airline.yaml
    // The supported aircraft (D-243); empty if no aircraft file could be read.
    const Catalog& catalog() const;

    // What the arrival gate needs to know (D-147): the type's wingspan from the catalog, gate or ramp from the kind,
    // and the airline's preferred gates at the airport. nullopt for a type the catalog doesn't have.
    std::optional<GateContext> gateContext(const std::string& typeCode, FlightKind kind,
                                           const std::string& destinationIcao) const;

    // The open database for the planning code (planned_flight.h), or nullptr if it didn't open.
    db::Database* database() { return db_ ? &*db_ : nullptr; }

private:
    bool settle(Date today);  // true if it wrote anything

    std::optional<db::Database> db_;
    std::optional<Catalog> catalog_;
    std::optional<AirlineConfig> config_;
    std::vector<std::string> startNotices_;
    std::vector<std::string> notices_;
};

}  // namespace blocktime
