// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The aircraft catalog (design.md §3, D-091): types (reference data, D-093) and airframes (templates copied onto an
// airplane when it's leased, D-082), read from catalog.yaml.

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
};

struct Cabin {
    std::string cabinClass;  // "economy" | "business" | "first"
    int seats;
};

struct Hold {
    std::string hold;  // "forward" | "aft" | "bulk" | "main_deck"
    int maxLb, ratio;
};

// What New Flight needs before a SimBrief plan exists (D-100). They must match the airplane's SimBrief airframe.
struct AirplaneLimits {
    int minRunwayFt = 0, mlwLb = 0, passengerWeightLb = 0, bagWeightLb = 0;
    int maxPayloadLb = 0;  // MZFW - empty weight
};

struct CatalogAirframe {
    std::string name, typeCode;
    std::vector<Cabin> cabins;  // empty for freighters
    std::vector<Hold> holds;
    AirplaneLimits limits;      // defaults for every airplane leased from it (D-100)
};

struct Catalog {
    std::vector<CatalogType> types;
    std::vector<CatalogAirframe> airframes;

    const CatalogType* type(std::string_view code) const;
    const CatalogAirframe* airframe(std::string_view name) const;
};

// Parses and validates catalog.yaml's text. On failure returns nullopt with "catalog.yaml line N: ..." in *error.
std::optional<Catalog> parseCatalog(std::string_view yaml, std::string* error);
std::optional<Catalog> loadCatalog(const std::filesystem::path& file, std::string* error);

std::filesystem::path defaultCatalogFile();  // %LOCALAPPDATA%\BlockTime\catalog.yaml

}  // namespace blocktime
