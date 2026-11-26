// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "planned_flight.h"

#include <algorithm>
#include <cctype>
#include <format>

#include <sqlite3.h>

#include "airline_queries.h"
#include "geo.h"

namespace blocktime {

namespace {

constexpr double kMetresPerNm = 1852.0;

std::vector<Cabin> cabinsOf(db::Database& db, std::int64_t airplaneId) {
    std::vector<Cabin> out;
    db::Statement q(db, "SELECT cabin_class, seats FROM airplane_cabin WHERE airplane_id = ?"
                        " ORDER BY CASE cabin_class WHEN 'first' THEN 0 WHEN 'business' THEN 1 ELSE 2 END");
    q.bind(1, airplaneId);
    while (q.next()) out.push_back({q.text(0), static_cast<int>(q.integer(1))});
    return out;
}

std::vector<Hold> holdsOf(db::Database& db, std::int64_t airplaneId) {
    std::vector<Hold> out;
    db::Statement q(db, "SELECT hold, max_lb, ratio FROM airplane_hold WHERE airplane_id = ?"
                        " ORDER BY CASE hold WHEN 'main_deck' THEN 0 WHEN 'forward' THEN 1 WHEN 'aft' THEN 2 ELSE 3 END");
    q.bind(1, airplaneId);
    while (q.next()) out.push_back({q.text(0), static_cast<int>(q.integer(1)), static_cast<int>(q.integer(2))});
    return out;
}

std::optional<std::int64_t> airportId(db::Database& db, const std::string& icao) {
    db::Statement q(db, "SELECT airport_id FROM airport WHERE icao_code = ?");
    q.bind(1, icao);
    if (!q.next()) return std::nullopt;
    return q.integer(0);
}

void bindText(db::Statement& q, int index, const std::string& text) {
    if (text.empty()) {
        q.bindNull(index);
    } else {
        q.bind(index, text);
    }
}

}  // namespace

std::optional<FlightAirplane> loadFlightAirplane(db::Database& db, std::string_view registration) {
    db::Statement q(db,
                    "SELECT a.airplane_id, a.registration, t.code, t.family, t.role, a.simbrief_airframe_id,"
                    " p.icao_code, p.name, p.time_zone, p.latitude, p.longitude, t.cruise_speed_kt,"
                    " a.awaiting_delivery, COALESCE(a.free_flight_number, '')"
                    " FROM airplane a JOIN aircraft_type t USING (aircraft_type_id)"
                    " JOIN airport p ON p.airport_id = a.location_airport_id"
                    " WHERE a.registration = ? AND a.returned_on IS NULL");
    q.bind(1, registration);
    if (!q.next()) return std::nullopt;
    FlightAirplane a;
    a.airplaneId = q.integer(0);
    a.registration = q.text(1);
    a.typeCode = q.text(2);
    a.family = q.text(3);
    a.role = q.text(4);
    a.simbriefAirframeId = q.text(5);
    a.originIcao = q.text(6);
    a.originName = q.text(7);
    a.originTimeZone = q.text(8);
    a.originLat = q.real(9);
    a.originLon = q.real(10);
    a.cruiseSpeedKt = static_cast<int>(q.integer(11));
    a.awaitingDelivery = q.integer(12) != 0;
    a.freeFlightNumber = q.text(13);
    a.cabins = cabinsOf(db, a.airplaneId);
    a.holds = holdsOf(db, a.airplaneId);
    const auto limits = airplaneLimits(db, a.airplaneId);
    if (!limits) return std::nullopt;
    a.limits = *limits;
    return a;
}

std::vector<FlightKind> allowedKinds(const FlightAirplane& a) {
    if (a.awaitingDelivery) return {FlightKind::Delivery};
    if (a.role == "executive") return {FlightKind::Vip, FlightKind::Charter};
    if (a.role == "freighter") return {FlightKind::Cargo};
    return {FlightKind::Passenger};
}

DestinationCheck checkDestination(db::Database& db, const FlightAirplane& a, std::string_view typed) {
    DestinationCheck c;
    while (!typed.empty() && std::isspace(static_cast<unsigned char>(typed.front()))) typed.remove_prefix(1);
    while (!typed.empty() && std::isspace(static_cast<unsigned char>(typed.back()))) typed.remove_suffix(1);
    std::string icao(typed);
    std::transform(icao.begin(), icao.end(), icao.begin(), [](char ch) {
        return static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    });
    const bool shaped = icao.size() == 4 && std::all_of(icao.begin(), icao.end(), [](char ch) {
                            return std::isalnum(static_cast<unsigned char>(ch)) != 0;
                        });
    if (!shaped) {
        c.message = "Type a 4-letter ICAO code.";
        return c;
    }
    c.icao = icao;
    db::Statement q(db, "SELECT name, latitude, longitude, longest_runway_ft FROM airport WHERE icao_code = ?");
    q.bind(1, icao);
    if (!q.next()) {
        c.message = icao + " isn't in the airport data.";
        return c;
    }
    if (icao == a.originIcao) {
        c.message = std::format("{} is already at {}.", a.registration, icao);
        return c;
    }
    const int runway = static_cast<int>(q.integer(3));
    if (runway < a.limits.minRunwayFt) {
        c.message = std::format("{}'s longest runway is {} ft; {} needs {} ft.", icao, formatNumber(runway),
                                a.registration, formatNumber(a.limits.minRunwayFt));
        return c;
    }
    c.ok = true;
    c.name = q.text(0);
    c.lat = q.real(1);
    c.lon = q.real(2);
    c.distanceNm = distanceMeters(a.originLat, a.originLon, c.lat, c.lon) / kMetresPerNm;
    return c;
}

bool validFlightNumber(std::string_view number) {
    return !number.empty() && number.size() <= 4 &&
           std::all_of(number.begin(), number.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
}

std::optional<std::int64_t> createPlannedFlight(db::Database& db, const NewPlannedFlight& f, std::string* error) {
    const auto fail = [&](std::string message) -> std::optional<std::int64_t> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    if (const auto existing = currentPlannedFlight(db)) {
        return fail(std::format("Flight {} {}–{} is already planned.", existing->flightNumber, existing->origin,
                                existing->destination));
    }
    const auto origin = airportId(db, f.originIcao), destination = airportId(db, f.destinationIcao);
    if (!origin || !destination) return fail("Unknown airport.");
    db::Transaction tx(db);
    if (!tx.active()) return fail("Couldn't begin a transaction.");
    db::Statement insert(db,
                         "INSERT INTO planned_flight (airplane_id, origin_airport_id, destination_airport_id,"
                         " flight_number, kind, cargo_lb, fuel_price_cents_per_100lb, planned_at, origin_runway,"
                         " destination_runway) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.bind(1, f.airplaneId);
    insert.bind(2, *origin);
    insert.bind(3, *destination);
    insert.bind(4, f.flightNumber);
    insert.bind(5, kindCode(f.kind));
    insert.bind(6, static_cast<std::int64_t>(f.cargoLb));
    insert.bind(7, f.fuelPriceCentsPer100Lb);
    insert.bind(8, f.plannedAt);
    bindText(insert, 9, f.originRunway);
    bindText(insert, 10, f.destinationRunway);
    if (!insert.run()) return fail(insert.error());
    const std::int64_t id = sqlite3_last_insert_rowid(db.handle());
    for (const CabinLoad& c : f.cabins) {
        db::Statement cabin(db, "INSERT INTO planned_flight_cabin (planned_flight_id, cabin_class, passengers)"
                                " VALUES (?, ?, ?)");
        cabin.bind(1, id);
        cabin.bind(2, c.cabinClass);
        cabin.bind(3, static_cast<std::int64_t>(c.passengers));
        if (!cabin.run()) return fail(cabin.error());
    }
    std::string message;
    if (!tx.commit(&message)) return fail(message);
    return id;
}

namespace {

// Where a flight view is read from: the planned flight, or a settled flight (D-175). The tables have the same columns
// for what the view needs; a settled flight's destination is the planned one.
struct ViewTables {
    const char* flight;       // "planned_flight" or "flight"
    const char* key;          // its key column
    const char* destination;  // its destination column
    const char* cabins;       // "planned_flight_cabin" or "flight_cabin"
    const char* plan;         // "simbrief_plan" or "flight_plan"
    const char* parking;      // the start's spot: "NULL" for a planned flight, origin_parking for a flown one
};

constexpr ViewTables kPlannedTables{"planned_flight", "planned_flight_id", "destination_airport_id",
                                    "planned_flight_cabin", "simbrief_plan", "NULL"};
constexpr ViewTables kFlownTables{"flight", "flight_id", "planned_destination_airport_id", "flight_cabin",
                                  "flight_plan", "f.origin_parking"};

// The first row of `t.flight` matching `where` (bound to `whereValue` if not empty), as a view.
std::optional<PlannedFlightView> readFlightView(db::Database& db, const ViewTables& t, const std::string& where,
                                                std::string_view whereValue) {
    db::Statement q(db,
                    std::string("SELECT f.") + t.key + ", a.registration, t.code, a.simbrief_airframe_id, o.icao_code,"
                    " o.name, d.icao_code, d.name, f.flight_number, f.kind, t.family, t.role, t.cruise_speed_kt,"
                    " f.cargo_lb, f.fuel_price_cents_per_100lb, f.planned_at, COALESCE(f.origin_runway, ''),"
                    " COALESCE(f.destination_runway, ''), o.latitude, o.longitude, d.latitude, d.longitude,"
                    " a.airplane_id, COALESCE(" + t.parking + ", ''), IIF(o.city = '', o.name, o.city),"
                    " IIF(d.city = '', d.name, d.city)"
                    " FROM " + t.flight + " f JOIN airplane a USING (airplane_id)"
                    " JOIN aircraft_type t USING (aircraft_type_id)"
                    " JOIN airport o ON o.airport_id = f.origin_airport_id"
                    " JOIN airport d ON d.airport_id = f." + t.destination + " " + where +
                    " ORDER BY f." + t.key + " LIMIT 1");
    if (!whereValue.empty()) q.bind(1, whereValue);
    if (!q.next()) return std::nullopt;
    PlannedFlightView v;
    v.id = q.integer(0);
    v.registration = q.text(1);
    v.typeCode = q.text(2);
    v.airframeId = q.text(3);
    v.origin = q.text(4);
    v.originName = q.text(5);
    v.destination = q.text(6);
    v.destinationName = q.text(7);
    v.flightNumber = q.text(8);
    v.kind = parseKind(q.text(9)).value_or(FlightKind::Passenger);
    v.family = q.text(10);
    v.role = q.text(11);
    v.cruiseSpeedKt = static_cast<int>(q.integer(12));
    v.cargoLb = static_cast<int>(q.integer(13));
    v.fuelPriceCentsPer100Lb = q.integer(14);
    v.plannedAt = q.text(15);
    v.originRunway = q.text(16);
    v.destinationRunway = q.text(17);
    v.distanceNm = distanceMeters(q.real(18), q.real(19), q.real(20), q.real(21)) / kMetresPerNm;
    v.loadInParking = q.text(23);
    v.originCity = q.text(24);
    v.destinationCity = q.text(25);
    const std::int64_t airplaneId = q.integer(22);
    for (const Cabin& c : cabinsOf(db, airplaneId)) v.seatsInstalled += c.seats;
    v.limits = airplaneLimits(db, airplaneId).value_or(AirplaneLimits{});
    v.holds = holdsOf(db, airplaneId);

    db::Statement cabins(db, std::string("SELECT cabin_class, passengers FROM ") + t.cabins + " WHERE " + t.key +
                                 " = ? ORDER BY CASE cabin_class WHEN 'first' THEN 0 WHEN 'business' THEN 1 ELSE 2 END");
    cabins.bind(1, v.id);
    while (cabins.next()) v.cabins.push_back({cabins.text(0), static_cast<int>(cabins.integer(1))});

    db::Statement plan(db,
                       std::string("SELECT request_id, generated_at, route, origin_runway, destination_runway,"
                                   " cruise_altitude_ft, est_block_s, ramp_fuel_lb, enroute_burn_lb,"
                                   " est_landing_weight_lb, origin_transition_altitude_ft,"
                                   " destination_transition_level_ft, ofp_json FROM ") + t.plan + " WHERE " + t.key +
                           " = ?");
    plan.bind(1, v.id);
    v.route.push_back({q.real(18), q.real(19)});
    if (plan.next()) {
        for (const RoutePoint& p : parseNavlog(plan.text(12))) v.route.push_back(p);
        v.brief = parsePlanBrief(plan.text(12));
        SimbriefPlan p;
        p.requestId = plan.text(0);
        p.generatedAt = plan.text(1);
        p.route = plan.text(2);
        p.originRunway = plan.text(3);
        p.destinationRunway = plan.text(4);
        p.cruiseAltitudeFt = static_cast<int>(plan.integer(5));
        p.estBlockS = static_cast<int>(plan.integer(6));
        p.rampFuelLb = static_cast<int>(plan.integer(7));
        p.enrouteBurnLb = static_cast<int>(plan.integer(8));
        p.estLandingWeightLb = static_cast<int>(plan.integer(9));
        p.originTransitionAltitudeFt = static_cast<int>(plan.integer(10));
        p.destinationTransitionLevelFt = static_cast<int>(plan.integer(11));
        p.originIcao = v.origin;
        p.destinationIcao = v.destination;
        p.airframeId = v.airframeId;
        p.units = "lbs";
        v.plan = p;
    }
    v.route.push_back({q.real(20), q.real(21)});
    return v;
}

}  // namespace

std::optional<PlannedFlightView> currentPlannedFlight(db::Database& db) {
    auto v = readFlightView(db, kPlannedTables, "", "");
    if (!v) return v;
    // The latest flight's spot, if it ended where this one starts (D-148).
    db::Statement last(db,
                       "SELECT COALESCE(l.arrival_parking, '') FROM flight l JOIN airport ap"
                       " ON ap.airport_id = l.arrival_airport_id JOIN airplane a ON a.airplane_id = l.airplane_id"
                       " WHERE a.registration = ? AND ap.icao_code = ?"
                       " AND l.block_in_at = (SELECT max(block_in_at) FROM flight WHERE airplane_id = l.airplane_id)");
    last.bind(1, std::string_view(v->registration));
    last.bind(2, std::string_view(v->origin));
    if (last.next()) v->loadInParking = last.text(0);
    return v;
}

std::optional<PlannedFlightView> flownFlightView(db::Database& db, std::string_view logFlightId) {
    return readFlightView(db, kFlownTables, "WHERE f.log_flight_id = ?", logFlightId);
}

std::optional<std::string> latestFlightId(db::Database& db) {
    db::Statement q(db, "SELECT log_flight_id FROM flight ORDER BY block_in_at DESC LIMIT 1");
    if (!q.next()) return std::nullopt;
    return q.text(0);
}

bool cancelPlannedFlight(db::Database& db, std::string* error) { return db.exec("DELETE FROM planned_flight", error); }

bool storeSimbriefPlan(db::Database& db, std::int64_t plannedFlightId, const SimbriefPlan& p, std::string_view ofpJson,
                       std::string_view importedAt, std::string* error) {
    db::Statement q(db,
                    "INSERT OR REPLACE INTO simbrief_plan (planned_flight_id, request_id, generated_at, imported_at,"
                    " route, origin_runway, destination_runway, cruise_altitude_ft, est_block_s, ramp_fuel_lb,"
                    " enroute_burn_lb, est_landing_weight_lb, origin_transition_altitude_ft,"
                    " destination_transition_level_ft, ofp_json) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.bind(1, plannedFlightId);
    q.bind(2, p.requestId);
    q.bind(3, p.generatedAt);
    q.bind(4, importedAt);
    q.bind(5, p.route);
    q.bind(6, p.originRunway);
    q.bind(7, p.destinationRunway);
    q.bind(8, static_cast<std::int64_t>(p.cruiseAltitudeFt));
    q.bind(9, static_cast<std::int64_t>(p.estBlockS));
    q.bind(10, static_cast<std::int64_t>(p.rampFuelLb));
    q.bind(11, static_cast<std::int64_t>(p.enrouteBurnLb));
    q.bind(12, static_cast<std::int64_t>(p.estLandingWeightLb));
    q.bind(13, static_cast<std::int64_t>(p.originTransitionAltitudeFt));
    q.bind(14, static_cast<std::int64_t>(p.destinationTransitionLevelFt));
    q.bind(15, ofpJson);
    if (q.run()) return true;
    if (error) *error = q.error();
    return false;
}

std::optional<FlightTarget> flightTarget(db::Database& db, const PlannedFlightView& flight) {
    db::Statement q(db, "SELECT t.sim_folder, o.latitude, o.longitude, a.fuel_on_board_lb, d.latitude, d.longitude"
                        " FROM planned_flight f"
                        " JOIN airplane a USING (airplane_id) JOIN aircraft_type t USING (aircraft_type_id)"
                        " JOIN airport o ON o.airport_id = f.origin_airport_id"
                        " JOIN airport d ON d.airport_id = f.destination_airport_id WHERE f.planned_flight_id = ?");
    q.bind(1, flight.id);
    if (!q.next()) return std::nullopt;
    FlightTarget t;
    t.plannedFlightId = flight.id;
    t.plannedAt = flight.plannedAt;
    t.simFolder = q.text(0);
    t.typeCode = flight.typeCode;
    t.registration = flight.registration;
    t.originIcao = flight.origin;
    t.destinationIcao = flight.destination;
    t.originLat = q.real(1);
    t.originLon = q.real(2);
    t.fuelOnRecordLb = static_cast<int>(q.integer(3));
    t.destinationLat = q.real(4);
    t.destinationLon = q.real(5);
    if (flight.plan) {  // the phases' plan values (D-124)
        t.cruiseAltitudeFt = flight.plan->cruiseAltitudeFt;
        t.transitionAltitudeFt = flight.plan->originTransitionAltitudeFt;
        t.transitionLevelFt = flight.plan->destinationTransitionLevelFt;
    }
    return t;
}

PriceInput priceInput(const PlannedFlightView& f) {
    PriceInput in;
    in.kind = f.kind;
    in.family = f.family;
    in.role = f.role;
    in.seatsInstalled = f.seatsInstalled;
    in.cabins = f.cabins;
    in.cargoLb = f.cargoLb;
    in.distanceNm = f.distanceNm;
    in.mlwLb = f.limits.mlwLb;
    in.fuelPriceCentsPer100Lb = f.fuelPriceCentsPer100Lb;
    if (f.plan) {
        in.blockHours = f.plan->estBlockS / 3600.0;
        in.fuelBurnLb = f.plan->enrouteBurnLb;
    } else {
        in.blockHours = estimatedBlockHours(f.distanceNm, f.cruiseSpeedKt);
    }
    return in;
}

bool nearMlw(const PlannedFlightView& f) {
    return f.plan && f.limits.mlwLb > 0 && f.plan->estLandingWeightLb * 100 >= 95LL * f.limits.mlwLb;
}

}  // namespace blocktime
