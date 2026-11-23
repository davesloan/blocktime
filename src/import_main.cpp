// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// blocktime-import: imports airport data into the airline database (D-079, D-084, D-094).
//
//   blocktime-import <folder> [--db <file>]
//
// <folder> holds airports.csv and runways.csv from OurAirports (public domain) and airports.json from mwgg/Airports
// (MIT, Copyright (c) 2014 mwgg). See docs/notes/airport-data.md for where to download them.

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <string>

#include "airline_db.h"
#include "airport_import.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "usage: blocktime-import <folder> [--db <file>]\n";
        return 1;
    }
    const fs::path folder = fs::path(argv[1]);
    fs::path dbFile = blocktime::defaultAirlineDatabaseFile();
    for (int i = 2; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--db") dbFile = fs::path(argv[++i]);
    }

    std::ifstream airports(folder / "airports.csv", std::ios::binary);
    std::ifstream runways(folder / "runways.csv", std::ios::binary);
    std::ifstream zones(folder / "airports.json", std::ios::binary);
    for (const auto& [stream, name] : {std::pair{&airports, "airports.csv"}, std::pair{&runways, "runways.csv"},
                                       std::pair{&zones, "airports.json"}}) {
        if (!*stream) {
            std::cerr << "Can't open " << (folder / name).string() << "\n";
            return 1;
        }
    }

    const blocktime::AirportImport result = blocktime::filterAirports(airports, runways, zones);
    if (!result.error.empty()) {
        std::cerr << result.error << "\n";
        return 1;
    }
    const auto& s = result.stats;
    std::cout << std::format("Read {} airports, kept {}.\n", s.read, s.kept)
              << std::format("Left out: {} not large or medium, {} without an ICAO code, {} without an open paved "
                             "runway of {} ft, {} without a time zone, {} missing a field.\n",
                             s.notLargeOrMedium, s.noIcao, s.noRunway, blocktime::kMinRunwayFt, s.noTimeZone,
                             s.missingField);

    blocktime::AirlineDbOpen opened = blocktime::openAirlineDatabase(dbFile);
    if (!opened.db) {
        std::cerr << "Can't open " << dbFile.string() << ": " << opened.error << "\n";
        return 1;
    }
    if (opened.testSchemaSetAside) std::cout << "The Qt test's database was set aside as blocktime.test-schema.db.\n";

    const std::string now = std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
    std::string error;
    if (!blocktime::writeAirports(*opened.db, result.airports, now, &error)) {
        std::cerr << "Writing the airports failed: " << error << "\n";
        return 1;
    }
    std::cout << std::format("Wrote {} airports to {}.\n", result.airports.size(), dbFile.string());
    return 0;
}
