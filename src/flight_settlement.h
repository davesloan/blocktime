// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Settling a flown flight (design.md §13.7, D-109, D-113): the planned flight's block_out and block_in are read back
// from the event log, which is written first (D-008), and turned into a `flight` with its money, in one transaction.
// Runs on every block event and once at startup, so a crash between block in and settling loses nothing; a flight is
// never settled twice.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "flight_events.h"
#include "open_flight.h"
#include "planned_flight.h"
#include "pricing.h"
#include "rates.h"
#include "sqlite_db.h"

namespace blocktime {

struct PhaseStart {
    std::string phase, at;  // at: whole seconds
};
struct ServiceTime {
    std::string service, startedAt, endedAt;  // whole seconds
};

struct FlownFlight {
    std::string logFlightId;            // the log's flight_id
    std::string blockOutAt, blockInAt;  // instants, whole seconds (the log's milliseconds dropped)
    int fuelOutLb = 0;                  // block_out fuel_lb
    int fuelInLb = 0;                   // block_in shutdown_fuel_lb: what's left in the tanks (D-110)
    double inLat = 0, inLon = 0;        // where it parked
    std::string originParking, arrivalParking;  // the block events' spots ("Gate D14"); "" = none (D-148)
    long long blockS = 0;               // block_in block_time_s
    std::vector<PhaseStart> phases;     // its phase_changed events, in log order (D-137)
    std::vector<ServiceTime> services;  // the GSX services completed before block out (D-137, D-138)
    // The flight's log lines (flight_events.h), measured for the debrief when it settles (D-160). Empty: no debrief.
    std::vector<LogLine> lines;
};

// Reads the daily logs in `logDir` from plannedAt's day on (a flight is never in an older file), at most the last
// maxFiles of them, oldest first, as one stream. The first block_out with this
// planned_flight_id, released after `plannedAt` and not in `alreadySettled`, that is followed by a block_in with its
// flight_id is the flown flight. An interrupted attempt is skipped (D-108). Lines that don't parse are skipped.
// Its phases are the phase_changed events with its flight_id; its services are those a GsxTimeline, fed every block,
// gsx_service and sim session line, gives the block out (D-138).
std::optional<FlownFlight> findFlownFlight(const std::filesystem::path& logDir, std::int64_t plannedFlightId,
                                           std::string_view plannedAt, const std::set<std::string>& alreadySettled,
                                           std::size_t maxFiles = SIZE_MAX);

struct SettledFlight {
    std::string logFlightId, arrivalIcao;
    bool diverted = false;  // parked somewhere other than the planned destination
    Preview money;          // the posted lines and their total
    // Why the flight settled without its debrief measurements ("" if it has them, or had no lines): for errors.log.
    std::string debriefError;
};

// One transaction: the flight with its cabins, plan, phases and services; its debrief measurements (D-160, measured
// from flown.lines with "diverted" from the arrival); its money lines, each a ledger_line posted at block in plus a
// flight_money_line; the airplane moved to the arrival airport with fuelInLb on board and no longer awaiting
// delivery; and the planned flight deleted. Money (D-109): the planned flight's prices with the real block time for
// crew and maintenance, and fuel charged for what was added. On failure nothing is written and `error` says why.
std::optional<SettledFlight> settleFlight(db::Database& db, const PlannedFlightView& planned, const FlownFlight& flown,
                                          const Rates& rates, std::string* error);

// Adds each completed GSX deboarding in the log to the settled flight whose block_in came before it (D-137): deboarding
// happens after block in, when the flight is already settled. Rows that exist are skipped (INSERT OR IGNORE on the
// UNIQUE), so it can run at startup and whenever a deboarding ends. Returns the rows added.
int recordDeboardings(db::Database& db, const std::filesystem::path& logDir,
                      std::size_t maxFiles = kOpenFlightLookbackFiles);

// The current planned flight and its flown flight, if the log has one not yet settled. `withLines` also loads the
// flight's log lines for the debrief (the slow part).
struct CurrentFlown {
    PlannedFlightView planned;
    FlownFlight flown;
};
std::optional<CurrentFlown> findCurrentFlown(db::Database& db, const std::filesystem::path& logDir, bool withLines);

// findCurrentFlown, then settleFlight. nullopt with `error` "" when nothing was flown.
std::optional<SettledFlight> settleIfFlown(db::Database& db, const std::filesystem::path& logDir, const Rates& rates,
                                           std::string* error);

}  // namespace blocktime
