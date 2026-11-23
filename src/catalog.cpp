// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog.h"

#include <algorithm>
#include <format>
#include <set>

#include "app_settings.h"
#include "config_yaml.h"

namespace blocktime {

namespace {

using config::YamlReader;

bool oneOf(const std::string& value, std::initializer_list<const char*> allowed) {
    return std::any_of(allowed.begin(), allowed.end(), [&](const char* a) { return value == a; });
}

bool positive(YamlReader& r, const YAML::Node& map, const char* key, int& out) {
    std::int64_t value = 0;
    if (!r.integer(map, key, value)) return false;
    if (value <= 0 || value > 10'000'000) return r.fail(map[key], std::format("{} must be a positive number", key));
    out = static_cast<int>(value);
    return true;
}

bool readType(YamlReader& r, const YAML::Node& node, CatalogType& t) {
    if (!r.string(node, "code", t.code) || !r.string(node, "family", t.family) || !r.string(node, "role", t.role)) {
        return false;
    }
    if (!oneOf(t.family, {"737", "777"})) return r.fail(node["family"], "family must be \"737\" or \"777\"");
    if (!oneOf(t.role, {"airliner", "executive", "freighter"})) {
        return r.fail(node["role"], "role must be airliner, executive or freighter");
    }
    if (!positive(r, node, "turnaround_min", t.turnaroundMin) || !positive(r, node, "cruise_speed_kt", t.cruiseSpeedKt)) {
        return false;
    }
    std::int64_t lease = 0;
    if (!r.integer(node, "lease_per_month", lease)) return false;
    if (lease <= 0 || lease % 5 != 0) {
        return r.fail(node["lease_per_month"], "lease_per_month must be a positive multiple of 5 dollars");
    }
    t.monthlyLeaseCents = lease * 100;
    return r.string(node, "sim_folder", t.simFolder);
}

bool readAirframe(YamlReader& r, const YAML::Node& node, const Catalog& catalog, CatalogAirframe& a) {
    if (!r.string(node, "name", a.name) || !r.string(node, "type", a.typeCode)) return false;
    const CatalogType* type = catalog.type(a.typeCode);
    if (!type) return r.fail(node["type"], std::format("{}: unknown type \"{}\"", a.name, a.typeCode));

    const YAML::Node cabins = node["cabins"];
    if (cabins.IsDefined() && !cabins.IsNull()) {
        if (!cabins.IsMap()) return r.fail(cabins, std::format("{}: cabins must be a map like {{ economy: 48 }}", a.name));
        for (const auto& entry : cabins) {
            Cabin cabin{entry.first.as<std::string>(), 0};
            if (!oneOf(cabin.cabinClass, {"economy", "business", "first"})) {
                return r.fail(entry.first, std::format("{}: cabin class must be economy, business or first", a.name));
            }
            std::int64_t seats = 0;
            try {
                seats = entry.second.as<std::int64_t>();
            } catch (const YAML::BadConversion&) {
                seats = 0;
            }
            if (seats <= 0 || seats > 1000) return r.fail(entry.second, std::format("{}: seats must be a positive number", a.name));
            cabin.seats = static_cast<int>(seats);
            a.cabins.push_back(cabin);
        }
    }
    if (type->role == "freighter" && !a.cabins.empty()) {
        return r.fail(cabins, std::format("{}: a freighter has no cabins", a.name));
    }
    if (type->role != "freighter" && a.cabins.empty()) {
        return r.fail(node, std::format("{}: an {} needs cabins", a.name, type->role));
    }

    const YAML::Node holds = node["holds"];
    if (!holds.IsSequence() || holds.size() == 0) return r.fail(node, std::format("{}: needs at least one hold", a.name));
    std::set<std::string> seen;
    for (const YAML::Node& h : holds) {
        Hold hold{};
        if (!r.string(h, "hold", hold.hold)) return false;
        if (!oneOf(hold.hold, {"forward", "aft", "bulk", "main_deck"})) {
            return r.fail(h["hold"], std::format("{}: hold must be forward, aft, bulk or main_deck", a.name));
        }
        if (!seen.insert(hold.hold).second) return r.fail(h, std::format("{}: hold {} is listed twice", a.name, hold.hold));
        if (!positive(r, h, "max_lb", hold.maxLb) || !positive(r, h, "ratio", hold.ratio)) return false;
        a.holds.push_back(hold);
    }

    const YAML::Node limits = node["limits"];
    if (!limits.IsDefined() || !limits.IsMap()) return r.fail(node, std::format("{}: needs limits", a.name));
    AirplaneLimits& l = a.limits;
    return positive(r, limits, "min_runway_ft", l.minRunwayFt) && positive(r, limits, "mlw_lb", l.mlwLb) &&
           positive(r, limits, "passenger_weight_lb", l.passengerWeightLb) &&
           positive(r, limits, "bag_weight_lb", l.bagWeightLb) && positive(r, limits, "max_payload_lb", l.maxPayloadLb);
}

}  // namespace

const CatalogType* Catalog::type(std::string_view code) const {
    const auto it = std::find_if(types.begin(), types.end(), [&](const CatalogType& t) { return t.code == code; });
    return it == types.end() ? nullptr : &*it;
}

const CatalogAirframe* Catalog::airframe(std::string_view name) const {
    const auto it =
        std::find_if(airframes.begin(), airframes.end(), [&](const CatalogAirframe& a) { return a.name == name; });
    return it == airframes.end() ? nullptr : &*it;
}

std::optional<Catalog> parseCatalog(std::string_view yaml, std::string* error) {
    std::string message;
    YamlReader r("catalog.yaml", &message);
    const auto finish = [&]() -> std::optional<Catalog> {
        if (error) *error = message;
        return std::nullopt;
    };
    const auto root = config::parseYaml(yaml, r);
    if (!root) return finish();
    try {
        Catalog catalog;
        const YAML::Node types = (*root)["types"];
        if (!types.IsSequence()) return r.fail(*root, "missing the types list"), finish();
        for (const YAML::Node& node : types) {
            CatalogType t{};
            if (!readType(r, node, t)) return finish();
            if (catalog.type(t.code)) return r.fail(node, std::format("type {} is listed twice", t.code)), finish();
            catalog.types.push_back(t);
        }
        const YAML::Node airframes = (*root)["airframes"];
        if (!airframes.IsSequence()) return r.fail(*root, "missing the airframes list"), finish();
        for (const YAML::Node& node : airframes) {
            CatalogAirframe a;
            if (!readAirframe(r, node, catalog, a)) return finish();
            if (catalog.airframe(a.name)) return r.fail(node, std::format("airframe {} is listed twice", a.name)), finish();
            catalog.airframes.push_back(std::move(a));
        }
        if (error) error->clear();
        return catalog;
    } catch (const YAML::Exception& e) {
        r.fail(e.mark, e.msg);
        return finish();
    }
}

std::optional<Catalog> loadCatalog(const std::filesystem::path& file, std::string* error) {
    const auto text = config::readFile(file, error);
    if (!text) return std::nullopt;
    return parseCatalog(*text, error);
}

std::filesystem::path defaultCatalogFile() { return defaultSettingsFile().parent_path() / "catalog.yaml"; }

}  // namespace blocktime
