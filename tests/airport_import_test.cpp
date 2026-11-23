// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "airline_db.h"
#include "airport_import.h"
#include "sqlite_db.h"

using namespace blocktime;

namespace {

const std::string kAirportsHeader =
    "\"id\",\"ident\",\"type\",\"name\",\"latitude_deg\",\"longitude_deg\",\"elevation_ft\",\"continent\","
    "\"iso_country\",\"iso_region\",\"municipality\",\"scheduled_service\",\"icao_code\",\"iata_code\",\"gps_code\","
    "\"local_code\",\"home_link\",\"wikipedia_link\",\"keywords\"\n";
const std::string kRunwaysHeader =
    "\"id\",\"airport_ref\",\"airport_ident\",\"length_ft\",\"width_ft\",\"surface\",\"lighted\",\"closed\","
    "\"le_ident\",\"le_latitude_deg\",\"le_longitude_deg\",\"le_elevation_ft\",\"le_heading_degT\","
    "\"le_displaced_threshold_ft\",\"he_ident\",\"he_latitude_deg\",\"he_longitude_deg\",\"he_elevation_ft\","
    "\"he_heading_degT\",\"he_displaced_threshold_ft\"\n";

// One airports.csv row: id, type, name, icao_code, gps_code (the rest filled in).
std::string airport(int id, const std::string& type, const std::string& name, const std::string& icao,
                    const std::string& gps) {
    return std::to_string(id) + ",\"" + gps + "\",\"" + type + "\",\"" + name +
           "\",32.73,-117.19,17,\"NA\",\"US\",\"US-CA\",\"Somewhere\",\"yes\",\"" + icao + "\",\"\",\"" + gps +
           "\",\"\",\"\",\"\",\"\"\n";
}

// One runways.csv row for airport `ref`.
std::string runway(int ref, const std::string& length, const std::string& surface, int closed) {
    return "1," + std::to_string(ref) + ",\"X\"," + length + ",150,\"" + surface + "\",1," + std::to_string(closed) +
           ",\"09\",,,,,,\"27\",,,,,\n";
}

AirportImport run(const std::string& airportsCsv, const std::string& runwaysCsv, const std::string& json) {
    std::istringstream a(airportsCsv), r(runwaysCsv), j(json);
    return filterAirports(a, r, j);
}

const ImportedAirport* find(const AirportImport& result, const std::string& icao) {
    const auto it = std::find_if(result.airports.begin(), result.airports.end(),
                                 [&](const ImportedAirport& a) { return a.icao == icao; });
    return it == result.airports.end() ? nullptr : &*it;
}

}  // namespace

TEST(AirportImport, KeepsLargeAndMediumWithLongPavedRunwayAndZone) {
    const AirportImport result =
        run(kAirportsHeader + airport(1, "large_airport", "San Diego", "KSAN", "KSAN"),
            kRunwaysHeader + runway(1, "9401", "ASP", 0) + runway(1, "5000", "CON", 0),
            R"({"KSAN": {"icao": "KSAN", "tz": "America/Los_Angeles"}})");
    ASSERT_EQ(result.error, "");
    const ImportedAirport* ksan = find(result, "KSAN");
    ASSERT_NE(ksan, nullptr);
    EXPECT_EQ(ksan->longestRunwayFt, 9401);
    EXPECT_EQ(ksan->timeZone, "America/Los_Angeles");
    EXPECT_EQ(ksan->name, "San Diego");
    EXPECT_EQ(ksan->country, "US");
    EXPECT_EQ(ksan->elevationFt, 17);
    EXPECT_DOUBLE_EQ(ksan->latitude, 32.73);
    EXPECT_EQ(result.stats.kept, 1);
    EXPECT_EQ(result.stats.read, 1);
}

TEST(AirportImport, DropsShortUnpavedClosedAndSmall) {
    const std::string airports = kAirportsHeader + airport(1, "medium_airport", "Short", "KAAA", "KAAA") +
                                 airport(2, "medium_airport", "Grass", "KBBB", "KBBB") +
                                 airport(3, "medium_airport", "Closed", "KCCC", "KCCC") +
                                 airport(4, "small_airport", "Small", "KDDD", "KDDD") +
                                 airport(5, "medium_airport", "No ICAO", "", "") +
                                 airport(6, "medium_airport", "No zone", "KFFF", "KFFF");
    const std::string runways = kRunwaysHeader + runway(1, "3999", "ASP", 0) + runway(2, "6000", "GRASS", 0) +
                                runway(3, "8000", "ASP", 1) + runway(4, "8000", "ASP", 0) +
                                runway(5, "8000", "ASP", 0) + runway(6, "8000", "ASP", 0);
    const std::string json = R"({"KAAA": {"tz": "America/Chicago"}, "KBBB": {"tz": "America/Chicago"},
                                 "KCCC": {"tz": "America/Chicago"}, "KDDD": {"tz": "America/Chicago"}})";
    const AirportImport result = run(airports, runways, json);
    ASSERT_EQ(result.error, "");
    EXPECT_TRUE(result.airports.empty());
    EXPECT_EQ(result.stats.read, 6);
    EXPECT_EQ(result.stats.noRunway, 3);
    EXPECT_EQ(result.stats.notLargeOrMedium, 1);
    EXPECT_EQ(result.stats.noIcao, 1);
    EXPECT_EQ(result.stats.noTimeZone, 1);
}

