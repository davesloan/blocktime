// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The planned flight (design.md §13.1-13.3, D-103): the airplane as New Flight sees it, the destination check, and
// the planned_flight rows with their loads and imported SimBrief plan (docs/database.md). One at a time; current
// state, not history.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalog.h"
#include "flight_match.h"
#include "loads.h"
#include "money.h"
#include "pricing.h"
#include "simbrief.h"
#include "sqlite_db.h"

namespace blocktime {

// A parked airplane, with what New Flight needs to set up its next flight.
struct FlightAirplane {
    std::int64_t airplaneId = 0;
    std::string registration, typeCode, family, role, simbriefAirframeId, originIcao, originName, originTimeZone;
    double originLat = 0, originLon = 0;
    int cruiseSpeedKt = 0;
    bool awaitingDelivery = false;
    std::string freeFlightNumber;  // "" = none (D-106)
    std::vector<Cabin> cabins;
    std::vector<Hold> holds;
    AirplaneLimits limits;
};

// nullopt if the airplane is unknown, returned, or has no limits.
std::optional<FlightAirplane> loadFlightAirplane(db::Database& db, std::string_view registration);

// What the airplane may fly (design.md §13.1): only its delivery flight while awaiting delivery; VIP or charter for
// an executive jet; passengers for an airliner; cargo for a freighter.
std::vector<FlightKind> allowedKinds(const FlightAirplane& airplane);

struct DestinationCheck {
    bool ok = false;
    std::string message;  // "" when ok
    std::string icao, name;
    double lat = 0, lon = 0, distanceNm = 0;  // great-circle from the origin
};

// Trims and uppercases `typed`, then checks it's an imported airport other than the origin with a long enough
// runway (design.md §4).
DestinationCheck checkDestination(db::Database& db, const FlightAirplane& airplane, std::string_view typed);

bool validFlightNumber(std::string_view number);  // 1-4 digits

struct NewPlannedFlight {
    std::int64_t airplaneId = 0;
    std::string originIcao, destinationIcao, flightNumber;
    FlightKind kind = FlightKind::Passenger;
    std::vector<CabinLoad> cabins;
    int cargoLb = 0;
    Cents fuelPriceCentsPer100Lb = 0;
    std::string plannedAt;                        // UTC instant
    std::string originRunway, destinationRunway;  // "" = none from the ATIS
};

// Writes the flight and its cabins in one transaction. Refuses while a flight is planned already (D-103).
std::optional<std::int64_t> createPlannedFlight(db::Database& db, const NewPlannedFlight& flight, std::string* error);

struct PlannedFlightView {
    std::int64_t id = 0;
    std::string registration, typeCode, airframeId, origin, originName, destination, destinationName, flightNumber;
    FlightKind kind = FlightKind::Passenger;
    std::string family, role;
    int seatsInstalled = 0, cruiseSpeedKt = 0;
    AirplaneLimits limits;
    std::vector<CabinLoad> cabins;
    int cargoLb = 0;
    double distanceNm = 0;
    Cents fuelPriceCentsPer100Lb = 0;
    std::string plannedAt, originRunway, destinationRunway;  // runways "" = none from the ATIS
    std::optional<SimbriefPlan> plan;
};

std::optional<PlannedFlightView> currentPlannedFlight(db::Database& db);
bool cancelPlannedFlight(db::Database& db, std::string* error);  // deletes it with its cabins and plan

// Writes the imported plan, replacing an earlier one (D-101).
bool storeSimbriefPlan(db::Database& db, std::int64_t plannedFlightId, const SimbriefPlan& plan,
                       std::string_view ofpJson, std::string_view importedAt, std::string* error);

// The money inputs: SimBrief's block time and en-route burn once a plan is in, else the estimate and no fuel (D-104).
PriceInput priceInput(const PlannedFlightView& flight);

// What the recorder checks a brake release against (D-088): the type's sim folder, the origin's position and the
// airplane's fuel on record. nullopt if the planned flight is gone.
std::optional<FlightTarget> flightTarget(db::Database& db, const PlannedFlightView& flight);

// The plan's estimated landing weight is 95% of MLW or more (D-104). False without a plan.
bool nearMlw(const PlannedFlightView& flight);

}  // namespace blocktime
