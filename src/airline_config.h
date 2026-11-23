// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// airline.yaml (D-076): the airline's identity, read on every start (D-093), and the initial data for fleets and
// leased airplanes (D-082, D-087).

#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalog.h"
#include "dates.h"
#include "money.h"

namespace blocktime {

struct AirlineIdentity {
    std::string name, icaoCode, callsign;
};

// An airplane's own values in place of its airframe's limits (D-100). Unset keeps the airframe's.
struct LimitOverrides {
    std::optional<int> minRunwayFt, mlwLb, passengerWeightLb, bagWeightLb, maxPayloadLb;
};

AirplaneLimits withOverrides(const AirplaneLimits& defaults, const LimitOverrides& overrides);

struct ConfigAirplane {
    std::string registration;
    std::string tailNumber;  // "" = none
    std::string airframe, fleet, simbriefAirframeId, locationIcao;
    Date leasedOn;
    bool awaitingDelivery = false;
    LimitOverrides limits;     // read at lease time only
    std::string flightNumber;  // the default for its free flights (D-106), 1-4 digits; "" = none. Read every start
};

struct AirlineConfig {
    AirlineIdentity identity;
    Date foundedOn;
    Cents startingCashCents = 0;
    Cents fuelPriceCentsPer100Lb = 4100;  // design.md §11; locked on each planned flight (D-104)
    std::vector<std::string> fleets;
    std::vector<ConfigAirplane> airplanes;
};

// Parses and validates airline.yaml's text. On failure returns nullopt with "airline.yaml line N: ..." in *error.
// Whether each airplane's airframe exists is checked at leasing time, against the catalog.
std::optional<AirlineConfig> parseAirlineConfig(std::string_view yaml, std::string* error);
std::optional<AirlineConfig> loadAirlineConfig(const std::filesystem::path& file, std::string* error);

std::filesystem::path defaultAirlineConfigFile();  // %LOCALAPPDATA%\BlockTime\airline.yaml

}  // namespace blocktime
