// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airport import (D-079, D-084): OurAirports' airports.csv and runways.csv, joined on the ICAO code with the
// time zones in mwgg/Airports' airports.json (MIT), filtered to airports the fleet could use, and written to the
// airline database. Run by the blocktime-import tool (D-094).

#pragma once

#include <istream>
#include <string>
#include <string_view>
#include <vector>

#include "sqlite_db.h"

namespace blocktime {

inline constexpr int kMinRunwayFt = 4000;

struct ImportedAirport {
    std::string icao, name, country, timeZone;
    double latitude, longitude;
    int elevationFt, longestRunwayFt;
};

// Why airports were left out. Checked in this order, so each airport counts once.
struct AirportImportStats {
    int read = 0, kept = 0, notLargeOrMedium = 0, noIcao = 0, noRunway = 0, noTimeZone = 0, missingField = 0;
};

struct AirportImport {
    std::vector<ImportedAirport> airports;
    AirportImportStats stats;
    std::string error;  // "" on success; otherwise `airports` is empty
};

// Keeps large and medium airports with a 4-character ICAO code (icao_code, else gps_code), an open paved runway of
// at least kMinRunwayFt, and a time zone known to the C++ library. Columns are found by header name.
AirportImport filterAirports(std::istream& airportsCsv, std::istream& runwaysCsv, std::istream& airportsJson);

// Upserts by ICAO code and logs the import, in one transaction. Airports are never deleted (airplanes point at them).
bool writeAirports(db::Database& db, const std::vector<ImportedAirport>& airports, std::string_view importedAt,
                   std::string* error);

// After upper-casing: contains ASP, CON, PEM, BIT, TAR or PAV.
bool isPavedSurface(std::string_view surface);

}  // namespace blocktime
