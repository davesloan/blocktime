// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The debrief in the airline database (migration 7, docs/database.md "Milestone 10's tables"): each flight's
// measurements, written once when it settles, and Claude's review, written once when it arrives (D-160, D-161).

#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "debrief_measure.h"
#include "sqlite_db.h"

namespace blocktime {

// Writes flight_debrief, its value and skip rows and flight_landing. Runs inside the caller's transaction (settling).
bool writeMeasurements(db::Database& db, std::int64_t flightId, const FlightMeasurements& m, std::string* error);

// Every measured flight's measurements, by log_flight_id. Flights settled before 0.11.0 aren't in it.
std::map<std::string, FlightMeasurements> loadMeasurements(db::Database& db);

struct StoredReview {
    std::string summary, landing, atc;
    std::string model;      // the model that wrote it
    std::string writtenAt;  // UTC instant
    std::vector<std::string> workOn;
};

bool isMeasured(db::Database& db, std::string_view logFlightId);  // has a flight_debrief row
bool hasReview(db::Database& db, std::string_view logFlightId);

// Every written review, by log_flight_id.
std::map<std::string, StoredReview> loadReviews(db::Database& db);

// The flight's review, in one transaction. False (nothing written) if the flight isn't measured or already has one.
bool storeReview(db::Database& db, std::string_view logFlightId, const StoredReview& review, std::string* error);

}  // namespace blocktime
