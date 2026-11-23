// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airline_queries.h"

#include "geo.h"

namespace blocktime {

namespace {

// Which lease month of a lease from `leasedOn` starts on `periodStart`.
int periodIndex(Date leasedOn, Date periodStart) {
    const auto ym = [](Date d) { return static_cast<int>(d.year()) * 12 + static_cast<int>(static_cast<unsigned>(d.month())); };
    return ym(periodStart) - ym(leasedOn);
}

}  // namespace

std::vector<AirplaneView> listAirplanes(db::Database& db) {
    std::vector<AirplaneView> out;
    db::Statement q(db,
                    "SELECT a.registration, COALESCE(a.tail_number, ''), t.code, a.airframe_name, f.name, p.icao_code,"
                    " a.fuel_on_board_lb, a.awaiting_delivery, a.returned_on, COALESCE(a.return_reason, '')"
                    " FROM airplane a JOIN aircraft_type t USING (aircraft_type_id) JOIN fleet f USING (fleet_id)"
                    " JOIN airport p ON p.airport_id = a.location_airport_id ORDER BY a.registration");
    while (q.next()) {
        AirplaneView a;
        a.registration = q.text(0);
        a.tailNumber = q.text(1);
        a.typeCode = q.text(2);
        a.airframeName = q.text(3);
        a.fleet = q.text(4);
        a.locationIcao = q.text(5);
        a.fuelOnBoardLb = static_cast<int>(q.integer(6));
        a.awaitingDelivery = q.integer(7) != 0;
        if (!q.isNull(8)) a.returnedOn = parseDate(q.text(8));
        a.returnReason = q.text(9);
        out.push_back(std::move(a));
    }
    return out;
}

std::vector<LedgerView> listLedger(db::Database& db) {
    std::vector<LedgerView> out;
    db::Statement q(db,
                    "SELECT l.posted_at, l.amount_cents, c.label, a.registration, a.leased_on, lc.period_start,"
                    " fa.registration, o.icao_code, d.icao_code"
                    " FROM ledger_line l JOIN ledger_category c ON c.code = l.category"
                    " LEFT JOIN lease_charge lc ON lc.ledger_line_id = l.ledger_line_id"
                    " LEFT JOIN airplane a ON a.airplane_id = lc.airplane_id"
                    " LEFT JOIN flight_money_line fm ON fm.ledger_line_id = l.ledger_line_id"
                    " LEFT JOIN flight f ON f.flight_id = fm.flight_id"
                    " LEFT JOIN airplane fa ON fa.airplane_id = f.airplane_id"
                    " LEFT JOIN airport o ON o.airport_id = f.origin_airport_id"
                    " LEFT JOIN airport d ON d.airport_id = f.arrival_airport_id"
                    " ORDER BY l.posted_at DESC, l.ledger_line_id DESC");
    while (q.next()) {
        LedgerView line{q.text(0), q.text(2), q.integer(1)};
        const auto leasedOn = parseDate(q.text(4));
        const auto start = parseDate(q.text(5));
        if (leasedOn && start) {
            const Date next = leasePeriodStart(*leasedOn, periodIndex(*leasedOn, *start) + 1);
            line.text = "Lease " + q.text(3) + ", " + formatLeasePeriod(*start, next);
        } else if (!q.isNull(6)) {
            line.text = q.text(6) + " " + q.text(7) + "\u2013" + q.text(8) + " \u00b7 " + line.text;  // "N814SX KBFI–KSAN · Fuel"
        }
        out.push_back(std::move(line));
    }
    return out;
}

Cents cashCents(db::Database& db) {
    db::Statement q(db, "SELECT COALESCE(SUM(amount_cents), 0) FROM ledger_line");
    return q.next() ? q.integer(0) : 0;
}

Cents cashCentsAsOf(db::Database& db, std::string_view instant) {
    db::Statement q(db, "SELECT COALESCE(SUM(amount_cents), 0) FROM ledger_line WHERE posted_at <= ?");
    q.bind(1, instant);
    return q.next() ? q.integer(0) : 0;
}

std::optional<NearestAirport> nearestAirport(db::Database& db, double lat, double lon) {
    // A few thousand airports: scanning them all is quicker than being clever, and it's done once per flight.
    db::Statement q(db, "SELECT airport_id, icao_code, name, latitude, longitude FROM airport");
    std::optional<NearestAirport> best;
    while (q.next()) {
        const double d = distanceMeters(lat, lon, q.real(3), q.real(4));
        if (!best || d < best->distanceM) best = NearestAirport{q.integer(0), q.text(1), q.text(2), d};
    }
    return best;
}

std::set<std::string> settledLogFlightIds(db::Database& db) {
    std::set<std::string> ids;
    db::Statement q(db, "SELECT log_flight_id FROM flight");
    while (q.next()) ids.insert(q.text(0));
    return ids;
}

double hoursFlown(db::Database& db) {
    db::Statement q(db, "SELECT COALESCE(SUM(unixepoch(block_in_at) - unixepoch(block_out_at)), 0) FROM flight");
    return q.next() ? static_cast<double>(q.integer(0)) / 3600.0 : 0.0;
}

int airportCount(db::Database& db) {
    db::Statement q(db, "SELECT COUNT(*) FROM airport");
    return q.next() ? static_cast<int>(q.integer(0)) : 0;
}

}  // namespace blocktime

namespace blocktime {

std::optional<AirplaneLimits> airplaneLimits(db::Database& db, std::int64_t airplaneId) {
    db::Statement q(db,
                    "SELECT min_runway_ft, mlw_lb, passenger_weight_lb, bag_weight_lb, max_payload_lb"
                    " FROM airplane_limits WHERE airplane_id = ?");
    q.bind(1, airplaneId);
    if (!q.next()) return std::nullopt;
    return AirplaneLimits{static_cast<int>(q.integer(0)), static_cast<int>(q.integer(1)),
                          static_cast<int>(q.integer(2)), static_cast<int>(q.integer(3)),
                          static_cast<int>(q.integer(4))};
}

}  // namespace blocktime
