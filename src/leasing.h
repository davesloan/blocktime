// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline's lease processes (docs/database.md): types from the aircraft files (D-093), the starting cash from
// airline.yaml, leasing an airplane from its SimBrief airframe (D-242), with factory or instant delivery (D-245), and
// charging lease months in date order (D-082, D-083, D-096).

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "airline_config.h"
#include "catalog.h"
#include "dates.h"
#include "fleet_rules.h"
#include "rates.h"
#include "sqlite_db.h"

namespace blocktime {

// Inserts or updates every catalog type by code. Types are reference data (D-093).
bool syncCatalogTypes(db::Database& db, const Catalog& catalog, std::string* error);
// Posts the starting cash, rounded to $5, at the start of founded_on, unless it's already posted.
bool postOpeningCash(db::Database& db, const AirlineConfig& config, std::string* error);

// Fleet slots (design.md §6, D-083, D-114, D-240): two to start, then one more for every 10 hours flown by Dave (slot
// 3 at 10 h, slot 4 at 20 h, ...), at most 20.
inline constexpr int kStartingSlots = 2;
inline constexpr int kMaxSlots = 20;
int slotsEarned(double hoursFlown);
double hoursForSlot(int n);

// One airplane to lease: what its SimBrief airframe gave, the type and layout picked for it, its free-flight number
// ("" for none) and where it starts: its type's factory (nullopt, a factory delivery) or an airport it's delivered to
// at once (D-245). Leased on `today`.
struct LeaseRequest {
    SimbriefReading reading;
    const CatalogType* type = nullptr;
    const CatalogAirframe* layout = nullptr;
    std::string flightNumber;
    std::optional<std::string> instantDeliveryIcao;
    Date today;
};

struct LeaseResult {
    std::optional<std::int64_t> airplaneId;
    std::string refusal;  // why it wasn't leased, as the form shows it; "" when it was
};

// Leases the airplane in one transaction (docs/fleet.md): the airplane with its cabins, its holds split from SimBrief's
// Max Cargo, its limits and SimBrief values, the first lease month and instant delivery's lines. Every check is made
// again inside the transaction; if one fails nothing is written. `slots` overrides the slots earned (tests).
LeaseResult leaseAirplane(db::Database& db, const LeaseRequest& request, const Rates& rates,
                          std::optional<int> slots = std::nullopt);
// The same checks without writing: "" when the airplane would be leased.
std::string leaseRefusal(db::Database& db, const LeaseRequest& request, const Rates& rates,
                         std::optional<int> slots = std::nullopt);

// The airplane's current-state values from a SimBrief reading, replacing what it had. Call inside a transaction.
bool writeHolds(db::Database& db, std::int64_t airplaneId, const std::vector<Hold>& holds, std::string& error);
bool writeLimits(db::Database& db, std::int64_t airplaneId, const AirplaneLimits& limits, std::string& error);
bool writeSimbriefValues(db::Database& db, std::int64_t airplaneId, const SimbriefReading& reading,
                         std::string_view readAt, std::string& error);

struct LeaseOutcome {
    int monthsCharged = 0, returned = 0;
    std::vector<std::string> notices;  // for the screens, in event order
    std::string error;                 // a database failure; settling stopped there
};

// Charges every lease month that has started and isn't charged, all airplanes in date order (ties by registration),
// one transaction per month. A month that costs more than the cash on its date isn't charged: the airplane is returned
// (reason unpaid).
LeaseOutcome settleLeases(db::Database& db, Date today);

}  // namespace blocktime
