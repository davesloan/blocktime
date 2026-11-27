// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "taxi_history.h"

#include <algorithm>
#include <string>

namespace blocktime {

namespace {

// The last 5 taxi times (seconds) through the airport, newest first. `taxi` is the SQL for one flight's taxi time
// (NULL when its phase is missing), `airportColumn` the flight's column the airport is matched on.
std::vector<int> lastTaxiTimes(db::Database& db, const std::string& taxi, const char* airportColumn,
                               std::string_view icao, std::string_view beforeInstant) {
    db::Statement q(db, "SELECT s FROM (SELECT f.block_in_at AS block_in, " + taxi +
                            " AS s FROM flight f JOIN airport a ON a.airport_id = f." + airportColumn +
                            " WHERE a.icao_code = ?1 AND (?2 = '' OR f.block_in_at < ?2))"
                            " WHERE s IS NOT NULL AND s >= 0 ORDER BY block_in DESC LIMIT 5");
    q.bind(1, icao);
    q.bind(2, beforeInstant);
    std::vector<int> out;
    while (q.next()) out.push_back(static_cast<int>(q.integer(0)));
    return out;
}

std::optional<int> median(std::vector<int> values) {
    if (values.empty()) return std::nullopt;
    return medianOf(std::move(values));
}

}  // namespace

TaxiHistory taxiHistory(db::Database& db, std::string_view originIcao, std::string_view arrivalIcao,
                        std::string_view beforeInstant) {
    const std::string out =
        "strftime('%s', (SELECT started_at FROM flight_phase p WHERE p.flight_id = f.flight_id AND p.phase = 'takeoff'"
        " ORDER BY p.seq LIMIT 1)) - strftime('%s', f.block_out_at)";
    const std::string in =
        "strftime('%s', f.block_in_at) - strftime('%s', (SELECT started_at FROM flight_phase p"
        " WHERE p.flight_id = f.flight_id AND p.phase = 'landing' ORDER BY p.seq DESC LIMIT 1))";
    return {median(lastTaxiTimes(db, out, "origin_airport_id", originIcao, beforeInstant)),
            median(lastTaxiTimes(db, in, "arrival_airport_id", arrivalIcao, beforeInstant))};
}

int medianOf(std::vector<int> values) {
    std::sort(values.begin(), values.end());
    const std::size_t n = values.size();
    if (n % 2 == 1) return values[n / 2];
    return (values[n / 2 - 1] + values[n / 2]) / 2;
}

}  // namespace blocktime
