// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rates.h"

#include <cmath>

#include "app_settings.h"
#include "config_yaml.h"

namespace blocktime {

namespace {

using config::YamlReader;

// A required map of `root`, or an error at `root` naming it.
bool section(YamlReader& r, const YAML::Node& root, const char* key, YAML::Node& out) {
    out = root[key];
    if (!out.IsMap()) return r.fail(root, std::string("missing ") + key);
    return true;
}

bool family(YamlReader& r, const YAML::Node& root, const char* key, FamilyRates& f) {
    YAML::Node m;
    return section(r, root, key, m) && r.number(m, "gate", f.gate) && r.number(m, "handling", f.handling) &&
           r.number(m, "atc_per_nm", f.atcPerNm) && r.number(m, "pilot_per_hour", f.pilotPerHour) &&
           r.number(m, "maintenance_per_hour", f.maintenancePerHour);
}

bool parse(YamlReader& r, const YAML::Node& root, Rates& x) {
    double fuel = 0;
    if (!r.number(root, "fuel_per_100lb", fuel)) return false;
    x.fuelPer100Lb = std::llround(fuel * 100);
    if (!r.number(root, "landing_per_1000lb_mlw", x.landingPer1000LbMlw) ||
        !r.number(root, "cargo_handling_per_1000lb", x.cargoHandlingPer1000Lb) ||
        !r.number(root, "flight_attendant_per_hour", x.flightAttendantPerHour) ||
        !family(r, root, "narrowbody", x.narrowbody) || !family(r, root, "widebody", x.widebody)) {
        return false;
    }
    YAML::Node m;
    if (!section(r, root, "catering_per_passenger", m) || !r.number(m, "under_3h", x.catering[0]) ||
        !r.number(m, "3h_to_8h", x.catering[1]) || !r.number(m, "8h_and_over", x.catering[2])) {
        return false;
    }
    if (!section(r, root, "fares", m) || !r.number(m, "economy_base", x.economyBase) ||
        !r.number(m, "economy_per_nm", x.economyPerNm) ||
        !r.number(m, "business_times_economy", x.businessTimesEconomy) ||
        !r.number(m, "first_times_economy", x.firstTimesEconomy)) {
        return false;
    }
    if (!section(r, root, "vip", m) || !r.number(m, "fare_base", x.vipFareBase) ||
        !r.number(m, "fare_per_nm", x.vipFarePerNm) || !r.number(m, "pilot_per_hour", x.vipPilotPerHour) ||
        !r.number(m, "flight_attendant_per_hour", x.vipFlightAttendantPerHour) ||
        !r.number(m, "flight_attendants_times", x.vipFlightAttendantsTimes) ||
        !r.number(m, "catering_times", x.vipCateringTimes)) {
        return false;
    }
    if (!section(r, root, "charter", m) || !r.number(m, "per_hour", x.charterPerHour) ||
        !r.number(m, "min_hours", x.charterMinHours) || !r.number(m, "ga_services", x.gaServices)) {
        return false;
    }
    return section(r, root, "cargo", m) && r.number(m, "base_per_100lb", x.cargoBasePer100Lb) &&
           r.number(m, "per_100lb_per_nm", x.cargoPer100LbPerNm);
}

}  // namespace

std::optional<Rates> loadRates(const std::filesystem::path& file, std::string* error) {
    const auto text = config::readFile(file, error);
    if (!text) return std::nullopt;
    YamlReader r("rates.yaml", error);
    const auto root = config::parseYaml(*text, r);
    if (!root) return std::nullopt;
    Rates rates;
    if (!root->IsMap()) return r.fail(*root, "expected the rates as key: value lines"), std::nullopt;
    if (!parse(r, *root, rates)) return std::nullopt;
    return rates;
}

std::filesystem::path defaultRatesFile() { return defaultSettingsFile().parent_path() / "rates.yaml"; }

}  // namespace blocktime
