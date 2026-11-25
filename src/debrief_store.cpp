// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debrief_store.h"

#include <optional>

namespace blocktime {

namespace {

void bindOptional(db::Statement& q, int index, const std::optional<int>& v) {
    if (v) {
        q.bind(index, static_cast<std::int64_t>(*v));
    } else {
        q.bindNull(index);
    }
}

std::optional<int> optionalInt(db::Statement& q, int col) {
    if (q.isNull(col)) return std::nullopt;
    return static_cast<int>(q.integer(col));
}

}  // namespace

bool writeMeasurements(db::Database& db, std::int64_t flightId, const FlightMeasurements& m, std::string* error) {
    const auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    db::Statement debrief(db, "INSERT INTO flight_debrief (flight_id, family) VALUES (?, ?)");
    debrief.bind(1, flightId);
    debrief.bind(2, std::string_view(m.family == Family::B777 ? "777" : "737"));
    if (!debrief.run()) return fail(debrief.error());

    db::Statement value(db, "INSERT INTO flight_check_value (flight_id, part, value) VALUES (?, ?, ?)");
    for (const CheckValue& v : m.values) {
        value.reset();
        value.bind(1, flightId);
        value.bind(2, v.part);
        value.bind(3, static_cast<std::int64_t>(v.value));
        if (!value.run()) return fail(v.part + ": " + value.error());
    }
    db::Statement skip(db, "INSERT INTO flight_check_skip (flight_id, part, reason) VALUES (?, ?, ?)");
    for (const CheckSkip& s : m.skipped) {
        skip.reset();
        skip.bind(1, flightId);
        skip.bind(2, s.part);
        skip.bind(3, s.reason);
        if (!skip.run()) return fail(s.part + ": " + skip.error());
    }
    if (m.landing) {
        const LandingFacts& f = *m.landing;
        db::Statement landing(db, "INSERT INTO flight_landing (flight_id, vs_fpm, g_hundredths, bounces, ias_kt,"
                                  " heading_true_deg, wind_dir_deg, wind_kt, pitch_tenths, bank_tenths, vref_kt,"
                                  " off_centreline_dm, off_heading_tenths) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        landing.bind(1, flightId);
        int i = 2;
        for (int v : {f.vsFpm, f.gHundredths, f.bounces, f.iasKt, (f.headingTrueDeg % 360 + 360) % 360,
                      (f.windDirDeg % 360 + 360) % 360, f.windKt}) {
            landing.bind(i++, static_cast<std::int64_t>(v));
        }
        for (const auto& v : {f.pitchTenths, f.bankTenths, f.vrefKt, f.offCentrelineDm, f.offHeadingTenths}) {
            bindOptional(landing, i++, v);
        }
        if (!landing.run()) return fail("landing: " + landing.error());
    }
    return true;
}

std::map<std::string, FlightMeasurements> loadMeasurements(db::Database& db) {
    std::map<std::string, FlightMeasurements> out;
    db::Statement debriefs(db, "SELECT f.log_flight_id, d.family FROM flight_debrief d JOIN flight f USING (flight_id)");
    while (debriefs.next()) out[debriefs.text(0)].family = debriefs.text(1) == "777" ? Family::B777 : Family::B737;
    db::Statement values(db, "SELECT f.log_flight_id, v.part, v.value FROM flight_check_value v"
                             " JOIN flight f USING (flight_id)");
    while (values.next()) out[values.text(0)].values.push_back({values.text(1), static_cast<int>(values.integer(2))});
    db::Statement skips(db, "SELECT f.log_flight_id, s.part, s.reason FROM flight_check_skip s"
                            " JOIN flight f USING (flight_id)");
    while (skips.next()) out[skips.text(0)].skipped.push_back({skips.text(1), skips.text(2)});
    db::Statement landings(db, "SELECT f.log_flight_id, vs_fpm, g_hundredths, bounces, ias_kt, heading_true_deg,"
                               " wind_dir_deg, wind_kt, pitch_tenths, bank_tenths, vref_kt, off_centreline_dm,"
                               " off_heading_tenths FROM flight_landing l JOIN flight f USING (flight_id)");
    while (landings.next()) {
        LandingFacts f;
        f.vsFpm = static_cast<int>(landings.integer(1));
        f.gHundredths = static_cast<int>(landings.integer(2));
        f.bounces = static_cast<int>(landings.integer(3));
        f.iasKt = static_cast<int>(landings.integer(4));
        f.headingTrueDeg = static_cast<int>(landings.integer(5));
        f.windDirDeg = static_cast<int>(landings.integer(6));
        f.windKt = static_cast<int>(landings.integer(7));
        f.pitchTenths = optionalInt(landings, 8);
        f.bankTenths = optionalInt(landings, 9);
        f.vrefKt = optionalInt(landings, 10);
        f.offCentrelineDm = optionalInt(landings, 11);
        f.offHeadingTenths = optionalInt(landings, 12);
        out[landings.text(0)].landing = f;
    }
    return out;
}

bool isMeasured(db::Database& db, std::string_view logFlightId) {
    db::Statement q(db, "SELECT 1 FROM flight_debrief JOIN flight USING (flight_id) WHERE log_flight_id = ?");
    q.bind(1, logFlightId);
    return q.next();
}

bool hasReview(db::Database& db, std::string_view logFlightId) {
    db::Statement q(db, "SELECT 1 FROM flight_review JOIN flight USING (flight_id) WHERE log_flight_id = ?");
    q.bind(1, logFlightId);
    return q.next();
}

std::map<std::string, StoredReview> loadReviews(db::Database& db) {
    std::map<std::string, StoredReview> out;
    db::Statement reviews(db, "SELECT f.log_flight_id, summary, landing, atc, model, written_at FROM flight_review r"
                              " JOIN flight f USING (flight_id)");
    while (reviews.next()) {
        out[reviews.text(0)] = {reviews.text(1), reviews.text(2), reviews.text(3), reviews.text(4), reviews.text(5), {}};
    }
    db::Statement items(db, "SELECT f.log_flight_id, i.text FROM flight_review_item i JOIN flight f USING (flight_id)"
                            " ORDER BY i.flight_id, i.seq");
    while (items.next()) out[items.text(0)].workOn.push_back(items.text(1));
    return out;
}

bool storeReview(db::Database& db, std::string_view logFlightId, const StoredReview& review, std::string* error) {
    const auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    db::Statement find(db, "SELECT d.flight_id FROM flight_debrief d JOIN flight f USING (flight_id)"
                           " WHERE f.log_flight_id = ?");
    find.bind(1, logFlightId);
    if (!find.next()) return fail("the flight isn't measured");
    const std::int64_t flightId = find.integer(0);

    db::Transaction tx(db);
    if (!tx.active()) return fail("couldn't start a transaction: " + tx.error());
    db::Statement row(db, "INSERT INTO flight_review (flight_id, summary, landing, atc, model, written_at)"
                          " VALUES (?, ?, ?, ?, ?, ?)");
    row.bind(1, flightId);
    row.bind(2, review.summary);
    row.bind(3, review.landing);
    row.bind(4, review.atc);
    row.bind(5, review.model);
    row.bind(6, review.writtenAt);
    if (!row.run()) return fail(row.error());
    db::Statement item(db, "INSERT INTO flight_review_item (flight_id, seq, text) VALUES (?, ?, ?)");
    for (std::size_t i = 0; i < review.workOn.size(); ++i) {
        item.reset();
        item.bind(1, flightId);
        item.bind(2, static_cast<std::int64_t>(i));
        item.bind(3, review.workOn[i]);
        if (!item.run()) return fail(item.error());
    }
    std::string message;
    if (!tx.commit(&message)) return fail(message);
    if (error) error->clear();
    return true;
}

}  // namespace blocktime
