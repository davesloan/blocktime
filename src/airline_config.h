// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// airline.yaml (D-076): the airline's identity, read on every start (D-093), its founding date and starting cash, and
// its preferred gates. Airplanes are leased in the app (D-242) and fleets are gone (D-244).

#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "dates.h"
#include "money.h"

namespace blocktime {

struct AirlineIdentity {
    std::string name, icaoCode, callsign;
};

struct AirlineConfig {
    AirlineIdentity identity;
    Date foundedOn;
    Cents startingCashCents = 0;
    // By ICAO, SayIntentions' short gate names ("D14"), first choice first; only for gate flights (D-147).
    std::map<std::string, std::vector<std::string>> preferredGates;
};

// Parses and validates airline.yaml's text. On failure returns nullopt with "airline.yaml line N: ..." in *error.
std::optional<AirlineConfig> parseAirlineConfig(std::string_view yaml, std::string* error);
std::optional<AirlineConfig> loadAirlineConfig(const std::filesystem::path& file, std::string* error);

std::filesystem::path defaultAirlineConfigFile();  // %LOCALAPPDATA%\BlockTime\airline.yaml

}  // namespace blocktime
