// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airline_config.h"

#include <algorithm>
#include <format>
#include <utility>

#include "app_settings.h"
#include "config_yaml.h"

namespace blocktime {

namespace {

using config::YamlReader;

bool date(YamlReader& r, const YAML::Node& map, const char* key, Date& out) {
    std::string text;
    if (!r.string(map, key, text)) return false;
    const auto parsed = parseDate(text);
    if (!parsed) return r.fail(map[key], std::format("{} must be a date like 2026-11-22, not \"{}\"", key, text));
    out = *parsed;
    return true;
}

bool notEmpty(YamlReader& r, const YAML::Node& map, const char* key, std::string& out) {
    if (!r.string(map, key, out)) return false;
    if (out.empty()) return r.fail(map[key], std::format("{} can't be empty", key));
    return true;
}

}  // namespace

std::optional<AirlineConfig> parseAirlineConfig(std::string_view yaml, std::string* error) {
    std::string message;
    YamlReader r("airline.yaml", &message);
    const auto finish = [&]() -> std::optional<AirlineConfig> {
        if (error) *error = message;
        return std::nullopt;
    };
    const auto root = config::parseYaml(yaml, r);
    if (!root) return finish();
    try {
        AirlineConfig config;
        const YAML::Node airline = (*root)["airline"];
        if (!airline.IsMap()) return r.fail(*root, "missing the airline section"), finish();
        AirlineIdentity& id = config.identity;
        if (!notEmpty(r, airline, "name", id.name) || !r.string(airline, "icao", id.icaoCode) ||
            !notEmpty(r, airline, "callsign", id.callsign)) {
            return finish();
        }
        if (id.icaoCode.size() != 3 || !std::all_of(id.icaoCode.begin(), id.icaoCode.end(), [](char c) { return c >= 'A' && c <= 'Z'; })) {
            return r.fail(airline["icao"], "icao must be 3 capital letters"), finish();
        }
        if (!date(r, *root, "founded_on", config.foundedOn)) return finish();
        std::int64_t cash = 0;
        if (!r.integer(*root, "starting_cash", cash)) return finish();
        if (cash <= 0 || cash % 5 != 0) {
            return r.fail((*root)["starting_cash"], "starting_cash must be a positive multiple of 5 dollars"), finish();
        }
        config.startingCashCents = cash * 100;

        // fleets and airplanes, if an older file still has them, are ignored: airplanes are leased in the app
        // (D-242) and fleets are gone (D-244).

        // Optional: { KPDX: [D14, D12] }, SayIntentions' short gate names by ICAO (D-147).
        const YAML::Node preferred = (*root)["preferred_gates"];
        if (preferred.IsDefined() && !preferred.IsNull()) {
            if (!preferred.IsMap()) return r.fail(preferred, "preferred_gates must be a map like { KPDX: [D14] }"), finish();
            for (const auto& entry : preferred) {
                std::vector<std::string> gates;
                for (const YAML::Node& g : entry.second) gates.push_back(g.as<std::string>());
                config.preferredGates[entry.first.as<std::string>()] = std::move(gates);
            }
        }
        if (error) error->clear();
        return config;
    } catch (const YAML::Exception& e) {
        r.fail(e.mark, e.msg);
        return finish();
    }
}

std::optional<AirlineConfig> loadAirlineConfig(const std::filesystem::path& file, std::string* error) {
    const auto text = config::readFile(file, error);
    if (!text) return std::nullopt;
    return parseAirlineConfig(*text, error);
}

std::filesystem::path defaultAirlineConfigFile() { return defaultSettingsFile().parent_path() / "airline.yaml"; }

}  // namespace blocktime
