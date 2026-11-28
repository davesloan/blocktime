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
#include "plan_brief.h"
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
    int additionalFuelLb = 0;                     // extra fuel sent to SimBrief; 0 = none
};

// Writes the flight and its cabins in one transaction. Refuses while a flight is planned already (D-103).
std::optional<std::int64_t> createPlannedFlight(db::Database& db, const NewPlannedFlight& flight, std::string* error);

// Rewrites a planned flight's row and cabins in one transaction, and deletes its imported plan and its acceptance: any
// change makes the plan stale (Change flight is offered only before Accept, D-251).
bool updatePlannedFlight(db::Database& db, std::int64_t id, const NewPlannedFlight& flight, std::string* error);

struct PlannedFlightView {
    std::int64_t id = 0;
    std::string registration, typeCode, airframeId, origin, originName, destination, destinationName, flightNumber;
    // The airports' cities, or their names when OurAirports has no city (D-181).
    std::string originCity, destinationCity;
    FlightKind kind = FlightKind::Passenger;
    std::string family, role;
    int seatsInstalled = 0, cruiseSpeedKt = 0;
    AirplaneLimits limits;
    std::vector<CabinLoad> cabins;
    int cargoLb = 0;
    double distanceNm = 0;
    Cents fuelPriceCentsPer100Lb = 0;
    // The fuel BlockTime had on record for the airplane: today's for a planned flight, the one at block out for a
    // settled flight (its replay).
    int fuelOnRecordLb = 0;
    std::string plannedAt, originRunway, destinationRunway;  // runways "" = none from the ATIS
    // Where the airplane last parked ("Gate E7"), if its latest flight ended at this origin; "" otherwise (D-148).
    std::string loadInParking;
    std::optional<SimbriefPlan> plan;
    // The plan's extras from its stored OFP, set with `plan` (D-188).
    std::optional<PlanBrief> brief;
    // The origin, the plan's navlog fixes, then the destination; only the two airports without a plan (D-182).
    std::vector<RoutePoint> route;
    std::vector<PlanFix> planFixes;  // the navlog with the plan's times and fuel: route[k + 1] is planFixes[k]
    int taxiOutS = 0, taxiInS = 0;   // D-212: Dave's median at the airport, else the plan's; 0 with neither
    std::vector<Hold> holds;  // the airplane's, for the load sheet (D-153)
    int additionalFuelLb = 0;  // extra fuel sent to SimBrief; 0 = none
    std::optional<std::string> acceptedAt;  // set once the flight is accepted (D-222); a planned flight only
};

std::optional<PlannedFlightView> currentPlannedFlight(db::Database& db);
// A settled flight as it was planned: its row, cabins and plan, for the replay (D-175). `loadInParking` is where it
// started. nullopt if no flight has that log id.
std::optional<PlannedFlightView> flownFlightView(db::Database& db, std::string_view logFlightId);
// The log id of the flight that blocked in last; nullopt with none settled.
std::optional<std::string> latestFlightId(db::Database& db);
bool cancelPlannedFlight(db::Database& db, std::string* error);  // deletes it with its cabins and plan

// Writes the imported plan, replacing an earlier one (D-101).
bool storeSimbriefPlan(db::Database& db, std::int64_t plannedFlightId, const SimbriefPlan& plan,
                       std::string_view ofpJson, std::string_view importedAt, std::string* error);

// The money inputs: SimBrief's block time and the fuel to add once a plan is in (D-223), else the estimate and no
// fuel (D-104). New Flight's draft fills the same fields; a test checks they agree (review 4.3).
PriceInput priceInput(const PlannedFlightView& flight);

// What the recorder checks a brake release against (D-088): the type's sim folder, the origin's position and the
// airplane's fuel on record, and whether it was accepted. nullopt if the planned flight is gone or has no plan
// imported: a flight is only recorded with a plan (D-250).
std::optional<FlightTarget> flightTarget(db::Database& db, const PlannedFlightView& flight);

// The plan's estimated landing weight is 95% of MLW or more (D-104). False without a plan.
bool nearMlw(const PlannedFlightView& flight);

}  // namespace blocktime
