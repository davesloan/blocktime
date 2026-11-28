// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "accepted_breakdown.h"

#include <cmath>
#include <utility>

#include <sqlite3.h>

namespace blocktime {

namespace {

bool failWith(db::Statement& q, std::string* error) {
    if (error) *error = q.error();
    return false;
}

// The breakdown linked from `linkTable` (keyed by `keyColumn`), if any.
std::optional<std::int64_t> linkedBreakdown(db::Database& db, const char* linkTable, const char* keyColumn,
                                            std::int64_t key) {
    db::Statement q(db, std::string("SELECT accepted_breakdown_id FROM ") + linkTable + " WHERE " + keyColumn + " = ?");
    q.bind(1, key);
    if (!q.next()) return std::nullopt;
    return q.integer(0);
}

std::optional<AcceptedBreakdown> readBreakdown(db::Database& db, std::int64_t breakdownId) {
    db::Statement head(db, "SELECT accepted_at, at_block_out FROM accepted_breakdown WHERE accepted_breakdown_id = ?");
    head.bind(1, breakdownId);
    if (!head.next()) return std::nullopt;
    AcceptedBreakdown b;
    b.acceptedAt = head.text(0);
    b.atBlockOut = head.integer(1) != 0;
    b.money.fuelIncluded = true;

    db::Statement lines(db, "SELECT position, category, label, cents, rate_micro_dollars, rate_basis, rate_unit"
                            " FROM accepted_line WHERE accepted_breakdown_id = ? ORDER BY position");
    lines.bind(1, breakdownId);
    while (lines.next()) {
        MoneyLine m;
        const std::int64_t position = lines.integer(0);
        m.category = lines.text(1);
        m.label = lines.text(2);
        m.cents = lines.integer(3);
        m.rateDollars = static_cast<double>(lines.integer(4)) / 1e6;
        m.rateBasis = static_cast<int>(lines.integer(5));
        m.rateUnit = lines.text(6);
        db::Statement qs(db, "SELECT value, unit FROM accepted_line_quantity WHERE accepted_breakdown_id = ?"
                             " AND position = ? ORDER BY quantity_position");
        qs.bind(1, breakdownId);
        qs.bind(2, position);
        while (qs.next()) m.quantities.push_back({qs.real(0), qs.text(1)});
        b.money.totalCents += m.cents;
        (m.cents > 0 ? b.money.income : b.money.costs).push_back(std::move(m));
    }
    return b;
}

bool insertLines(db::Database& db, std::int64_t breakdownId, const Preview& money, std::string* error) {
    std::int64_t position = 0;
    for (const auto* lines : {&money.income, &money.costs}) {
        for (const MoneyLine& m : *lines) {
            db::Statement line(db, "INSERT INTO accepted_line (accepted_breakdown_id, position, category, label, cents,"
                                   " rate_micro_dollars, rate_basis, rate_unit) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
            line.bind(1, breakdownId);
            line.bind(2, position);
            line.bind(3, std::string_view(m.category));
            line.bind(4, std::string_view(m.label));
            line.bind(5, m.cents);
            line.bind(6, static_cast<std::int64_t>(std::llround(m.rateDollars * 1e6)));
            line.bind(7, static_cast<std::int64_t>(m.rateBasis));
            line.bind(8, std::string_view(m.rateUnit));
            if (!line.run()) return failWith(line, error);
            std::int64_t quantityPosition = 0;
            for (const Quantity& q : m.quantities) {
                db::Statement qty(db, "INSERT INTO accepted_line_quantity (accepted_breakdown_id, position,"
                                      " quantity_position, value, unit) VALUES (?, ?, ?, ?, ?)");
                qty.bind(1, breakdownId);
                qty.bind(2, position);
                qty.bind(3, quantityPosition++);
                qty.bind(4, q.value);
                qty.bind(5, std::string_view(q.unit));
                if (!qty.run()) return failWith(qty, error);
            }
            ++position;
        }
    }
    return true;
}

}  // namespace

bool dropPlannedAcceptance(db::Database& db, std::int64_t plannedFlightId, std::string* error) {
    const auto old = linkedBreakdown(db, "planned_flight_acceptance", "planned_flight_id", plannedFlightId);
    if (!old) return true;
    db::Statement unlink(db, "DELETE FROM planned_flight_acceptance WHERE planned_flight_id = ?");
    unlink.bind(1, plannedFlightId);
    if (!unlink.run()) return failWith(unlink, error);
    db::Statement drop(db, "DELETE FROM accepted_breakdown WHERE accepted_breakdown_id = ?");  // lines cascade
    drop.bind(1, *old);
    if (!drop.run()) return failWith(drop, error);
    return true;
}

bool acceptPlannedFlight(db::Database& db, std::int64_t plannedFlightId, const Preview& money,
                         std::string_view acceptedAt, bool atBlockOut, std::string* error) {
    db::Transaction tx(db);
    if (!tx.active()) {
        if (error) *error = tx.error();
        return false;
    }
    if (!writeAcceptance(db, plannedFlightId, money, acceptedAt, atBlockOut, error)) return false;
    return tx.commit(error);
}

bool writeAcceptance(db::Database& db, std::int64_t plannedFlightId, const Preview& money, std::string_view acceptedAt,
                     bool atBlockOut, std::string* error) {
    if (!dropPlannedAcceptance(db, plannedFlightId, error)) return false;
    db::Statement head(db, "INSERT INTO accepted_breakdown (accepted_at, at_block_out) VALUES (?, ?)");
    head.bind(1, acceptedAt);
    head.bind(2, static_cast<std::int64_t>(atBlockOut ? 1 : 0));
    if (!head.run()) return failWith(head, error);
    const std::int64_t id = sqlite3_last_insert_rowid(db.handle());
    if (!insertLines(db, id, money, error)) return false;
    db::Statement link(db, "INSERT INTO planned_flight_acceptance (planned_flight_id, accepted_breakdown_id)"
                           " VALUES (?, ?)");
    link.bind(1, plannedFlightId);
    link.bind(2, id);
    if (!link.run()) return failWith(link, error);
    return true;
}

std::optional<AcceptedBreakdown> plannedAcceptance(db::Database& db, std::int64_t plannedFlightId) {
    const auto id = linkedBreakdown(db, "planned_flight_acceptance", "planned_flight_id", plannedFlightId);
    if (!id) return std::nullopt;
    return readBreakdown(db, *id);
}

std::optional<AcceptedBreakdown> flightAcceptance(db::Database& db, std::int64_t flightId) {
    const auto id = linkedBreakdown(db, "flight_acceptance", "flight_id", flightId);
    if (!id) return std::nullopt;
    return readBreakdown(db, *id);
}

bool moveAcceptanceToFlight(db::Database& db, std::int64_t plannedFlightId, std::int64_t flightId,
                            std::string* error) {
    const auto id = linkedBreakdown(db, "planned_flight_acceptance", "planned_flight_id", plannedFlightId);
    if (!id) return true;
    db::Statement unlink(db, "DELETE FROM planned_flight_acceptance WHERE planned_flight_id = ?");
    unlink.bind(1, plannedFlightId);
    if (!unlink.run()) return failWith(unlink, error);
    db::Statement link(db, "INSERT INTO flight_acceptance (flight_id, accepted_breakdown_id) VALUES (?, ?)");
    link.bind(1, flightId);
    link.bind(2, *id);
    if (!link.run()) return failWith(link, error);
    return true;
}

}  // namespace blocktime
