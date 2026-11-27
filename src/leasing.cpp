// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "leasing.h"

#include <cstdint>
#include <format>
#include <map>
#include <queue>
#include <set>

#include "airline_queries.h"
#include "money.h"

namespace blocktime {

namespace {

// One lease month to settle: a new airplane's first month (`config` set) or a leased airplane's next month.
struct LeaseEvent {
    Date date;
    std::string registration;
    int period = 0;
    std::int64_t airplaneId = 0;            // leased airplanes
    Date leasedOn;                          // leased airplanes
    Cents monthlyCents = 0;                 // leased airplanes
    const ConfigAirplane* config = nullptr;  // new airplanes

    // Min-heap order: the earliest date first, then by registration.
    bool operator>(const LeaseEvent& other) const {
        if (date != other.date) return date > other.date;
        return registration > other.registration;
    }
};

using EventQueue = std::priority_queue<LeaseEvent, std::vector<LeaseEvent>, std::greater<>>;

std::optional<std::int64_t> idOf(db::Database& db, const char* sql, const std::string& key) {
    db::Statement q(db, sql);
    q.bind(1, key);
    if (!q.next()) return std::nullopt;
    return q.integer(0);
}

// Airplanes in the fleet: leased and not returned. Slots cap them (design.md §6). nullopt if the read failed.
std::optional<std::int64_t> fleetSize(db::Database& db) {
    db::Statement q(db, "SELECT COUNT(*) FROM airplane WHERE returned_on IS NULL");
    if (!q.next()) return std::nullopt;
    return q.integer(0);
}

// The registration of the leased airplane that already has this SimBrief airframe, or "".
std::string registrationWithSimbriefId(db::Database& db, const std::string& simbriefId) {
    db::Statement q(db, "SELECT registration FROM airplane WHERE simbrief_airframe_id = ?");
    q.bind(1, simbriefId);
    return q.next() ? q.text(0) : std::string();
}

// Posts a lease line and its lease_charge row. Call inside a transaction.
bool postLeaseMonth(db::Database& db, std::int64_t airplaneId, Date periodStart, Cents charge, std::string& error) {
    db::Statement line(db, "INSERT INTO ledger_line (category, posted_at, amount_cents) VALUES ('lease', ?, ?)");
    line.bind(1, formatInstant(periodStart));
    line.bind(2, -charge);
    if (!line.run()) {
        error = line.error();
        return false;
    }
    db::Statement charged(db, "INSERT INTO lease_charge (ledger_line_id, airplane_id, period_start) VALUES (last_insert_rowid(), ?, ?)");
    charged.bind(1, airplaneId);
    charged.bind(2, formatDate(periodStart));
    if (!charged.run()) {
        error = charged.error();
        return false;
    }
    return true;
}

bool insertLimits(db::Database& db, std::int64_t airplaneId, const AirplaneLimits& l, std::string& error) {
    db::Statement q(db,
                    "INSERT INTO airplane_limits (airplane_id, min_runway_ft, mlw_lb, passenger_weight_lb, bag_weight_lb,"
                    " max_payload_lb) VALUES (?, ?, ?, ?, ?, ?)");
    q.bind(1, airplaneId);
    q.bind(2, static_cast<std::int64_t>(l.minRunwayFt));
    q.bind(3, static_cast<std::int64_t>(l.mlwLb));
    q.bind(4, static_cast<std::int64_t>(l.passengerWeightLb));
    q.bind(5, static_cast<std::int64_t>(l.bagWeightLb));
    q.bind(6, static_cast<std::int64_t>(l.maxPayloadLb));
    if (q.run()) return true;
    error = q.error();
    return false;
}

// Inserts a new airplane copied from its airframe (D-082), with its first lease month. Call inside a transaction.
std::optional<std::int64_t> insertAirplane(db::Database& db, const ConfigAirplane& a, const CatalogAirframe& airframe,
                                           const CatalogType& type, std::int64_t airportId, std::string& error) {
    const auto typeId = idOf(db, "SELECT aircraft_type_id FROM aircraft_type WHERE code = ?", type.code);
    const auto fleetId = idOf(db, "SELECT fleet_id FROM fleet WHERE name = ?", a.fleet);
    if (!typeId || !fleetId) {
        error = a.registration + ": its type or fleet isn't in the database";
        return std::nullopt;
    }
    db::Statement insert(db,
                         "INSERT INTO airplane (registration, tail_number, aircraft_type_id, fleet_id, airframe_name,"
                         " simbrief_airframe_id, leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb,"
                         " awaiting_delivery) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0, ?)");
    insert.bind(1, a.registration);
    if (a.tailNumber.empty()) {
        insert.bindNull(2);
    } else {
        insert.bind(2, a.tailNumber);
    }
    insert.bind(3, *typeId);
    insert.bind(4, *fleetId);
    insert.bind(5, airframe.name);
    insert.bind(6, a.simbriefAirframeId);
    insert.bind(7, formatDate(a.leasedOn));
    insert.bind(8, type.monthlyLeaseCents);
    insert.bind(9, airportId);
    insert.bind(10, static_cast<std::int64_t>(a.awaitingDelivery ? 1 : 0));
    if (!insert.run()) {
        error = a.registration + ": " + insert.error();
        return std::nullopt;
    }
    const std::int64_t id = *idOf(db, "SELECT airplane_id FROM airplane WHERE registration = ?", a.registration);

    for (const Cabin& c : airframe.cabins) {
        db::Statement cabin(db, "INSERT INTO airplane_cabin (airplane_id, cabin_class, seats) VALUES (?, ?, ?)");
        cabin.bind(1, id);
        cabin.bind(2, c.cabinClass);
        cabin.bind(3, static_cast<std::int64_t>(c.seats));
        if (!cabin.run()) {
            error = cabin.error();
            return std::nullopt;
        }
    }
    for (const Hold& h : airframe.holds) {
        db::Statement hold(db, "INSERT INTO airplane_hold (airplane_id, hold, max_lb, ratio) VALUES (?, ?, ?, ?)");
        hold.bind(1, id);
        hold.bind(2, h.hold);
        hold.bind(3, static_cast<std::int64_t>(h.maxLb));
        hold.bind(4, static_cast<std::int64_t>(h.ratio));
        if (!hold.run()) {
            error = hold.error();
            return std::nullopt;
        }
    }
    if (!insertLimits(db, id, withOverrides(airframe.limits, a.limits), error)) return std::nullopt;
    if (!postLeaseMonth(db, id, a.leasedOn, type.monthlyLeaseCents, error)) return std::nullopt;
    return id;
}

}  // namespace

bool syncCatalogTypes(db::Database& db, const Catalog& catalog, std::string* error) {
    db::Transaction tx(db);
    if (!tx.active()) {
        if (error) *error = "couldn't start a transaction: " + tx.error();
        return false;
    }
    db::Statement upsert(db,
                         "INSERT INTO aircraft_type (code, family, role, turnaround_min, cruise_speed_kt, sim_folder)"
                         " VALUES (?, ?, ?, ?, ?, ?)"
                         " ON CONFLICT (code) DO UPDATE SET family = excluded.family, role = excluded.role,"
                         " turnaround_min = excluded.turnaround_min, cruise_speed_kt = excluded.cruise_speed_kt,"
                         " sim_folder = excluded.sim_folder");
    for (const CatalogType& t : catalog.types) {
        upsert.reset();
        upsert.bind(1, t.code);
        upsert.bind(2, t.family);
        upsert.bind(3, t.role);
        upsert.bind(4, static_cast<std::int64_t>(t.turnaroundMin));
        upsert.bind(5, static_cast<std::int64_t>(t.cruiseSpeedKt));
        upsert.bind(6, t.simFolder);
        if (!upsert.run()) {
            if (error) *error = t.code + ": " + upsert.error();
            return false;
        }
    }
    return tx.commit(error);
}

bool syncFleets(db::Database& db, const AirlineConfig& config, std::string* error) {
    db::Transaction tx(db);
    if (!tx.active()) {
        if (error) *error = "couldn't start a transaction: " + tx.error();
        return false;
    }
    db::Statement insert(db, "INSERT INTO fleet (name) VALUES (?) ON CONFLICT (name) DO NOTHING");
    for (const std::string& name : config.fleets) {
        insert.reset();
        insert.bind(1, name);
        if (!insert.run()) {
            if (error) *error = name + ": " + insert.error();
            return false;
        }
    }
    return tx.commit(error);
}

bool postOpeningCash(db::Database& db, const AirlineConfig& config, std::string* error) {
    db::Statement insert(db,
                         "INSERT INTO ledger_line (category, posted_at, amount_cents) SELECT 'opening_cash', ?, ?"
                         " WHERE NOT EXISTS (SELECT 1 FROM ledger_line WHERE category = 'opening_cash')");
    insert.bind(1, formatInstant(config.foundedOn));
    insert.bind(2, roundToFiveDollars(config.startingCashCents));
    if (!insert.run()) {
        if (error) *error = insert.error();
        return false;
    }
    return true;
}

int slotsEarned(double hoursFlown) {
    int n = kStartingSlots;
    while (hoursForSlot(n + 1) <= hoursFlown) ++n;
    return n;
}

double hoursForSlot(int n) {
    const int earned = n - kStartingSlots + 1;  // slot 3 is the first earned one
    return earned <= 1 ? 0.0 : 5.0 * earned * (earned - 1);
}

LeaseOutcome settleLeases(db::Database& db, const Catalog& catalog, const AirlineConfig& config, Date today,
                          std::optional<int> slots) {
    LeaseOutcome outcome;
    // A read that fails stops the run (review 1.4): read as 0, it would return airplanes for good.
    const auto readFailed = [&outcome](const std::string& what) {
        outcome.error = "couldn't read " + what;
        return outcome;
    };
    const auto hoursRead = hoursFlown(db);
    if (!hoursRead) return readFailed("the hours flown");
    const double hours = *hoursRead;
    const int slotCount = slots.value_or(slotsEarned(hours));
    EventQueue events;

    // Leased airplanes: their next uncharged month. Months are charged in order, so it's the count of charged months.
    std::set<std::string> known;
    {
        db::Statement q(db,
                        "SELECT a.airplane_id, a.registration, a.leased_on, a.monthly_lease_cents, a.returned_on IS NULL,"
                        " (SELECT COUNT(*) FROM lease_charge c WHERE c.airplane_id = a.airplane_id) FROM airplane a");
        while (q.next()) {
            known.insert(q.text(1));
            const auto leasedOn = parseDate(q.text(2));
            if (q.integer(4) == 0 || !leasedOn) continue;  // returned
            LeaseEvent e;
            e.registration = q.text(1);
            e.airplaneId = q.integer(0);
            e.leasedOn = *leasedOn;
            e.monthlyCents = q.integer(3);
            e.period = static_cast<int>(q.integer(5));
            e.date = leasePeriodStart(e.leasedOn, e.period);
            if (e.date <= today) events.push(e);
        }
        if (!q.error().empty()) return readFailed("the airplanes: " + q.error());
    }
    // New airplanes from the config whose lease has started.
    for (const ConfigAirplane& a : config.airplanes) {
        if (known.contains(a.registration) || a.leasedOn > today) continue;
        LeaseEvent e;
        e.date = a.leasedOn;
        e.registration = a.registration;
        e.config = &a;
        events.push(e);
    }

    while (!events.empty()) {
        LeaseEvent e = events.top();
        events.pop();
        const auto cashRead = cashCentsAsOf(db, formatInstant(e.date));
        if (!cashRead) return readFailed("the cash on " + formatDate(e.date));
        const Cents cash = *cashRead;
        std::string error;

        if (e.config) {
            const ConfigAirplane& a = *e.config;
            const CatalogAirframe* airframe = catalog.airframe(a.airframe);
            const CatalogType* type = airframe ? catalog.type(airframe->typeCode) : nullptr;
            if (!airframe || !type) {
                outcome.notices.push_back(std::format("{}: no airframe \"{}\" in catalog.yaml.", a.registration, a.airframe));
                continue;
            }
            const auto airportId = idOf(db, "SELECT airport_id FROM airport WHERE icao_code = ?", a.locationIcao);
            if (!airportId) {
                outcome.notices.push_back(std::format("{}: {} isn't in the airport data. Run blocktime-import (README).",
                                                      a.registration, a.locationIcao));
                continue;
            }
            if (cash < type->monthlyLeaseCents) {
                // The check is the cash as of leased_on (D-096), so only a later leased_on changes the answer.
                outcome.notices.push_back(std::format(
                    "Can't afford {}'s first month: {} needed, {} in cash on {}. Set a later leased_on to try again.",
                    a.registration, formatMoneyFull(type->monthlyLeaseCents), formatMoneyFull(cash),
                    formatDayMonthYear(e.date)));
                continue;
            }
            const auto inFleetRead = fleetSize(db);
            if (!inFleetRead) return readFailed("the fleet size");
            if (const std::int64_t inFleet = *inFleetRead; inFleet >= slotCount) {
                outcome.notices.push_back(std::format(
                    "Can't lease {} yet: {} of {} slots in use. Slot {} comes at {:.0f} hours flown (you have {:.1f}).",
                    a.registration, inFleet, slotCount, slotCount + 1, hoursForSlot(slotCount + 1), hours));
                continue;
            }
            if (const std::string owner = registrationWithSimbriefId(db, a.simbriefAirframeId); !owner.empty()) {
                outcome.notices.push_back(std::format(
                    "{}: simbrief_airframe_id {} already belongs to {}; a leased airplane can't be renamed in "
                    "airline.yaml (D-082).",
                    a.registration, a.simbriefAirframeId, owner));
                continue;
            }
            db::Transaction tx(db);
            if (!tx.active()) {
                outcome.error = "couldn't start a transaction: " + tx.error();
                return outcome;
            }
            const auto id = insertAirplane(db, a, *airframe, *type, *airportId, error);
            if (!id) {
                // This airplane only: the transaction rolls back, and every other lease still settles.
                outcome.notices.push_back(std::format("{} couldn't be leased: {}", a.registration, error));
                continue;
            }
            if (!tx.commit(&error)) {
                outcome.error = error;
                return outcome;
            }
            ++outcome.leased;
            ++outcome.monthsCharged;
            e.airplaneId = *id;
            e.leasedOn = a.leasedOn;
            e.monthlyCents = type->monthlyLeaseCents;
            e.config = nullptr;
        } else if (cash < e.monthlyCents) {
            db::Statement ret(db, "UPDATE airplane SET returned_on = ?, return_reason = 'unpaid' WHERE airplane_id = ?");
            ret.bind(1, formatDate(e.date));
            ret.bind(2, e.airplaneId);
            if (!ret.run()) {
                outcome.error = ret.error();
                return outcome;
            }
            ++outcome.returned;
            outcome.notices.push_back(std::format("{} returned on {}: the lease month couldn't be paid.", e.registration,
                                                  formatDayMonthYear(e.date)));
            continue;  // no later months
        } else {
            db::Transaction tx(db);
            if (!tx.active()) {
                outcome.error = "couldn't start a transaction: " + tx.error();
                return outcome;
            }
            if (!postLeaseMonth(db, e.airplaneId, e.date, e.monthlyCents, error) || !tx.commit(&error)) {
                outcome.error = error;
                return outcome;
            }
            ++outcome.monthsCharged;
        }

        // Queue the airplane's next month.
        ++e.period;
        e.date = leasePeriodStart(e.leasedOn, e.period);
        if (e.date <= today) events.push(e);
    }
    return outcome;
}

bool fillMissingLimits(db::Database& db, const Catalog& catalog, const AirlineConfig& config, std::string* error) {
    struct Missing {
        std::int64_t id;
        std::string registration, airframe;
    };
    std::vector<Missing> missing;
    {
        db::Statement q(db, "SELECT airplane_id, registration, airframe_name FROM airplane"
                            " WHERE airplane_id NOT IN (SELECT airplane_id FROM airplane_limits)");
        while (q.next()) missing.push_back({q.integer(0), q.text(1), q.text(2)});
    }
    std::string message;
    for (const Missing& m : missing) {
        const CatalogAirframe* airframe = catalog.airframe(m.airframe);
        if (!airframe) continue;  // happy path only (D-099)
        LimitOverrides overrides;
        for (const ConfigAirplane& a : config.airplanes) {
            if (a.registration == m.registration) overrides = a.limits;
        }
        if (!insertLimits(db, m.id, withOverrides(airframe->limits, overrides), message)) {
            if (error) *error = message;
            return false;
        }
    }
    return true;
}

bool syncFreeFlightNumbers(db::Database& db, const AirlineConfig& config, std::string* error) {
    db::Transaction tx(db);
    std::string message;
    if (!tx.active()) {
        if (error) *error = "couldn't begin a transaction";
        return false;
    }
    // Clear first, so two airplanes can swap numbers without tripping the unique index.
    if (!db.exec("UPDATE airplane SET free_flight_number = NULL WHERE free_flight_number IS NOT NULL", &message)) {
        if (error) *error = message;
        return false;
    }
    for (const ConfigAirplane& a : config.airplanes) {
        if (a.flightNumber.empty()) continue;
        db::Statement q(db, "UPDATE airplane SET free_flight_number = ? WHERE registration = ?");
        q.bind(1, a.flightNumber);
        q.bind(2, a.registration);
        if (!q.run()) {
            if (error) *error = a.registration + ": " + q.error();
            return false;
        }
    }
    return tx.commit(error);
}

std::vector<std::string> configDriftWarnings(db::Database& db, const AirlineConfig& config) {
    std::vector<std::string> warnings;
    std::map<std::string, const ConfigAirplane*> byRegistration;
    for (const ConfigAirplane& a : config.airplanes) byRegistration[a.registration] = &a;

    db::Statement q(db,
                    "SELECT a.registration, a.airframe_name, f.name, a.simbrief_airframe_id, a.leased_on,"
                    " COALESCE(a.tail_number, ''), a.returned_on IS NULL"
                    " FROM airplane a JOIN fleet f USING (fleet_id) ORDER BY a.registration");
    while (q.next()) {
        const std::string registration = q.text(0);
        const auto it = byRegistration.find(registration);
        if (it == byRegistration.end()) {
            if (q.integer(6) != 0) {
                warnings.push_back(std::format(
                    "{} is no longer in airline.yaml; returning a lease comes with the settings screen (D-092).", registration));
            }
            continue;
        }
        const ConfigAirplane& a = *it->second;
        const std::pair<const char*, bool> fields[] = {
            {"airframe", a.airframe != q.text(1)},
            {"fleet", a.fleet != q.text(2)},
            {"simbrief_airframe_id", a.simbriefAirframeId != q.text(3)},
            {"leased_on", formatDate(a.leasedOn) != q.text(4)},
            {"tail_number", a.tailNumber != q.text(5)},
        };
        for (const auto& [name, changed] : fields) {
            if (changed) {
                warnings.push_back(std::format(
                    "{}: airline.yaml changed {}, but the database keeps the leased values (D-082).", registration, name));
            }
        }
    }
    return warnings;
}

}  // namespace blocktime