TEST(AirportImport, PrefersIcaoCodeColumnOverGpsCode) {
    const AirportImport result = run(kAirportsHeader + airport(1, "large_airport", "Two codes", "EGLL", "XXXX"),
                                     kRunwaysHeader + runway(1, "12000", "ASP", 0),
                                     R"({"EGLL": {"tz": "Europe/London"}, "XXXX": {"tz": "Europe/London"}})");
    ASSERT_EQ(result.airports.size(), 1u);
    EXPECT_EQ(result.airports[0].icao, "EGLL");
}

TEST(AirportImport, FallsBackToGpsCodeWithoutIcaoCodeColumn) {
    const std::string header =
        "\"id\",\"ident\",\"type\",\"name\",\"latitude_deg\",\"longitude_deg\",\"elevation_ft\",\"continent\","
        "\"iso_country\",\"iso_region\",\"municipality\",\"scheduled_service\",\"gps_code\"\n";
    const AirportImport result =
        run(header + "1,\"KSAN\",\"large_airport\",\"San Diego\",32.73,-117.19,17,\"NA\",\"US\",\"US-CA\",\"SD\",\"yes\",\"KSAN\"\n",
            kRunwaysHeader + runway(1, "9401", "ASP", 0), R"({"KSAN": {"tz": "America/Los_Angeles"}})");
    ASSERT_EQ(result.error, "");
    ASSERT_EQ(result.airports.size(), 1u);
    EXPECT_EQ(result.airports[0].icao, "KSAN");
}

TEST(AirportImport, UnknownTimeZoneNameIsDropped) {
    const AirportImport result = run(kAirportsHeader + airport(1, "large_airport", "Mars", "KMRS", "KMRS"),
                                     kRunwaysHeader + runway(1, "9000", "ASP", 0), R"({"KMRS": {"tz": "Mars/Olympus"}})");
    EXPECT_TRUE(result.airports.empty());
    EXPECT_EQ(result.stats.noTimeZone, 1);
}

TEST(AirportImport, MissingColumnIsAnError) {
    std::string header = kAirportsHeader;
    header.replace(header.find("\"latitude_deg\","), std::string("\"latitude_deg\",").size(), "");
    const AirportImport result = run(header, kRunwaysHeader, "{}");
    EXPECT_NE(result.error.find("missing column latitude_deg"), std::string::npos) << result.error;
    EXPECT_TRUE(result.airports.empty());
}

TEST(AirportImport, BadJsonIsAnError) {
    const AirportImport result = run(kAirportsHeader, kRunwaysHeader, "not json");
    EXPECT_NE(result.error.find("airports.json"), std::string::npos) << result.error;
}

TEST(AirportImport, WriteUpsertsAndLogsTheImport) {
    db::Database d = db::Database::openInMemory();
    std::string error;
    ASSERT_TRUE(migrateAirlineDatabase(d, &error)) << error;
    ImportedAirport ksan{"KSAN", "San Diego", "US", "America/Los_Angeles", 32.73, -117.19, 17, 9401};
    ASSERT_TRUE(writeAirports(d, {ksan}, "2026-11-22T10:00:00Z", &error)) << error;
    ksan.name = "San Diego International";
    ASSERT_TRUE(writeAirports(d, {ksan}, "2026-11-23T10:00:00Z", &error)) << error;

    db::Statement airports(d, "SELECT COUNT(*), MAX(name) FROM airport");
    ASSERT_TRUE(airports.next());
    EXPECT_EQ(airports.integer(0), 1);
    EXPECT_EQ(airports.text(1), "San Diego International");
    db::Statement imports(d, "SELECT COUNT(*), MAX(airport_count) FROM airport_import");
    ASSERT_TRUE(imports.next());
    EXPECT_EQ(imports.integer(0), 2);
    EXPECT_EQ(imports.integer(1), 1);
}

TEST(AirportImport, PavedSurfaces) {
    for (const char* s : {"ASP", "asphalt", "CONC", "PEM", "BIT", "Asphalt/Concrete"}) EXPECT_TRUE(isPavedSurface(s)) << s;
    for (const char* s : {"GRS", "TURF", "GRAVEL", "WATER", ""}) EXPECT_FALSE(isPavedSurface(s)) << s;
}
