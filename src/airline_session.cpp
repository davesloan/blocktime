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
    return {defaultAirlineDatabaseFile(), defaultCatalogFile(), defaultAirlineConfigFile()};
}

AirlineSession AirlineSession::start(const AirlinePaths& paths, Date today, std::ostream& warnings) {
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

    std::string error;
    if (!std::filesystem::exists(paths.catalog)) {
        s.startNotices_.push_back(std::format("No catalog.yaml in {}. Copy config\\catalog.yaml there (README).",
                                              folderOf(paths.catalog)));
    } else if (s.catalog_ = loadCatalog(paths.catalog, &error); !s.catalog_) {
        s.startNotices_.push_back(error);
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
    s.settle(today);
    return s;
}

void AirlineSession::tick(Date today) { settle(today); }

void AirlineSession::settle(Date today) {
    notices_ = startNotices_;
    if (!db_ || !catalog_ || !config_) return;
    const LeaseOutcome outcome = settleLeases(*db_, *catalog_, *config_, today);
    notices_.insert(notices_.end(), outcome.notices.begin(), outcome.notices.end());
    if (!outcome.error.empty()) notices_.push_back("Airline database error: " + outcome.error);
}

std::vector<AirplaneView> AirlineSession::airplanes() { return db_ ? listAirplanes(*db_) : std::vector<AirplaneView>{}; }

std::vector<LedgerView> AirlineSession::ledger() { return db_ ? listLedger(*db_) : std::vector<LedgerView>{}; }

Cents AirlineSession::cash() { return db_ ? cashCents(*db_) : 0; }

std::string AirlineSession::airlineName() const { return config_ ? config_->identity.name : std::string(); }

}  // namespace blocktime
