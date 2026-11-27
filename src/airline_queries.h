// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Read-only views of the airline database for the screens: the airplanes, the ledger and the cash (docs/database.md).

#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "catalog.h"
#include "dates.h"
#include "money.h"
#include "pricing.h"
#include "sqlite_db.h"

namespace blocktime {

struct AirplaneView {
    std::string registration, tailNumber /* "" = none */, typeCode, airframeName, locationIcao;
    int fuelOnBoardLb = 0;
    bool awaitingDelivery = false;
    std::optional<Date> returnedOn;  // set once the lease has ended
    std::string returnReason;        // "" while leased, "unpaid"
    int flights = 0;                 // settled flights (D-097 undone: counted from the database now)
    double blockHours = 0;           // their block time
};

struct LedgerView {
    std::string postedAt;  // UTC instant, YYYY-MM-DDTHH:MM:SSZ
    std::string text;      // "Opening cash", "Lease N814SX, 22 Nov – 21 Dec"
    Cents amountCents = 0;
};

std::vector<AirplaneView> listAirplanes(db::Database& db);  // by registration
std::vector<LedgerView> listLedger(db::Database& db);       // newest first, then the latest written first

Cents cashCents(db::Database& db);                                // the sum of every ledger line
// Lines posted at or before `instant`; nullopt if the read failed (never $0 then: a lease is returned on it, 1.4).
std::optional<Cents> cashCentsAsOf(db::Database& db, std::string_view instant);
int airportCount(db::Database& db);
// The great-circle distance between two airports in the airport data, in nautical miles; nullopt if either is missing.
std::optional<double> airportDistanceNm(db::Database& db, std::string_view fromIcao, std::string_view toIcao);

// The airplane's limits (D-100), or nullopt if it has none yet.
std::optional<AirplaneLimits> airplaneLimits(db::Database& db, std::int64_t airplaneId);

struct NearestAirport {
    std::int64_t id = 0;
    std::string icao, name;
    double distanceM = 0;
};

// The imported airport closest to the position, at any distance (D-113); nullopt only with no airports.
std::optional<NearestAirport> nearestAirport(db::Database& db, double lat, double lon);

std::set<std::string> settledLogFlightIds(db::Database& db);  // every flight's log_flight_id

// The sum of every flight's block time, in hours (D-114); nullopt if the read failed.
std::optional<double> hoursFlown(db::Database& db);

// A settled flight with its money, for the Flights screen and the summary (D-111).
struct FlightView {
    std::string logFlightId, flightNumber, registration, typeCode, kind;  // kind: planned_flight's codes
    std::string origin, plannedDestination, arrival;                     // ICAO codes
    double originLat = 0, originLon = 0, arrivalLat = 0, arrivalLon = 0;
    std::string blockOutAt, blockInAt;  // UTC instants
    long long blockS = 0;
    int fuelOutLb = 0, fuelInLb = 0;
    std::string arrivalParking;  // "Gate E7"; "" when none was named (D-148)
    std::vector<MoneyLine> income, costs;  // in the order they were posted (the preview's order)
    Cents totalCents = 0;
};

std::vector<FlightView> listFlights(db::Database& db);  // newest block in first

}  // namespace blocktime
