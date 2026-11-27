// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "catalog.h"

#include <algorithm>
#include <format>
#include <set>

#include <windows.h>

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
    if (!r.string(node, "sim_folder", t.simFolder) || !r.number(node, "wingspan_m", t.wingspanM)) return false;
    if (t.wingspanM <= 0) return r.fail(node["wingspan_m"], "wingspan_m must be positive");
    const YAML::Node bases = node["simbrief_base_types"];
    if (!bases.IsSequence() || bases.size() == 0) {
        return r.fail(node, "needs simbrief_base_types, a list like [B738]");
    }
    for (const YAML::Node& b : bases) t.simbriefBaseTypes.push_back(b.as<std::string>());
    if (!r.string(node, "factory", t.factoryIcao)) return false;
    return positive(r, node, "delivery_fee_per_nm", t.deliveryFeePerNmDollars);
}

bool readLayout(YamlReader& r, const YAML::Node& node, const CatalogType* type, CatalogAirframe& a) {
    if (!r.string(node, "name", a.name)) return false;
    a.typeCode = type->code;

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
        if (!positive(r, h, "share_lb", hold.maxLb) || !positive(r, h, "ratio", hold.ratio)) return false;
        a.holds.push_back(hold);
    }
    return true;
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

std::optional<Catalog> parseAircraftFile(std::string_view yaml, std::string_view fileName, std::string* error) {
    std::string message;
    YamlReader r(std::string(fileName), &message);
    const auto finish = [&]() -> std::optional<Catalog> {
        if (error) *error = message;
        return std::nullopt;
    };
    const auto root = config::parseYaml(yaml, r);
    if (!root) return finish();
    try {
        Catalog catalog;
        CatalogType t{};
        if (!readType(r, *root, t)) return finish();
        catalog.types.push_back(t);
        const YAML::Node layouts = (*root)["layouts"];
        if (!layouts.IsSequence() || layouts.size() == 0) return r.fail(*root, "needs at least one layout"), finish();
        for (const YAML::Node& node : layouts) {
            CatalogAirframe a;
            if (!readLayout(r, node, &catalog.types[0], a)) return finish();
            if (catalog.airframe(a.name)) return r.fail(node, std::format("layout {} is listed twice", a.name)), finish();
            catalog.airframes.push_back(std::move(a));
        }
        if (error) error->clear();
        return catalog;
    } catch (const YAML::Exception& e) {
        r.fail(e.mark, e.msg);
        return finish();
    }
}

Catalog loadCatalogDir(const std::filesystem::path& dir, std::vector<std::string>* notices) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".yaml") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    Catalog catalog;
    for (const auto& file : files) {
        std::string error;
        const auto text = config::readFile(file, &error);
        const auto one = text ? parseAircraftFile(*text, file.filename().string(), &error) : std::nullopt;
        if (!one) {
            if (notices) notices->push_back(error + " (skipped)");
            continue;
        }
        const bool repeated = catalog.type(one->types[0].code) ||
                              std::any_of(one->airframes.begin(), one->airframes.end(),
                                          [&](const CatalogAirframe& a) { return catalog.airframe(a.name) != nullptr; });
        if (repeated) {
            if (notices) {
                notices->push_back(std::format("{}: type {} or one of its layouts is already in another file (skipped)",
                                               file.filename().string(), one->types[0].code));
            }
            continue;
        }
        catalog.types.push_back(one->types[0]);
        catalog.airframes.insert(catalog.airframes.end(), one->airframes.begin(), one->airframes.end());
    }
    return catalog;
}

std::filesystem::path defaultCatalogDir() {
    wchar_t buffer[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(std::wstring(buffer, n)).parent_path() / "data" / "aircraft";
}

}  // namespace blocktime
