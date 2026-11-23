// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airport_import.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <map>
#include <optional>
#include <stdexcept>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "csv.h"

namespace blocktime {

namespace {

using Header = std::unordered_map<std::string, std::size_t>;

Header readHeader(CsvReader& csv) {
    Header header;
    std::vector<std::string> names;
    if (csv.next(names)) {
        for (std::size_t i = 0; i < names.size(); ++i) header[names[i]] = i;
    }
    return header;
}

// The columns' positions, or an error naming the first missing one.
std::optional<std::vector<std::size_t>> findColumns(const Header& header, std::initializer_list<const char*> names,
                                                    std::string_view file, std::string& error) {
    std::vector<std::size_t> out;
    for (const char* name : names) {
        const auto it = header.find(name);
        if (it == header.end()) {
            error = std::string("missing column ") + name + " in " + std::string(file);
            return std::nullopt;
        }
        out.push_back(it->second);
    }
    return out;
}

const std::string& field(const std::vector<std::string>& row, std::size_t col) {
    static const std::string empty;
    return col < row.size() ? row[col] : empty;
}

template <typename T>
std::optional<T> number(const std::string& text) {
    T value{};
    const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc() || end != text.data() + text.size() || text.empty()) return std::nullopt;
    return value;
}

bool isIcao(const std::string& code) {
    return code.size() == 4 &&
           std::all_of(code.begin(), code.end(), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
}

bool knownZone(const std::string& name) {
    try {
        (void)std::chrono::locate_zone(name);
        return true;
    } catch (const std::runtime_error&) {
        return false;
    }
}

}  // namespace

bool isPavedSurface(std::string_view surface) {
    std::string upper(surface);
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](char c) { return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c; });
    for (const char* word : {"ASP", "CON", "PEM", "BIT", "TAR", "PAV"}) {
        if (upper.find(word) != std::string::npos) return true;
    }
    return false;
}

AirportImport filterAirports(std::istream& airportsCsv, std::istream& runwaysCsv, std::istream& airportsJson) {
    AirportImport result;
    std::vector<std::string> row;

    // Longest open, paved runway per OurAirports airport id.
    CsvReader runways(runwaysCsv);
    const Header runwayHeader = readHeader(runways);
    const auto rc = findColumns(runwayHeader, {"airport_ref", "length_ft", "surface", "closed"}, "runways.csv", result.error);
    if (!rc) return result;
    std::unordered_map<std::string, int> longest;
    while (runways.next(row)) {
        if (field(row, (*rc)[3]) == "1" || !isPavedSurface(field(row, (*rc)[2]))) continue;
        const auto length = number<int>(field(row, (*rc)[1]));
        if (!length) continue;
        int& best = longest[field(row, (*rc)[0])];
        best = std::max(best, *length);
    }

    // Time zones by ICAO code.
    const nlohmann::json zones = nlohmann::json::parse(airportsJson, nullptr, false);
    if (zones.is_discarded() || !zones.is_object()) {
        result.error = "airports.json isn't a JSON object of airports";
        return result;
    }

    CsvReader airports(airportsCsv);
    const Header airportHeader = readHeader(airports);
    const auto ac = findColumns(airportHeader,
                                {"id", "type", "name", "latitude_deg", "longitude_deg", "elevation_ft", "iso_country",
                                 "gps_code"},
                                "airports.csv", result.error);
    if (!ac) return result;
    const auto icaoColumn = airportHeader.find("icao_code");  // newer files only

    while (airports.next(row)) {
        ++result.stats.read;
        const std::string& type = field(row, (*ac)[1]);
        if (type != "large_airport" && type != "medium_airport") {
            ++result.stats.notLargeOrMedium;
            continue;
        }
        std::string icao = icaoColumn != airportHeader.end() ? field(row, icaoColumn->second) : std::string();
        if (icao.empty()) icao = field(row, (*ac)[7]);
        if (!isIcao(icao)) {
            ++result.stats.noIcao;
            continue;
        }
        const auto runway = longest.find(field(row, (*ac)[0]));
        if (runway == longest.end() || runway->second < kMinRunwayFt) {
            ++result.stats.noRunway;
            continue;
        }
        const auto zone = zones.find(icao);
        std::string tz;
        if (zone != zones.end() && zone->is_object() && zone->contains("tz") && (*zone)["tz"].is_string()) {
            tz = (*zone)["tz"].get<std::string>();
        }
        if (tz.empty() || !knownZone(tz)) {
            ++result.stats.noTimeZone;
            continue;
        }
        const auto latitude = number<double>(field(row, (*ac)[3]));
        const auto longitude = number<double>(field(row, (*ac)[4]));
        const auto elevation = number<int>(field(row, (*ac)[5]));
        const std::string& country = field(row, (*ac)[6]);
        if (!latitude || !longitude || !elevation || country.size() != 2 || field(row, (*ac)[2]).empty()) {
            ++result.stats.missingField;
            continue;
        }
        result.airports.push_back(ImportedAirport{icao, field(row, (*ac)[2]), country, tz, *latitude, *longitude,
                                                  *elevation, runway->second});
    }
    // An ICAO code can appear twice in OurAirports; keep the one with the longest runway.
    std::map<std::string, ImportedAirport> unique;
    for (ImportedAirport& a : result.airports) {
        auto [it, inserted] = unique.try_emplace(a.icao, a);
        if (!inserted && a.longestRunwayFt > it->second.longestRunwayFt) it->second = a;
    }
    result.airports.clear();
    for (auto& [icao, a] : unique) result.airports.push_back(std::move(a));
    result.stats.kept = static_cast<int>(result.airports.size());
    return result;
}

bool writeAirports(db::Database& db, const std::vector<ImportedAirport>& airports, std::string_view importedAt,
                   std::string* error) {
    db::Transaction tx(db);
    if (!tx.active()) {
        if (error) *error = "couldn't start a transaction";
        return false;
    }
    db::Statement upsert(db,
                         "INSERT INTO airport (icao_code, name, latitude, longitude, elevation_ft, country_code,"
                         " time_zone, longest_runway_ft) VALUES (?, ?, ?, ?, ?, ?, ?, ?)"
                         " ON CONFLICT (icao_code) DO UPDATE SET name = excluded.name, latitude = excluded.latitude,"
                         " longitude = excluded.longitude, elevation_ft = excluded.elevation_ft,"
                         " country_code = excluded.country_code, time_zone = excluded.time_zone,"
                         " longest_runway_ft = excluded.longest_runway_ft");
    for (const ImportedAirport& a : airports) {
        upsert.reset();
        upsert.bind(1, a.icao);
        upsert.bind(2, a.name);
        upsert.bind(3, a.latitude);
        upsert.bind(4, a.longitude);
        upsert.bind(5, static_cast<std::int64_t>(a.elevationFt));
        upsert.bind(6, a.country);
        upsert.bind(7, a.timeZone);
        upsert.bind(8, static_cast<std::int64_t>(a.longestRunwayFt));
        if (!upsert.run()) {
            if (error) *error = a.icao + ": " + upsert.error();
            return false;
        }
    }
    db::Statement log(db, "INSERT INTO airport_import (imported_at, airport_count) VALUES (?, ?)");
    log.bind(1, importedAt);
    log.bind(2, static_cast<std::int64_t>(airports.size()));
    if (!log.run()) {
        if (error) *error = log.error();
        return false;
    }
    return tx.commit(error);
}

}  // namespace blocktime
