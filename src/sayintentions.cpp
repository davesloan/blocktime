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
    if (f->is_number_float()) return f->dump();
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

std::string getWxUrl(std::string_view apiKey, std::string_view icao) {
    return "https://apipri.sayintentions.ai/sapi/getWX?api_key=" + percentEncode(apiKey) + "&icao=" +
           percentEncode(icao);
}

std::string commsHistoryUrl(std::string_view apiKey) {
    return "https://apipri.sayintentions.ai/sapi/getCommsHistory?api_key=" + percentEncode(apiKey);
}

std::optional<Atis> parseAtis(std::string_view body, std::string_view icao) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    const auto airports = root.find("airports");
    if (airports == root.end() || !airports->is_array()) return std::nullopt;
    for (const json& a : *airports) {
        if (!a.is_object() || field(a, "airport") != icao) continue;
        return Atis{std::string(icao),        field(a, "current"),  field(a, "atis"),
                    field(a, "metar"),        field(a, "altimeter"), field(a, "active_runways_departing"),
                    field(a, "active_runways_arriving")};
    }
    return std::nullopt;
}

std::optional<CommsHistory> parseCommsHistory(std::string_view body) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    const auto history = root.find("comm_history");
    if (history == root.end() || !history->is_array()) return std::nullopt;
    CommsHistory h;
    h.flightId = field(root, "flight_id");
    for (const json& e : *history)
        if (e.is_object()) h.entries.push_back(e);
    return h;
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

namespace {

std::optional<double> number(const json& j, const char* name) {
    const auto f = j.find(name);
    if (f == j.end()) return std::nullopt;
    if (f->is_number()) return f->get<double>();
    if (!f->is_string()) return std::nullopt;
    try {
        return std::stod(f->get<std::string>());
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

const json* child(const json& j, const char* name) {
    const auto f = j.find(name);
    return f != j.end() && f->is_object() ? &*f : nullptr;
}

}  // namespace

std::optional<SiFlight> parseSiFlight(std::string_view body) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    const json* details = child(root, "flight_details");
    const json* current = details ? child(*details, "current_flight") : nullptr;
    if (!current) return std::nullopt;
    SiFlight f;
    f.destination = field(*current, "flight_destination");
    f.assignedGate = field(*current, "assigned_gate");
    f.gateLat = number(*current, "assigned_gate_lat");
    f.gateLon = number(*current, "assigned_gate_lon");
    f.arrivingRunway = normalizeRunway(field(*current, "flight_plan_arriving_runway"));
    if (f.arrivingRunway.empty()) {
        const json* atc = child(*details, "atc");
        const json* approach = atc ? child(*atc, "approach") : nullptr;
        if (approach) f.arrivingRunway = normalizeRunway(field(*approach, "runway"));
    }
    return f;
}

std::string assignGateUrl(std::string_view apiKey, std::string_view gate, std::string_view icao) {
    return "https://apipri.sayintentions.ai/sapi/assignGate?api_key=" + percentEncode(apiKey) + "&gate=" +
           percentEncode(gate) + "&airport=" + percentEncode(icao);
}

std::string getParkingUrl(std::string_view apiKey) {
    return "https://apipri.sayintentions.ai/sapi/getParking?api_key=" + percentEncode(apiKey);
}

std::optional<std::string> parseAssignedGate(std::string_view body) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    const std::string name = field(root, "assigned_gate_name");
    if (name.empty()) return std::nullopt;
    return name;
}

std::optional<SiParking> parseParking(std::string_view body) {
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return std::nullopt;
    const json* parking = child(root, "parking");
    if (!parking) return std::nullopt;
    SiParking p;
    p.name = field(*parking, "name");
    p.lat = number(*parking, "lat").value_or(0);
    p.lon = number(*parking, "lon").value_or(0);
    if (p.name.empty()) return std::nullopt;
    return p;
}

}  // namespace blocktime
