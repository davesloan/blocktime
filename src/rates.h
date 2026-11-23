// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The fare and cost rates of design.md §10-12, from %LOCALAPPDATA%\BlockTime\rates.yaml (D-107), so Dave can tune
// them without rebuilding. Read whenever New Flight opens and when a flight is settled. The defaults are the design's
// numbers and equal config/rates.yaml.

#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>

#include "money.h"

namespace blocktime {

// The rates that differ between narrowbodies (737) and widebodies (777), in dollars.
struct FamilyRates {
    double gate = 0, handling = 0, atcPerNm = 0, pilotPerHour = 0, maintenancePerHour = 0;
    bool operator==(const FamilyRates&) const = default;
};

// Dollars, except the fuel price, which is locked on each planned flight in cents.
struct Rates {
    Cents fuelPer100Lb = 4100;
    double landingPer1000LbMlw = 4.50;
    double cargoHandlingPer1000Lb = 40;
    double flightAttendantPerHour = 55;
    FamilyRates narrowbody{300, 900, 1.00, 200, 300};
    FamilyRates widebody{900, 2800, 2.00, 300, 700};
    std::array<double, 3> catering{5, 15, 35};  // per passenger: under 3 h, 3-8 h, 8 h and over
    double economyBase = 90, economyPerNm = 0.075, businessTimesEconomy = 3, firstTimesEconomy = 5;
    double vipFareBase = 180, vipFarePerNm = 0.48;  // per seat sold
    double vipPilotPerHour = 250, vipFlightAttendantPerHour = 75, vipFlightAttendantsTimes = 3, vipCateringTimes = 2;
    double charterPerHour = 7500, charterMinHours = 2, gaServices = 1500;
    double cargoBasePer100Lb = 15, cargoPer100LbPerNm = 0.028;

    bool operator==(const Rates&) const = default;
};

// nullopt with "rates.yaml line <n>: missing <key>" (or "can't read <path>") in `error`. Every key is required.
std::optional<Rates> loadRates(const std::filesystem::path& file, std::string* error);

std::filesystem::path defaultRatesFile();  // %LOCALAPPDATA%\BlockTime\rates.yaml

}  // namespace blocktime
