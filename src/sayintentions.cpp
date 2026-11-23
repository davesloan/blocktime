// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sayintentions.h"

#include <nlohmann/json.hpp>

#include "url.h"

namespace blocktime {

namespace {

using nlohmann::json;

// A field as text, whether SayIntentions sent a string or a number this time.
std::string field(const json& airport, const char* name) {
    const auto f = airport.find(name);
    if (f == airport.end()) return {};
    if (f->is_string()) return f->get<std::string>();
    if (f->is_number_integer()) return std::to_string(f->get<long long>());
    return {};
}

std::string firstOf(const std::string& list) { return list.substr(0, list.find(',')); }

std::string runwayFor(const json& root, std::string_view icao, const char* listField) {
    const auto airports = root.find("airports");
    if (airports == root.end() || !airports->is_array()) return {};
    for (const json& a : *airports) {
        if (!a.is_object() || field(a, "airport") != icao) continue;
        std::string runway = normalizeRunway(firstOf(field(a, listField)));
        if (runway.empty()) runway = normalizeRunway(field(a, "active_runway"));
        return runway;
    }
    return {};
}

}  // namespace

std::string getWxUrl(std::string_view apiKey, std::string_view origin, std::string_view destination) {
    return "https://apipri.sayintentions.ai/sapi/getWX?api_key=" + percentEncode(apiKey) + "&icao=" +
           percentEncode(origin) + "," + percentEncode(destination);
}

AtisRunways parseAtisRunways(std::string_view body, std::string_view origin, std::string_view destination) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return {};
    return {runwayFor(root, origin, "active_runways_departing"), runwayFor(root, destination, "active_runways_arriving")};
}

std::string atisError(std::string_view body) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return {};
    const auto e = root.find("error");
    return e != root.end() && e->is_string() ? e->get<std::string>() : std::string();
}

std::string normalizeRunway(std::string_view runway) {
    while (!runway.empty() && runway.front() == ' ') runway.remove_prefix(1);
    while (!runway.empty() && runway.back() == ' ') runway.remove_suffix(1);
    std::size_t digits = 0;
    while (digits < runway.size() && runway[digits] >= '0' && runway[digits] <= '9') ++digits;
    if (digits < 1 || digits > 2) return {};
    const std::string_view rest = runway.substr(digits);
    if (rest.size() > 1 || (rest.size() == 1 && rest != "L" && rest != "R" && rest != "C")) return {};
    return (digits == 1 ? "0" : "") + std::string(runway);
}

}  // namespace blocktime
