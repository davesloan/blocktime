// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airline_config.h"

#include <algorithm>
#include <format>
#include <set>

#include "app_settings.h"
#include "config_yaml.h"

namespace blocktime {

namespace {

using config::YamlReader;

bool onlyChars(const std::string& text, bool allowDash) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [&](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (allowDash && c == '-');
    });
}

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

bool readAirplane(YamlReader& r, const YAML::Node& node, const AirlineConfig& config, ConfigAirplane& a) {
    if (!notEmpty(r, node, "registration", a.registration)) return false;
    if (!onlyChars(a.registration, true)) {
        return r.fail(node["registration"], std::format("registration \"{}\" may only use capitals, digits and -", a.registration));
    }
    if (!r.optionalString(node, "tail_number", a.tailNumber) || !notEmpty(r, node, "airframe", a.airframe) ||
        !notEmpty(r, node, "fleet", a.fleet) || !notEmpty(r, node, "simbrief_airframe_id", a.simbriefAirframeId) ||
        !date(r, node, "leased_on", a.leasedOn) || !r.string(node, "location", a.locationIcao) ||
        !r.optionalBool(node, "awaiting_delivery", a.awaitingDelivery)) {
        return false;
    }
    if (std::find(config.fleets.begin(), config.fleets.end(), a.fleet) == config.fleets.end()) {
        return r.fail(node["fleet"], std::format("{}: fleet \"{}\" isn't in the fleets list", a.registration, a.fleet));
    }
    if (a.leasedOn < config.foundedOn) {
        return r.fail(node["leased_on"], std::format("{}: leased_on is before founded_on", a.registration));
    }
    if (a.locationIcao.size() != 4 || !onlyChars(a.locationIcao, false)) {
        return r.fail(node["location"], std::format("{}: location must be a 4-character ICAO code", a.registration));
    }
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

        const YAML::Node fleets = (*root)["fleets"];
        if (!fleets.IsSequence()) return r.fail(*root, "missing the fleets list"), finish();
        for (const YAML::Node& f : fleets) {
            const std::string name = f.IsScalar() ? f.as<std::string>() : std::string();
            if (name.empty()) return r.fail(f, "a fleet needs a name"), finish();
            if (std::find(config.fleets.begin(), config.fleets.end(), name) != config.fleets.end()) {
                return r.fail(f, std::format("fleet \"{}\" is listed twice", name)), finish();
            }
            config.fleets.push_back(name);
        }

        const YAML::Node airplanes = (*root)["airplanes"];
        if (airplanes.IsDefined() && !airplanes.IsNull() && !airplanes.IsSequence()) {
            return r.fail(airplanes, "airplanes must be a list"), finish();
        }
        std::set<std::string> registrations, simbriefIds;
        for (const YAML::Node& node : airplanes) {
            ConfigAirplane a;
            if (!readAirplane(r, node, config, a)) return finish();
            if (!registrations.insert(a.registration).second) {
                return r.fail(node, std::format("duplicate registration {}", a.registration)), finish();
            }
            if (!simbriefIds.insert(a.simbriefAirframeId).second) {
                return r.fail(node, std::format("{}: duplicate simbrief_airframe_id", a.registration)), finish();
            }
            config.airplanes.push_back(std::move(a));
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
