// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "leasing.h"

#include <algorithm>
#include <format>
#include <queue>

#include "airline_queries.h"
#include "money.h"

namespace blocktime {

namespace {

// One lease month to settle: a leased airplane's next month.
struct LeaseEvent {
    Date date;
    std::string registration;
    int period = 0;
    std::int64_t airplaneId = 0;
    Date leasedOn;
    Cents monthlyCents = 0;

    // Min-heap order: the earliest date first, then by registration.
    bool operator>(const LeaseEvent& other) const {
        if (date != other.date) return date > other.date;
        return registration > other.registration;
    }
};

using EventQueue = std::priority_queue<LeaseEvent, std::vector<LeaseEvent>, std::greater<>>;

// Airplanes in the fleet: leased and not returned. Slots cap them (design.md §6). nullopt if the read failed.
std::optional<std::int64_t> fleetSize(db::Database& db) {
    db::Statement q(db, "SELECT COUNT(*) FROM airplane WHERE returned_on IS NULL");
    if (!q.next()) return std::nullopt;
    return q.integer(0);
}

// The registration of the airplane in the fleet whose `column` is `value`, or "".
std::string inFleetWith(db::Database& db, const char* column, const std::string& value) {
    db::Statement q(db, std::format("SELECT registration FROM airplane WHERE {} = ? AND returned_on IS NULL", column));
    q.bind(1, value);
    return q.next() ? q.text(0) : std::string();
}

struct Airport {
    std::int64_t id = 0;
    int longestRunwayFt = 0;
};

std::optional<Airport> airport(db::Database& db, const std::string& icao) {
    db::Statement q(db, "SELECT airport_id, longest_runway_ft FROM airport WHERE icao_code = ?");
    q.bind(1, icao);
    if (!q.next()) return std::nullopt;
    return Airport{q.integer(0), static_cast<int>(q.integer(1))};
}

bool run(db::Statement& q, std::string& error) {
    if (q.run()) return true;
    error = q.error();
    return false;
}

// Posts a lease line and its lease_charge row. Call inside a transaction.
bool postLeaseMonth(db::Database& db, std::int64_t airplaneId, Date periodStart, Cents charge, std::string& error) {
    db::Statement line(db, "INSERT INTO ledger_line (category, posted_at, amount_cents) VALUES ('lease', ?, ?)");
    line.bind(1, formatInstant(periodStart));
    line.bind(2, -charge);
    if (!run(line, error)) return false;
    db::Statement charged(db, "INSERT INTO lease_charge (ledger_line_id, airplane_id, period_start) VALUES (last_insert_rowid(), ?, ?)");
    charged.bind(1, airplaneId);
    charged.bind(2, formatDate(periodStart));
    return run(charged, error);
}

std::string joined(const std::vector<std::string>& items) {
    std::string out;
    for (const std::string& item : items) out += (out.empty() ? "" : ", ") + item;
    return out;
}

bool digitsOnly(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

bool registrationChars(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
                       [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'; });
}

// What a lease would write, worked out by the checks.
struct LeasePlan {
    std::int64_t typeId = 0;
    std::int64_t locationId = 0;
    bool awaitingDelivery = true;
    std::int64_t factoryId = 0;
    std::optional<std::int64_t> deliveryAirportId;  // instant delivery somewhere other than the factory
    Preview delivery;
    std::vector<Hold> holds;
    AirplaneLimits limits;
};

// Every check a lease must pass, in the form's order; fills `plan` when it passes. "" means it would be leased.
std::string check(db::Database& db, const LeaseRequest& req, const Rates& rates, std::optional<int> slots,
                  LeasePlan& plan) {
    if (!req.type || !req.layout) return "Pick a layout.";
    const SimbriefReading& r = req.reading;
    if (const auto missing = missingFields(r); !missing.empty()) {
        std::string list;
        for (const std::string& m : missing) list += (list.empty() ? "" : ", ") + m;
        return "Not read yet: " + list + ".";
    }
    if (const auto zero = zeroFields(r); !zero.empty()) return "Must be more than 0: " + joined(zero) + ".";
    const auto& bases = req.type->simbriefBaseTypes;
    if (std::find(bases.begin(), bases.end(), r.baseType) == bases.end()) {
        return std::format("SimBrief's base type {} isn't a {}.", r.baseType, req.type->code);
    }
    int seats = 0;
    for (const Cabin& c : req.layout->cabins) seats += c.seats;
    if (seats != *r.maxPassengers) {
        return std::format("SimBrief's Max Passengers is {}, but {} has {} seats.", *r.maxPassengers, req.layout->name,
                           seats);
    }
    if (!registrationChars(r.registration)) return "The registration may only use capitals, digits and -.";
    if (*r.mzfwLb <= *r.emptyWeightLb) return "Max Zero Fuel Weight must be more than Empty Weight.";
    if (!req.flightNumber.empty() && (req.flightNumber.size() > 4 || !digitsOnly(req.flightNumber))) {
        return "The flight number must be 1 to 4 digits.";
    }
    if (!inFleetWith(db, "registration", r.registration).empty()) {
        return std::format("{} is already in the fleet.", r.registration);
    }
    if (const std::string owner = inFleetWith(db, "simbrief_airframe_id", r.internalId); !owner.empty()) {
        return std::format("SimBrief airframe {} already belongs to {}.", r.internalId, owner);
    }
    if (!req.flightNumber.empty()) {
        if (const std::string owner = inFleetWith(db, "free_flight_number", req.flightNumber); !owner.empty()) {
            return std::format("Flight number {} belongs to {}.", req.flightNumber, owner);
        }
    }
    const auto hours = hoursFlown(db);
    const auto inFleet = fleetSize(db);
    if (!hours || !inFleet) return "Couldn't read the fleet.";
    const int slotCount = slots.value_or(slotsEarned(*hours));
    if (*inFleet >= slotCount) {
        return std::format("All {} slots are in use. Slot {} comes at {:.0f} hours flown (you have {:.1f}).", slotCount,
                           slotCount + 1, hoursForSlot(slotCount + 1), *hours);
    }
    const auto factory = airport(db, req.type->factoryIcao);
    if (!factory) {
        return std::format("{} (the factory) isn't in the airport data. Run blocktime-import (README).",
                           req.type->factoryIcao);
    }
    plan.locationId = factory->id;
    plan.factoryId = factory->id;
    plan.awaitingDelivery = true;
    if (req.instantDeliveryIcao) {
        const std::string& to = *req.instantDeliveryIcao;
        const auto destination = airport(db, to);
        if (!destination) return std::format("{} isn't in the airport data.", to);
        if (destination->longestRunwayFt < *r.minRunwayFt) {
            return std::format("{}'s longest runway is {} ft; {} needs {} ft.", to,
                               formatNumber(destination->longestRunwayFt), r.registration, formatNumber(*r.minRunwayFt));
        }
        plan.locationId = destination->id;
        plan.awaitingDelivery = false;
        if (to != req.type->factoryIcao) {
            const auto distance = airportDistanceNm(db, req.type->factoryIcao, to);
            if (!distance) return std::format("{} isn't in the airport data.", to);
            plan.deliveryAirportId = destination->id;
            plan.delivery = instantDeliveryPrice(*req.type, *distance, *r.mlwLb, rates);
        }
    }
    const Cents needed = req.type->monthlyLeaseCents - plan.delivery.totalCents;
    if (const Cents cash = cashCents(db); cash < needed) {
        return std::format("Not enough cash: {} needed, {} in cash.", formatMoneyFull(needed), formatMoneyFull(cash));
    }
    {
        db::Statement type(db, "SELECT aircraft_type_id FROM aircraft_type WHERE code = ?");
        type.bind(1, req.type->code);
        if (!type.next()) return std::format("{} isn't in the database yet.", req.type->code);
        plan.typeId = type.integer(0);
    }
    plan.holds = holdsForMaxCargo(req.layout->holds, *r.maxCargoLb);
    plan.limits = {*r.minRunwayFt, *r.mlwLb, *r.passengerWeightLb, *r.bagWeightLb, *r.mzfwLb - *r.emptyWeightLb};
    return "";
}

// Writes the planned lease. Call inside a transaction.
std::optional<std::int64_t> write(db::Database& db, const LeaseRequest& req, const LeasePlan& plan, std::string& error) {
    const SimbriefReading& r = req.reading;
    db::Statement insert(db,
                         "INSERT INTO airplane (registration, tail_number, aircraft_type_id, airframe_name,"
                         " simbrief_airframe_id, leased_on, monthly_lease_cents, location_airport_id, fuel_on_board_lb,"
                         " awaiting_delivery, free_flight_number) VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?)"
                         " RETURNING airplane_id");
    insert.bind(1, r.registration);
    if (r.finNumber.empty()) {
        insert.bindNull(2);
    } else {
        insert.bind(2, r.finNumber);
    }
    insert.bind(3, plan.typeId);
    insert.bind(4, req.layout->name);
    insert.bind(5, r.internalId);
    insert.bind(6, formatDate(req.today));
    insert.bind(7, req.type->monthlyLeaseCents);
    insert.bind(8, plan.locationId);
    insert.bind(9, static_cast<std::int64_t>(plan.awaitingDelivery ? 1 : 0));
    if (req.flightNumber.empty()) {
        insert.bindNull(10);
    } else {
        insert.bind(10, req.flightNumber);
    }
    if (!insert.next()) {
        error = insert.error();
        return std::nullopt;
    }
    const std::int64_t id = insert.integer(0);
    insert.reset();

    for (const Cabin& c : req.layout->cabins) {
        db::Statement cabin(db, "INSERT INTO airplane_cabin (airplane_id, cabin_class, seats) VALUES (?, ?, ?)");
        cabin.bind(1, id);
        cabin.bind(2, c.cabinClass);
        cabin.bind(3, static_cast<std::int64_t>(c.seats));
        if (!run(cabin, error)) return std::nullopt;
    }
    if (!writeHolds(db, id, plan.holds, error) || !writeLimits(db, id, plan.limits, error) ||
        !writeSimbriefValues(db, id, r, formatInstant(req.today), error) ||
        !postLeaseMonth(db, id, req.today, req.type->monthlyLeaseCents, error)) {
        return std::nullopt;
    }
    if (plan.deliveryAirportId) {
        db::Statement delivery(db, "INSERT INTO instant_delivery (airplane_id, from_airport_id, to_airport_id)"
                                   " VALUES (?, ?, ?) RETURNING instant_delivery_id");
        delivery.bind(1, id);
        delivery.bind(2, plan.factoryId);
        delivery.bind(3, *plan.deliveryAirportId);
        if (!delivery.next()) {
            error = delivery.error();
            return std::nullopt;
        }
        const std::int64_t deliveryId = delivery.integer(0);
        delivery.reset();
        for (const MoneyLine& m : plan.delivery.costs) {
            db::Statement line(db, "INSERT INTO ledger_line (category, posted_at, amount_cents) VALUES (?, ?, ?)");
            line.bind(1, m.category);
            line.bind(2, formatInstant(req.today));
            line.bind(3, m.cents);
            if (!run(line, error)) return std::nullopt;
            db::Statement fact(db, "INSERT INTO instant_delivery_line (ledger_line_id, instant_delivery_id)"
                                   " VALUES (last_insert_rowid(), ?)");
            fact.bind(1, deliveryId);
            if (!run(fact, error)) return std::nullopt;
        }
    }
    return id;
}

}  // namespace

bool writeHolds(db::Database& db, std::int64_t airplaneId, const std::vector<Hold>& holds, std::string& error) {
    db::Statement clear(db, "DELETE FROM airplane_hold WHERE airplane_id = ?");
    clear.bind(1, airplaneId);
    if (!run(clear, error)) return false;
    for (const Hold& h : holds) {
        db::Statement hold(db, "INSERT INTO airplane_hold (airplane_id, hold, max_lb, ratio) VALUES (?, ?, ?, ?)");
        hold.bind(1, airplaneId);
        hold.bind(2, h.hold);
        hold.bind(3, static_cast<std::int64_t>(h.maxLb));
        hold.bind(4, static_cast<std::int64_t>(h.ratio));
        if (!run(hold, error)) return false;
    }
    return true;
}

bool writeLimits(db::Database& db, std::int64_t airplaneId, const AirplaneLimits& l, std::string& error) {
    db::Statement q(db,
                    "INSERT OR REPLACE INTO airplane_limits (airplane_id, min_runway_ft, mlw_lb, passenger_weight_lb,"
                    " bag_weight_lb, max_payload_lb) VALUES (?, ?, ?, ?, ?, ?)");
    q.bind(1, airplaneId);
    q.bind(2, static_cast<std::int64_t>(l.minRunwayFt));
    q.bind(3, static_cast<std::int64_t>(l.mlwLb));
    q.bind(4, static_cast<std::int64_t>(l.passengerWeightLb));
    q.bind(5, static_cast<std::int64_t>(l.bagWeightLb));
    q.bind(6, static_cast<std::int64_t>(l.maxPayloadLb));
    return run(q, error);
}

bool writeSimbriefValues(db::Database& db, std::int64_t airplaneId, const SimbriefReading& r, std::string_view readAt,
                         std::string& error) {
    db::Statement q(db,
                    "INSERT OR REPLACE INTO airplane_simbrief (airplane_id, read_at, base_type, max_passengers,"
                    " empty_weight_lb, mzfw_lb, mtow_lb, max_fuel_lb, max_cargo_lb) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    q.bind(1, airplaneId);
    q.bind(2, readAt);
    q.bind(3, r.baseType);
    q.bind(4, static_cast<std::int64_t>(r.maxPassengers.value_or(0)));
    q.bind(5, static_cast<std::int64_t>(r.emptyWeightLb.value_or(0)));
    q.bind(6, static_cast<std::int64_t>(r.mzfwLb.value_or(0)));
    q.bind(7, static_cast<std::int64_t>(r.mtowLb.value_or(0)));
    q.bind(8, static_cast<std::int64_t>(r.maxFuelLb.value_or(0)));
    q.bind(9, static_cast<std::int64_t>(r.maxCargoLb.value_or(0)));
    return run(q, error);
}

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
    while (n < kMaxSlots && hoursForSlot(n + 1) <= hoursFlown) ++n;
    return n;
}

double hoursForSlot(int n) { return n <= kStartingSlots ? 0.0 : 10.0 * (n - kStartingSlots); }

std::string leaseRefusal(db::Database& db, const LeaseRequest& request, const Rates& rates, std::optional<int> slots) {
    LeasePlan plan;
    return check(db, request, rates, slots, plan);
}

LeaseResult leaseAirplane(db::Database& db, const LeaseRequest& request, const Rates& rates, std::optional<int> slots) {
    db::Transaction tx(db);
    if (!tx.active()) return {std::nullopt, "Couldn't start a transaction: " + tx.error()};
    LeasePlan plan;
    if (std::string refusal = check(db, request, rates, slots, plan); !refusal.empty()) {
        return {std::nullopt, std::move(refusal)};
    }
    std::string error;
    const auto id = write(db, request, plan, error);
    if (!id || !tx.commit(&error)) return {std::nullopt, "Couldn't lease it: " + error};
    return {id, ""};
}

namespace {

// An airplane as a re-read or a return needs it.
struct Current {
    std::string registration, typeCode, simbriefId, tail, layoutName;
    bool returned = false, planned = false;
    Date leasedOn;
    Cents monthlyCents = 0;
    AirplaneLimits limits;
    std::optional<SimbriefReading> simbrief;  // the airplane_simbrief row, if it has one
    std::vector<Hold> holds;
    int seats = 0;
};

std::optional<Current> current(db::Database& db, std::int64_t id) {
    Current c;
    {
        db::Statement q(db,
                        "SELECT a.registration, t.code, a.simbrief_airframe_id, COALESCE(a.tail_number, ''),"
                        " a.airframe_name, a.returned_on IS NOT NULL, a.leased_on, a.monthly_lease_cents,"
                        " EXISTS (SELECT 1 FROM planned_flight p WHERE p.airplane_id = a.airplane_id),"
                        " (SELECT COALESCE(SUM(seats), 0) FROM airplane_cabin c WHERE c.airplane_id = a.airplane_id)"
                        " FROM airplane a JOIN aircraft_type t USING (aircraft_type_id) WHERE a.airplane_id = ?");
        q.bind(1, id);
        if (!q.next()) return std::nullopt;
        c.registration = q.text(0);
        c.typeCode = q.text(1);
        c.simbriefId = q.text(2);
        c.tail = q.text(3);
        c.layoutName = q.text(4);
        c.returned = q.integer(5) != 0;
        c.leasedOn = parseDate(q.text(6)).value_or(Date{});
        c.monthlyCents = q.integer(7);
        c.planned = q.integer(8) != 0;
        c.seats = static_cast<int>(q.integer(9));
    }
    c.limits = airplaneLimits(db, id).value_or(AirplaneLimits{});
    {
        db::Statement q(db, "SELECT base_type, max_passengers, empty_weight_lb, mzfw_lb, mtow_lb, max_fuel_lb,"
                            " max_cargo_lb FROM airplane_simbrief WHERE airplane_id = ?");
        q.bind(1, id);
        if (q.next()) {
            SimbriefReading s;
            s.baseType = q.text(0);
            s.maxPassengers = static_cast<int>(q.integer(1));
            s.emptyWeightLb = static_cast<int>(q.integer(2));
            s.mzfwLb = static_cast<int>(q.integer(3));
            s.mtowLb = static_cast<int>(q.integer(4));
            s.maxFuelLb = static_cast<int>(q.integer(5));
            s.maxCargoLb = static_cast<int>(q.integer(6));
            c.simbrief = s;
        }
    }
    {
        db::Statement q(db, "SELECT hold, max_lb, ratio FROM airplane_hold WHERE airplane_id = ? ORDER BY rowid");
        q.bind(1, id);
        while (q.next()) c.holds.push_back({q.text(0), static_cast<int>(q.integer(1)), static_cast<int>(q.integer(2))});
    }
    return c;
}

std::string pounds(std::optional<int> lb) { return lb ? formatNumber(*lb) + " lb" : "—"; }

std::string holdLabel(const std::string& hold) {
    if (hold == "main_deck") return "Main deck";
    std::string label = hold + " hold";
    label[0] = static_cast<char>(label[0] - 'a' + 'A');
    return label;
}

// The refusal and the changes, from the airplane as it is.
RereadPlan plan(db::Database& db, const Catalog& catalog, std::int64_t airplaneId, const Current& c,
                const SimbriefReading& r, const CatalogAirframe* layout) {
    RereadPlan out;
    const auto refuse = [&](std::string why) {
        out.refusal = std::move(why);
        return out;
    };
    if (c.returned) return refuse(c.registration + " has been returned.");
    if (c.planned) return refuse(c.registration + " has a flight planned. Settle or cancel it first.");
    if (const auto missing = missingFields(r); !missing.empty()) {
        std::string list;
        for (const std::string& m : missing) list += (list.empty() ? "" : ", ") + m;
        return refuse("Not read yet: " + list + ".");
    }
    if (const auto zero = zeroFields(r); !zero.empty()) return refuse("Must be more than 0: " + joined(zero) + ".");
    const CatalogType* type = catalog.type(c.typeCode);
    if (!type) return refuse(c.typeCode + " isn't in the aircraft files.");
    const auto& bases = type->simbriefBaseTypes;
    if (std::find(bases.begin(), bases.end(), r.baseType) == bases.end()) {
        return refuse(std::format("SimBrief's base type {} isn't a {}.", r.baseType, c.typeCode));
    }
    if (r.registration != c.registration) {
        return refuse(std::format("This screenshot is for {}, not {}. A new registration is a new lease.",
                                  r.registration, c.registration));
    }
    if (r.internalId != c.simbriefId) {
        db::Statement q(db, "SELECT registration FROM airplane WHERE simbrief_airframe_id = ? AND returned_on IS NULL"
                            " AND airplane_id <> ?");
        q.bind(1, r.internalId);
        q.bind(2, airplaneId);
        if (q.next()) return refuse(std::format("SimBrief airframe {} already belongs to {}.", r.internalId, q.text(0)));
    }
    const auto choices = candidateLayouts(catalog, r.baseType, *r.maxPassengers);
    const bool anyFits = std::any_of(choices.begin(), choices.end(), [&](const LayoutChoice& choice) {
        return choice.seatsMatch && choice.type->code == c.typeCode;
    });
    if (!anyFits) {
        return refuse(std::format("No {} layout has SimBrief's {} seats.", c.typeCode, *r.maxPassengers));
    }
    if (!layout || layout->typeCode != c.typeCode) return refuse("Pick a layout of the " + c.typeCode + ".");
    int seats = 0;
    for (const Cabin& cabin : layout->cabins) seats += cabin.seats;
    if (seats != *r.maxPassengers) {
        return refuse(std::format("SimBrief's Max Passengers is {}, but {} has {} seats.", *r.maxPassengers,
                                  layout->name, seats));
    }
    if (*r.mzfwLb <= *r.emptyWeightLb) return refuse("Max Zero Fuel Weight must be more than Empty Weight.");

    const auto change = [&](const std::string& label, const std::string& before, const std::string& after) {
        if (before != after) out.changes.push_back({label, before, after});
    };
    const auto& s = c.simbrief;
    change("Internal ID", c.simbriefId, r.internalId);
    change("Tail", c.tail.empty() ? "—" : c.tail, r.finNumber.empty() ? "—" : r.finNumber);
    change("Max passengers", std::to_string(c.seats), std::to_string(*r.maxPassengers));
    change("Passenger weight", pounds(c.limits.passengerWeightLb), pounds(r.passengerWeightLb));
    change("Bag weight", pounds(c.limits.bagWeightLb), pounds(r.bagWeightLb));
    change("Empty weight", pounds(s ? s->emptyWeightLb : std::nullopt), pounds(r.emptyWeightLb));
    change("MZFW", pounds(s ? s->mzfwLb : std::nullopt), pounds(r.mzfwLb));
    change("MTOW", pounds(s ? s->mtowLb : std::nullopt), pounds(r.mtowLb));
    change("MLW", pounds(c.limits.mlwLb), pounds(r.mlwLb));
    change("Max fuel", pounds(s ? s->maxFuelLb : std::nullopt), pounds(r.maxFuelLb));
    change("Max cargo", pounds(s ? s->maxCargoLb : std::nullopt), pounds(r.maxCargoLb));
    change("Min runway", formatNumber(c.limits.minRunwayFt) + " ft", formatNumber(*r.minRunwayFt) + " ft");
    change("Layout", c.layoutName, layout->name);
    const std::vector<Hold> holds = holdsForMaxCargo(layout->holds, *r.maxCargoLb);
    for (const Hold& h : holds) {
        const auto old = std::find_if(c.holds.begin(), c.holds.end(), [&](const Hold& o) { return o.hold == h.hold; });
        change(holdLabel(h.hold), old == c.holds.end() ? "—" : pounds(old->maxLb), pounds(h.maxLb));
    }
    return out;
}

}  // namespace

RereadPlan planReread(db::Database& db, const Catalog& catalog, std::int64_t airplaneId, const SimbriefReading& reading,
                      const CatalogAirframe* layout) {
    const auto c = current(db, airplaneId);
    if (!c) return {{}, "That airplane isn't in the database."};
    return plan(db, catalog, airplaneId, *c, reading, layout);
}

std::string applyReread(db::Database& db, const Catalog& catalog, std::int64_t airplaneId,
                        const SimbriefReading& r, const CatalogAirframe* layout, std::string_view readAt) {
    db::Transaction tx(db);
    if (!tx.active()) return "Couldn't start a transaction: " + tx.error();
    const auto c = current(db, airplaneId);
    if (!c) return "That airplane isn't in the database.";
    if (const RereadPlan p = plan(db, catalog, airplaneId, *c, r, layout); !p.refusal.empty()) return p.refusal;
    std::string error;
    const AirplaneLimits limits{*r.minRunwayFt, *r.mlwLb, *r.passengerWeightLb, *r.bagWeightLb,
                                *r.mzfwLb - *r.emptyWeightLb};
    db::Statement identity(db, "UPDATE airplane SET simbrief_airframe_id = ?, tail_number = ?, airframe_name = ?"
                               " WHERE airplane_id = ?");
    identity.bind(1, r.internalId);
    if (r.finNumber.empty()) {
        identity.bindNull(2);
    } else {
        identity.bind(2, r.finNumber);
    }
    identity.bind(3, layout->name);
    identity.bind(4, airplaneId);
    if (!run(identity, error) || !writeLimits(db, airplaneId, limits, error) ||
        !writeSimbriefValues(db, airplaneId, r, readAt, error) ||
        !writeHolds(db, airplaneId, holdsForMaxCargo(layout->holds, *r.maxCargoLb), error)) {
        return "Couldn't save it: " + error;
    }
    if (layout->name != c->layoutName) {
        db::Statement clear(db, "DELETE FROM airplane_cabin WHERE airplane_id = ?");
        clear.bind(1, airplaneId);
        if (!run(clear, error)) return "Couldn't save it: " + error;
        for (const Cabin& cabin : layout->cabins) {
            db::Statement insert(db, "INSERT INTO airplane_cabin (airplane_id, cabin_class, seats) VALUES (?, ?, ?)");
            insert.bind(1, airplaneId);
            insert.bind(2, cabin.cabinClass);
            insert.bind(3, static_cast<std::int64_t>(cabin.seats));
            if (!run(insert, error)) return "Couldn't save it: " + error;
        }
    }
    if (!tx.commit(&error)) return "Couldn't save it: " + error;
    return "";
}

Cents refundIfReturned(db::Database& db, std::int64_t airplaneId, Date today) {
    const auto c = current(db, airplaneId);
    if (!c || c->returned) return 0;
    // Only a month already charged is refunded (a month whose day has come but isn't charged yet costs nothing).
    int k = 0;
    while (leasePeriodStart(c->leasedOn, k + 1) <= today) ++k;
    db::Statement charged(db, "SELECT 1 FROM lease_charge WHERE airplane_id = ? AND period_start = ?");
    charged.bind(1, airplaneId);
    charged.bind(2, formatDate(leasePeriodStart(c->leasedOn, k)));
    return charged.next() ? leaseRefundCents(c->monthlyCents, c->leasedOn, today) : 0;
}

std::string returnAirplane(db::Database& db, std::int64_t airplaneId, Date today) {
    db::Transaction tx(db);
    if (!tx.active()) return "Couldn't start a transaction: " + tx.error();
    const auto c = current(db, airplaneId);
    if (!c) return "That airplane isn't in the database.";
    if (c->returned) return c->registration + " has been returned.";
    if (c->planned) return c->registration + " has a flight planned. Settle or cancel it first.";

    const Cents refund = refundIfReturned(db, airplaneId, today);

    std::string error;
    db::Statement ret(db, "UPDATE airplane SET returned_on = ?, return_reason = 'returned' WHERE airplane_id = ?");
    ret.bind(1, formatDate(today));
    ret.bind(2, airplaneId);
    if (!run(ret, error)) return "Couldn't return it: " + error;
    if (refund > 0) {
        db::Statement line(db, "INSERT INTO ledger_line (category, posted_at, amount_cents) VALUES ('lease_refund', ?, ?)");
        line.bind(1, formatInstant(today));
        line.bind(2, refund);
        db::Statement fact(db, "INSERT INTO lease_refund (ledger_line_id, airplane_id) VALUES (last_insert_rowid(), ?)");
        fact.bind(1, airplaneId);
        if (!run(line, error) || !run(fact, error)) return "Couldn't return it: " + error;
    }
    if (!tx.commit(&error)) return "Couldn't return it: " + error;
    return "";
}

LeaseOutcome settleLeases(db::Database& db, Date today) {
    LeaseOutcome outcome;
    // A read that fails stops the run (review 1.4): read as 0, it would return airplanes for good.
    const auto readFailed = [&outcome](const std::string& what) {
        outcome.error = "couldn't read " + what;
        return outcome;
    };
    EventQueue events;
    // Each airplane in the fleet: its next uncharged month. Months are charged in order, so it's the count of charged
    // months.
    {
        db::Statement q(db,
                        "SELECT a.airplane_id, a.registration, a.leased_on, a.monthly_lease_cents,"
                        " (SELECT COUNT(*) FROM lease_charge c WHERE c.airplane_id = a.airplane_id)"
                        " FROM airplane a WHERE a.returned_on IS NULL");
        while (q.next()) {
            const auto leasedOn = parseDate(q.text(2));
            if (!leasedOn) continue;
            LeaseEvent e;
            e.airplaneId = q.integer(0);
            e.registration = q.text(1);
            e.leasedOn = *leasedOn;
            e.monthlyCents = q.integer(3);
            e.period = static_cast<int>(q.integer(4));
            e.date = leasePeriodStart(e.leasedOn, e.period);
            if (e.date <= today) events.push(e);
        }
        if (!q.error().empty()) return readFailed("the airplanes: " + q.error());
    }

    while (!events.empty()) {
        LeaseEvent e = events.top();
        events.pop();
        const auto cashRead = cashCentsAsOf(db, formatInstant(e.date));
        if (!cashRead) return readFailed("the cash on " + formatDate(e.date));
        std::string error;
        if (*cashRead < e.monthlyCents) {
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
        }
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
        ++e.period;
        e.date = leasePeriodStart(e.leasedOn, e.period);
        if (e.date <= today) events.push(e);
    }
    return outcome;
}

}  // namespace blocktime
