// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The aircraft catalog (design.md §3, D-091, D-243): types (reference data, D-093) and their layouts ("airframes",
// templates copied onto an airplane when it's leased, D-082), read from one file per type in data/aircraft/, shipped
// with the app.

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "money.h"

namespace blocktime {

struct CatalogType {
    std::string code, family, role;  // family "737" | "777"; role "airliner" | "executive" | "freighter"
    int turnaroundMin, cruiseSpeedKt;
    Cents monthlyLeaseCents;
    std::string simFolder;  // the sim's aircraft.cfg folder for this type, "PMDG 737-800BBJ2" (D-112)
    double wingspanM = 0;   // for the arrival gate's size (D-147)
    std::vector<std::string> simbriefBaseTypes;  // SimBrief's Base Type values this type accepts (D-242)
    std::string factoryIcao;                     // where a factory delivery starts (design.md §6)
    int deliveryFeePerNmDollars = 0;             // instant delivery's stand-in for fuel (D-245)
};

struct Cabin {
    std::string cabinClass;  // "economy" | "business" | "first"
    int seats;
};

// On an airplane, maxLb is the hold's maximum. In a layout it's the hold's share of SimBrief's Max Cargo, which the
// airplane's maximums are split by (D-242); ratio splits a flight's load between the holds either way.
struct Hold {
    std::string hold;  // "forward" | "aft" | "bulk" | "main_deck"
    int maxLb, ratio;
};

// What New Flight needs before a SimBrief plan exists, read from the airplane's SimBrief airframe (D-242).
struct AirplaneLimits {
    int minRunwayFt = 0, mlwLb = 0, passengerWeightLb = 0, bagWeightLb = 0;
    int maxPayloadLb = 0;  // MZFW - empty weight
};

// A layout: a type in one configuration ("737-800 Two-class").
struct CatalogAirframe {
    std::string name, typeCode;
    std::vector<Cabin> cabins;  // empty for freighters
    std::vector<Hold> holds;    // maxLb is the share (above)
};

struct Catalog {
    std::vector<CatalogType> types;
    std::vector<CatalogAirframe> airframes;

    const CatalogType* type(std::string_view code) const;
    const CatalogAirframe* airframe(std::string_view name) const;
};

// Parses and validates one aircraft file: one type and its layouts. On failure returns nullopt with
// "<fileName> line N: ..." in *error.
std::optional<Catalog> parseAircraftFile(std::string_view yaml, std::string_view fileName, std::string* error);
// Every *.yaml in `dir`, in file-name order. A file that doesn't parse, or repeats a type or layout, is skipped with
// a notice.
Catalog loadCatalogDir(const std::filesystem::path& dir, std::vector<std::string>* notices);

std::filesystem::path defaultCatalogDir();  // data/aircraft next to the executable

}  // namespace blocktime
