// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airline_session.h"

#include <format>

#include "airline_db.h"
#include "leasing.h"

namespace blocktime {

namespace {

std::string folderOf(const std::filesystem::path& file) { return file.parent_path().string(); }

}  // namespace

AirlinePaths defaultAirlinePaths() {
    return {defaultAirlineDatabaseFile(), defaultCatalogDir(), defaultAirlineConfigFile()};
}

AirlineSession AirlineSession::start(const AirlinePaths& paths, Date today, std::ostream& warnings) {
    AirlineSession s = open(paths, warnings);
    s.settle(today);
    return s;
}

AirlineSession AirlineSession::open(const AirlinePaths& paths, std::ostream& warnings) {
    AirlineSession s;
    AirlineDbOpen opened = openAirlineDatabase(paths.database);
    if (!opened.db) {
        s.startNotices_.push_back("Can't open the airline database: " + opened.error);
        s.notices_ = s.startNotices_;
        return s;
    }
    s.db_ = std::move(opened.db);
    if (opened.testSchemaSetAside) {
        s.startNotices_.push_back("The Qt test's database was set aside as blocktime.test-schema.db.");
    }
    if (!opened.backup.empty()) {
        s.startNotices_.push_back("Updated blocktime.db to a new schema; the copy from before is " +
                                  opened.backup.filename().string() + ".");
    }
    if (opened.versionRestored) {
        s.startNotices_.push_back("An older BlockTime had opened blocktime.db; its schema version was put back.");
    }

    std::string error;
    // The supported aircraft (D-243), shipped next to the executable. A file that doesn't parse is skipped.
    if (Catalog catalog = loadCatalogDir(paths.catalog, &s.startNotices_); !catalog.types.empty()) {
        s.catalog_ = std::move(catalog);
    } else {
        s.startNotices_.push_back(std::format("No aircraft files in {}. Reinstall BlockTime (README).",
                                              paths.catalog.string()));
    }
    if (!std::filesystem::exists(paths.airlineConfig)) {
        s.startNotices_.push_back(std::format(
            "No airline.yaml in {}. Copy config\\airline.example.yaml there as airline.yaml and fill it in (README).",
            folderOf(paths.airlineConfig)));
    } else if (s.config_ = loadAirlineConfig(paths.airlineConfig, &error); !s.config_) {
        s.startNotices_.push_back(error);
    }
    if (airportCount(*s.db_) == 0) s.startNotices_.push_back("No airport data yet. Run blocktime-import (README).");

    if (s.catalog_ && s.config_) {
        if (!syncCatalogTypes(*s.db_, *s.catalog_, &error) || !syncFleets(*s.db_, *s.config_, &error) ||
            !postOpeningCash(*s.db_, *s.config_, &error)) {
            s.startNotices_.push_back("Airline database error: " + error);
            s.catalog_.reset();  // don't settle on a half-synced database
        } else {
            for (const std::string& w : configDriftWarnings(*s.db_, *s.config_)) warnings << w << '\n';
        }
    }
    // Airplanes leased before milestone 5 get their airframe's limits (D-100).
    if (s.db_ && s.catalog_ && s.config_ && !fillMissingLimits(*s.db_, *s.catalog_, *s.config_, &error)) {
        s.startNotices_.push_back("Airline database error: " + error);
    }
    if (s.db_ && s.config_ && !syncFreeFlightNumbers(*s.db_, *s.config_, &error)) {
        s.startNotices_.push_back("Flight numbers in airline.yaml: " + error);
    }
    s.notices_ = s.startNotices_;
    return s;
}

bool AirlineSession::tick(Date today) {
    const std::vector<std::string> before = notices_;
    return settle(today) || notices_ != before;
}

bool AirlineSession::settle(Date today) {
    notices_ = startNotices_;
    if (!db_ || !catalog_ || !config_) return false;
    const LeaseOutcome outcome = settleLeases(*db_, *catalog_, *config_, today);
    notices_.insert(notices_.end(), outcome.notices.begin(), outcome.notices.end());
    if (!outcome.error.empty()) notices_.push_back("Airline database error: " + outcome.error);
    return outcome.leased + outcome.monthsCharged + outcome.returned > 0;
}

std::vector<AirplaneView> AirlineSession::airplanes() { return db_ ? listAirplanes(*db_) : std::vector<AirplaneView>{}; }

std::vector<LedgerView> AirlineSession::ledger() { return db_ ? listLedger(*db_) : std::vector<LedgerView>{}; }

Cents AirlineSession::cash() { return db_ ? cashCents(*db_) : 0; }

double AirlineSession::hoursFlown() { return db_ ? blocktime::hoursFlown(*db_).value_or(0.0) : 0.0; }

std::string AirlineSession::airlineName() const { return config_ ? config_->identity.name : std::string(); }

std::string AirlineSession::airlineIcao() const { return config_ ? config_->identity.icaoCode : std::string(); }


std::optional<GateContext> AirlineSession::gateContext(const std::string& typeCode, FlightKind kind,
                                                       const std::string& destinationIcao) const {
    if (!catalog_) return std::nullopt;
    const CatalogType* type = catalog_->type(typeCode);
    if (!type) return std::nullopt;
    GateContext c{type->wingspanM, spotUseFor(kindCode(kind)), {}};
    // Preferred gates are for gate flights only (D-147).
    if (config_ && c.use == SpotUse::Gate) {
        const auto it = config_->preferredGates.find(destinationIcao);
        if (it != config_->preferredGates.end()) c.preferred = it->second;
    }
    return c;
}

}  // namespace blocktime
