// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_settlement.h"

#include "debrief_store.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <vector>

#include <nlohmann/json.hpp>
#include <sqlite3.h>

#include "airline_queries.h"
#include "event_log.h"
#include "gsx_timeline.h"
#include "telemetry_json.h"

namespace blocktime {

namespace {

bool isFlightLine(const std::string& line) {
    return line.find(R"("type":"block_out")") != std::string::npos ||
           line.find(R"("type":"block_in")") != std::string::npos ||
           line.find(R"("type":"flight_interrupted")") != std::string::npos;
}

// What the timeline and the phases need besides the block events (D-137, D-138).
bool isTimingLine(const std::string& line) {
    return line.find(R"("type":"phase_changed")") != std::string::npos ||
           line.find(R"("type":"gsx_service")") != std::string::npos ||
           line.find(R"("type":"sim_stopped")") != std::string::npos ||
           line.find(R"("type":"sim_disconnected")") != std::string::npos;
}

// "2026-11-23T10:30:05.120Z" -> "2026-11-23T10:30:05Z" (D-081: the database keeps whole seconds).
std::string wholeSeconds(const std::string& logTime) {
    return logTime.size() >= 19 ? logTime.substr(0, 19) + "Z" : logTime;
}

double number(const nlohmann::json& data, const char* key) {
    const auto it = data.find(key);
    return it != data.end() && it->is_number() ? it->get<double>() : 0.0;
}

int pounds(double lb) { return std::max(0, static_cast<int>(std::lround(lb))); }

}  // namespace

std::optional<FlownFlight> findFlownFlight(const std::filesystem::path& logDir, std::int64_t plannedFlightId,
                                           std::string_view plannedAt, const std::set<std::string>& alreadySettled,
                                           std::size_t maxFiles) {
    std::vector<std::filesystem::path> files = dailyLogFiles(logDir);  // oldest first
    // A flight planned a week ago and flown today is still found (review 1.2): the files from its planned day on.
    if (plannedAt.size() >= 10) {
        const std::string first = "events-" + std::string(plannedAt.substr(0, 10));
        std::erase_if(files, [&first](const std::filesystem::path& f) { return f.filename().string() < first; });
    }
    if (files.size() > maxFiles) files.erase(files.begin(), files.end() - static_cast<std::ptrdiff_t>(maxFiles));

    std::map<std::string, FlownFlight> attempts;  // this planned flight's open block_outs, by flight_id
    GsxTimeline gsx;
    for (const auto& file : files) {
        std::ifstream in(file, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            const bool timing = isTimingLine(line);
            if (!timing && !isFlightLine(line)) continue;  // most lines are telemetry
            auto j = nlohmann::json::parse(line, nullptr, false);
            if (j.is_discarded() || !j.is_object()) continue;
            if (!j.contains("data") || !j["data"].is_object()) j["data"] = nlohmann::json::object();
            auto& data = j["data"];
            const std::string type = j.value("type", std::string());
            if (type == "sim_stopped" || type == "sim_disconnected") {
                data["time"] = j.value("ts", std::string());  // these lines carry no data time
                gsx.onEvent(type, data);
                continue;
            }
            if (type == "gsx_service") {
                gsx.onEvent(type, data);
                continue;
            }
            if (!data.contains("flight_id") || !data["flight_id"].is_string()) continue;
            const std::string id = data["flight_id"].get<std::string>();
            if (type == "phase_changed") {
                if (const auto it = attempts.find(id); it != attempts.end()) {
                    it->second.phases.push_back({data.value("phase", std::string()),
                                                 wholeSeconds(data.value("time", std::string()))});
                }
                continue;
            }
            gsx.onEvent(type, data);  // block_out, block_in, flight_interrupted
            if (type == "flight_interrupted") {
                attempts.erase(id);
                continue;
            }
            // The block events are read by the parser that pairs with the writer's toJson (review 1.10), so a field
            // renamed in one is caught by the tests of the other.
            const auto event = blockEventFromJson(type, data);
            if (!event) continue;
            if (const auto* out = std::get_if<BlockOut>(&*event)) {
                if (out->plannedFlightId != plannedFlightId) continue;
                FlownFlight f;
                f.logFlightId = id;
                f.blockOutAt = wholeSeconds(formatUtc(out->time));
                f.fuelOutLb = pounds(out->fuelLb);
                f.originParking = out->parking;
                for (const ServiceSpan& s : gsx.departed()) {
                    f.services.push_back({s.service, wholeSeconds(s.startIso), wholeSeconds(s.endIso)});
                }
                if (f.blockOutAt > plannedAt && !alreadySettled.contains(id)) attempts[id] = f;
            } else if (const auto* blockIn = std::get_if<BlockIn>(&*event)) {
                const auto it = attempts.find(id);
                if (it == attempts.end()) continue;
                FlownFlight f = it->second;
                f.blockInAt = wholeSeconds(formatUtc(blockIn->time));
                f.fuelInLb = pounds(blockIn->shutdownFuelLb);
                f.inLat = blockIn->latDeg;
                f.inLon = blockIn->lonDeg;
                f.arrivalParking = blockIn->parking;
                f.blockS = blockIn->blockTimeS;
                // The recorder logs block_in, then phase_changed arrived (FlightTracker::onBlockIn), and settling runs
                // on block_in, often before that line is written. Arrived is the block in, so it's added here.
                if (f.phases.empty() || f.phases.back().phase != "arrived") f.phases.push_back({"arrived", f.blockInAt});
                return f;
            }
        }
    }
    return std::nullopt;
}

std::optional<SettledFlight> settleFlight(db::Database& db, const PlannedFlightView& planned, const FlownFlight& flown,
                                          const Rates& rates, std::string* error) {
    const auto fail = [&](const std::string& message) -> std::optional<SettledFlight> {
        if (error) *error = "Couldn't settle " + planned.origin + "–" + planned.destination + ": " + message;
        return std::nullopt;
    };
    const auto arrival = nearestAirport(db, flown.inLat, flown.inLon);
    if (!arrival) return fail("no airports imported");

    db::Transaction tx(db);
    if (!tx.active()) return fail("couldn't start a transaction: " + tx.error());

    db::Statement airplane(db, "SELECT a.airplane_id, a.fuel_on_board_lb FROM planned_flight f"
                               " JOIN airplane a USING (airplane_id) WHERE f.planned_flight_id = ?");
    airplane.bind(1, planned.id);
    if (!airplane.next()) return fail("the planned flight is gone");
    const std::int64_t airplaneId = airplane.integer(0);
    const int fuelOnRecord = static_cast<int>(airplane.integer(1));

    PriceInput in = priceInput(planned);
    in.actualBlockHours = static_cast<double>(flown.blockS) / 3600.0;
    in.fuelAddedLb = std::max(0, flown.fuelOutLb - fuelOnRecord);  // fuel added, not burned (design.md §11)
    SettledFlight settled;
    settled.logFlightId = flown.logFlightId;
    settled.arrivalIcao = arrival->icao;
    settled.diverted = arrival->icao != planned.destination;
    settled.money = pricePreview(in, rates);

    db::Statement flight(db,
                         "INSERT INTO flight (log_flight_id, airplane_id, flight_number, kind, origin_airport_id,"
                         " planned_destination_airport_id, arrival_airport_id, cargo_lb, fuel_price_cents_per_100lb,"
                         " planned_at, block_out_at, block_in_at, fuel_on_record_lb, fuel_out_lb, fuel_in_lb,"
                         " origin_runway, destination_runway, origin_parking, arrival_parking)"
                         " SELECT ?, airplane_id, flight_number, kind, origin_airport_id, destination_airport_id, ?,"
                         " cargo_lb, fuel_price_cents_per_100lb, planned_at, ?, ?, ?, ?, ?, origin_runway,"
                         " destination_runway, ?, ? FROM planned_flight WHERE planned_flight_id = ?");
    flight.bind(1, flown.logFlightId);
    flight.bind(2, arrival->id);
    flight.bind(3, flown.blockOutAt);
    flight.bind(4, flown.blockInAt);
    flight.bind(5, static_cast<std::int64_t>(fuelOnRecord));
    flight.bind(6, static_cast<std::int64_t>(flown.fuelOutLb));
    flight.bind(7, static_cast<std::int64_t>(flown.fuelInLb));
    const auto bindSpot = [&flight](int index, const std::string& spot) {
        if (spot.empty()) {
            flight.bindNull(index);
        } else {
            flight.bind(index, std::string_view(spot));
        }
    };
    bindSpot(8, flown.originParking);
    bindSpot(9, flown.arrivalParking);
    flight.bind(10, planned.id);
    if (!flight.run()) return fail(flight.error());
    const std::int64_t flightId = db.lastInsertRowId();

    std::string message;
    if (!db.exec("INSERT INTO flight_cabin (flight_id, cabin_class, passengers) SELECT " + std::to_string(flightId) +
                     ", cabin_class, passengers FROM planned_flight_cabin WHERE planned_flight_id = " +
                     std::to_string(planned.id),
                 &message) ||
        !db.exec("INSERT INTO flight_plan (flight_id, request_id, generated_at, imported_at, route, origin_runway,"
                 " destination_runway, cruise_altitude_ft, est_block_s, ramp_fuel_lb, enroute_burn_lb,"
                 " est_landing_weight_lb, origin_transition_altitude_ft, destination_transition_level_ft, ofp_json)"
                 " SELECT " + std::to_string(flightId) +
                     ", request_id, generated_at, imported_at, route, origin_runway, destination_runway,"
                     " cruise_altitude_ft, est_block_s, ramp_fuel_lb, enroute_burn_lb, est_landing_weight_lb,"
                     " origin_transition_altitude_ft, destination_transition_level_ft, ofp_json FROM simbrief_plan"
                     " WHERE planned_flight_id = " + std::to_string(planned.id),
                 &message)) {
        return fail(message);
    }

    // Its phase and GSX service times, for the leg templates (D-137).
    db::Statement phase(db, "INSERT INTO flight_phase (flight_id, seq, phase, started_at) VALUES (?, ?, ?, ?)");
    for (std::size_t i = 0; i < flown.phases.size(); ++i) {
        phase.reset();
        phase.bind(1, flightId);
        phase.bind(2, static_cast<std::int64_t>(i));
        phase.bind(3, flown.phases[i].phase);
        phase.bind(4, flown.phases[i].at);
        if (!phase.run()) return fail("phase " + flown.phases[i].phase + ": " + phase.error());
    }
    db::Statement service(db, "INSERT INTO flight_service (flight_id, service, started_at, ended_at)"
                              " VALUES (?, ?, ?, ?)");
    for (const ServiceTime& s : flown.services) {
        service.reset();
        service.bind(1, flightId);
        service.bind(2, s.service);
        service.bind(3, s.startedAt);
        service.bind(4, s.endedAt);
        if (!service.run()) return fail(s.service + ": " + service.error());
    }

    // The debrief's measurements (D-160): written once, with the flight. The debrief is extra and the money isn't, so
    // a measurement that can't be taken or written leaves the flight settled without one (behind a savepoint).
    if (!flown.lines.empty()) {
        std::optional<FlightMeasurements> measured;
        try {
            measured = measureFlight(flown.lines, settled.diverted);
        } catch (const std::exception& e) {
            settled.debriefError = std::string("measuring the debrief: ") + e.what();
        }
        if (measured && db.exec("SAVEPOINT debrief", &message)) {
            if (!writeMeasurements(db, flightId, *measured, &message)) {
                settled.debriefError = "writing the debrief: " + message;
                if (db.inTransaction() && !db.exec("ROLLBACK TO debrief", &message)) return fail(message);
            }
            if (db.inTransaction() && !db.exec("RELEASE debrief", &message)) return fail(message);
        }
        // An error that rolled back the whole transaction leaves autocommit on: the money lines below would then each
        // commit on their own, and the flight would be settled again at the next start (review 1.3).
        if (!db.inTransaction()) return fail("the transaction was lost writing the debrief (" + settled.debriefError + ")");
    }

    db::Statement ledger(db, "INSERT INTO ledger_line (category, posted_at, amount_cents) VALUES (?, ?, ?)");
    db::Statement link(db, "INSERT INTO flight_money_line (ledger_line_id, flight_id) VALUES (?, ?)");
    for (const auto* lines : {&settled.money.income, &settled.money.costs}) {
        for (const MoneyLine& m : *lines) {
            if (m.cents == 0) continue;  // a $0 fuel line is shown, never posted
            ledger.reset();
            ledger.bind(1, m.category);
            ledger.bind(2, flown.blockInAt);
            ledger.bind(3, m.cents);
            if (!ledger.run()) return fail(m.category + ": " + ledger.error());
            link.reset();
            link.bind(1, db.lastInsertRowId());
            link.bind(2, flightId);
            if (!link.run()) return fail(link.error());
        }
    }

    db::Statement move(db, "UPDATE airplane SET location_airport_id = ?, fuel_on_board_lb = ?, awaiting_delivery = 0"
                           " WHERE airplane_id = ?");
    move.bind(1, arrival->id);
    move.bind(2, static_cast<std::int64_t>(flown.fuelInLb));
    move.bind(3, airplaneId);
    if (!move.run()) return fail(move.error());
    if (!db.exec("DELETE FROM planned_flight WHERE planned_flight_id = " + std::to_string(planned.id), &message)) {
        return fail(message);
    }
    if (!tx.commit(&message)) return fail(message);
    if (error) error->clear();
    return settled;
}

int recordDeboardings(db::Database& db, const std::filesystem::path& logDir, std::size_t maxFiles) {
    std::vector<std::filesystem::path> files = dailyLogFiles(logDir);  // oldest first
    if (files.size() > maxFiles) files.erase(files.begin(), files.end() - static_cast<std::ptrdiff_t>(maxFiles));

    db::Statement insert(db, "INSERT OR IGNORE INTO flight_service (flight_id, service, started_at, ended_at)"
                             " SELECT flight_id, 'deboarding', ?, ? FROM flight WHERE log_flight_id = ?");
    int added = 0;
    GsxTimeline gsx;
    for (const auto& file : files) {
        std::ifstream in(file, std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            if (!isTimingLine(line) && !isFlightLine(line)) continue;
            auto j = nlohmann::json::parse(line, nullptr, false);
            if (j.is_discarded() || !j.is_object()) continue;
            if (!j.contains("data") || !j["data"].is_object()) j["data"] = nlohmann::json::object();
            auto& data = j["data"];
            const std::string type = j.value("type", std::string());
            if (type == "phase_changed") continue;
            if (type == "sim_stopped" || type == "sim_disconnected") data["time"] = j.value("ts", std::string());
            const std::size_t before = gsx.arrival().size();
            const bool wasRunning = before > 0 && gsx.arrival().back().running;
            gsx.onEvent(type, data);
            // A deboarding that just completed: the last arrival span, running before this line and done after it.
            if (type != "gsx_service" || gsx.arrival().size() != before || !wasRunning || gsx.arrival().back().running) {
                continue;
            }
            const ServiceSpan& s = gsx.arrival().back();
            insert.reset();
            insert.bind(1, wholeSeconds(s.startIso));
            insert.bind(2, wholeSeconds(s.endIso));
            insert.bind(3, gsx.lastBlockInFlightId());
            if (insert.run()) added += sqlite3_changes(db.handle());
        }
    }
    return added;
}

std::optional<CurrentFlown> findCurrentFlown(db::Database& db, const std::filesystem::path& logDir, bool withLines) {
    auto planned = currentPlannedFlight(db);
    if (!planned) return std::nullopt;
    auto flown = findFlownFlight(logDir, planned->id, planned->plannedAt, settledLogFlightIds(db));
    if (!flown) return std::nullopt;
    if (withLines) flown->lines = loadFlightEvents(logDir, flown->logFlightId);
    return CurrentFlown{std::move(*planned), std::move(*flown)};
}

std::optional<SettledFlight> settleIfFlown(db::Database& db, const std::filesystem::path& logDir, const Rates& rates,
                                           std::string* error) {
    if (error) error->clear();
    const auto current = findCurrentFlown(db, logDir, true);
    if (!current) return std::nullopt;
    return settleFlight(db, current->planned, current->flown, rates, error);
}

}  // namespace blocktime
