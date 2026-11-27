// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "airframe_read.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>

#include <nlohmann/json.hpp>

#include "debrief_review.h"  // the model, URL and headers the debrief uses

namespace blocktime {

namespace {

constexpr const char* kSystemPrompt = R"(You read screenshots of SimBrief's airframe edit page for Dave, who runs a virtual airline. Return exactly the values shown in these fields, as they appear on the page:

- registration: "Civil Registration"
- fin_number: "Fin Number" (the tail number; often empty)
- internal_id: "Internal ID", shaped like 730097_1791648865922, shown near the top of the page
- base_type: "Base Type" (an ICAO-style code such as B738, BBJ2 or B77W)
- weight_units: "Weight Units" (LBS or KGS)
- max_passengers: "Max Passengers"
- passenger_weight_lb: "Passenger Weight"
- bag_weight_lb: "Baggage Weight"
- empty_weight_lb: "Empty Weight"
- mzfw_lb: "Max Zero Fuel Weight"
- mtow_lb: "Max Takeoff Weight"
- mlw_lb: "Max Landing Weight"
- max_fuel_lb: "Max Fuel Capacity"
- max_cargo_lb: "Max Cargo Weight"
- min_runway_ft: "Min Runway" (feet)

Weights are numbers without units or thousands separators. Use null for a field that isn't visible in the screenshot or is empty; never guess a value.)";

// Standard base64 (RFC 4648) with padding.
std::string base64(std::string_view bytes) {
    static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((bytes.size() + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 2 < bytes.size(); i += 3) {
        const unsigned n = (static_cast<unsigned char>(bytes[i]) << 16) |
                           (static_cast<unsigned char>(bytes[i + 1]) << 8) | static_cast<unsigned char>(bytes[i + 2]);
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += kAlphabet[(n >> 6) & 63];
        out += kAlphabet[n & 63];
    }
    if (i < bytes.size()) {
        unsigned n = static_cast<unsigned char>(bytes[i]) << 16;
        if (i + 1 < bytes.size()) n |= static_cast<unsigned char>(bytes[i + 1]) << 8;
        out += kAlphabet[(n >> 18) & 63];
        out += kAlphabet[(n >> 12) & 63];
        out += i + 1 < bytes.size() ? kAlphabet[(n >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

constexpr const char* kTextFields[] = {"registration", "fin_number", "internal_id", "base_type", "weight_units"};
constexpr const char* kNumberFields[] = {"max_passengers", "passenger_weight_lb", "bag_weight_lb", "empty_weight_lb",
                                         "mzfw_lb",        "mtow_lb",             "mlw_lb",        "max_fuel_lb",
                                         "max_cargo_lb",   "min_runway_ft"};

std::string trimmed(std::string s) {
    const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), space));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), space).base(), s.end());
    return s;
}

std::string text(const nlohmann::json& j, const char* key) {
    return j.contains(key) && j[key].is_string() ? trimmed(j[key].get<std::string>()) : std::string();
}

std::optional<int> number(const nlohmann::json& j, const char* key) {
    if (!j.contains(key) || !j[key].is_number()) return std::nullopt;
    const double value = j[key].get<double>();
    if (value < 0 || value > 10'000'000) return std::nullopt;
    return static_cast<int>(std::lround(value));
}

std::string upper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

}  // namespace

std::string buildAirframeReadRequest(std::string_view imageBytes, std::string_view mediaType) {
    const nlohmann::json nullableString = {{"anyOf", {{{"type", "string"}}, {{"type", "null"}}}}};
    const nlohmann::json nullableInteger = {{"anyOf", {{{"type", "integer"}}, {{"type", "null"}}}}};
    nlohmann::json properties = nlohmann::json::object();
    nlohmann::json required = nlohmann::json::array();
    for (const char* key : kTextFields) {
        properties[key] = nullableString;
        required.push_back(key);
    }
    for (const char* key : kNumberFields) {
        properties[key] = nullableInteger;
        required.push_back(key);
    }
    const nlohmann::json schema = {
        {"type", "object"}, {"properties", properties}, {"required", required}, {"additionalProperties", false}};
    const nlohmann::json content = nlohmann::json::array({
        {{"type", "image"},
         {"source", {{"type", "base64"}, {"media_type", std::string(mediaType)}, {"data", base64(imageBytes)}}}},
        {{"type", "text"}, {"text", "Read this SimBrief airframe page."}},
    });
    const nlohmann::json body = {
        {"model", kReviewModel},
        {"max_tokens", 2000},
        {"output_config", {{"format", {{"type", "json_schema"}, {"schema", schema}}}}},
        {"fallbacks", "default"},
        {"system", kSystemPrompt},
        {"messages", nlohmann::json::array({{{"role", "user"}, {"content", content}}})},
    };
    return body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

AirframeReadAnswer parseAirframeReadResponse(int httpStatus, std::string_view body) {
    const auto j = nlohmann::json::parse(body, nullptr, false);
    if (httpStatus >= 400) {
        if (j.is_object() && j.contains("error") && j["error"].is_object() && j["error"].contains("message") &&
            j["error"]["message"].is_string()) {
            return {std::nullopt, "Claude: " + j["error"]["message"].get<std::string>()};
        }
        return {std::nullopt, std::format("Claude answered HTTP {}", httpStatus)};
    }
    const AirframeReadAnswer unreadable{std::nullopt, "Claude's answer wasn't readable"};
    if (!j.is_object()) return unreadable;
    const std::string stop = text(j, "stop_reason");
    if (stop == "refusal") return {std::nullopt, "Claude declined to read it"};
    if (stop == "max_tokens") return {std::nullopt, "Claude's answer was cut off"};
    if (!j.contains("content") || !j["content"].is_array()) return unreadable;
    for (const auto& block : j["content"]) {
        if (!block.is_object() || text(block, "type") != "text" || !block.contains("text") ||
            !block["text"].is_string()) {
            continue;
        }
        const auto r = nlohmann::json::parse(block["text"].get<std::string>(), nullptr, false);
        if (!r.is_object()) return unreadable;
        SimbriefReading reading;
        reading.registration = upper(text(r, "registration"));
        reading.finNumber = text(r, "fin_number");
        reading.internalId = text(r, "internal_id");
        reading.baseType = upper(text(r, "base_type"));
        reading.weightUnits = upper(text(r, "weight_units"));
        if (!reading.weightUnits.empty() && reading.weightUnits != "LBS") {
            return {std::nullopt, "Set SimBrief's weight units to LBS and take a new screenshot."};
        }
        reading.maxPassengers = number(r, "max_passengers");
        reading.passengerWeightLb = number(r, "passenger_weight_lb");
        reading.bagWeightLb = number(r, "bag_weight_lb");
        reading.emptyWeightLb = number(r, "empty_weight_lb");
        reading.mzfwLb = number(r, "mzfw_lb");
        reading.mtowLb = number(r, "mtow_lb");
        reading.mlwLb = number(r, "mlw_lb");
        reading.maxFuelLb = number(r, "max_fuel_lb");
        reading.maxCargoLb = number(r, "max_cargo_lb");
        reading.minRunwayFt = number(r, "min_runway_ft");
        return {reading, ""};
    }
    return unreadable;
}

}  // namespace blocktime
