// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "telemetry_json.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "event_log.h"
#include "telemetry_filter.h"

namespace blocktime {

namespace {

int engineCount(const TelemetrySample& s) { return std::clamp(s.numEngines, 0, 4); }

nlohmann::json engRunning(const TelemetrySample& s) {
    nlohmann::json a = nlohmann::json::array();
    for (int i = 0; i < engineCount(s); ++i) a.push_back(s.engCombustion[i] != 0);
    return a;
}

double totalFuelFlow(const TelemetrySample& s) {
    double sum = 0;
    for (int i = 0; i < engineCount(s); ++i) sum += s.engFuelFlowPph[i];
    return sum;
}

double number(const nlohmann::json& d, const char* key) {
    const auto it = d.find(key);
    return it != d.end() && it->is_number() ? it->get<double>() : 0.0;
}

std::int32_t flag(const nlohmann::json& d, const char* key) {
    const auto it = d.find(key);
    if (it == d.end()) return 0;
    if (it->is_boolean()) return it->get<bool>() ? 1 : 0;
    return it->is_number() && it->get<double>() != 0 ? 1 : 0;
}

std::string text(const nlohmann::json& d, const char* key) {
    const auto it = d.find(key);
    return it != d.end() && it->is_string() ? it->get<std::string>() : std::string();
}

void copyString(char* dst, std::size_t cap, const std::string& src) {
    std::memset(dst, 0, cap);
    std::memcpy(dst, src.data(), std::min(src.size(), cap - 1));
}

std::optional<TimePoint> time(const nlohmann::json& d, const char* key) { return parseUtc(text(d, key)); }

}  // namespace

nlohmann::json toJson(const TelemetrySample& s) {
    return {
        {"parking_brake", s.parkingBrake != 0},
        {"on_ground", s.onGround != 0},
        {"fuel_lb", s.fuelTotalLb},
        {"fuel_gal", s.fuelTotalGal},
        {"lat", s.latitudeDeg},
        {"lon", s.longitudeDeg},
        {"alt_ft", s.altitudeFt},
        {"alt_agl_ft", s.altitudeAglFt},
        {"alt_ind_ft", s.indicatedAltitudeFt},
        {"vs_fpm", s.verticalSpeedFpm},
        {"gs_kt", s.groundSpeedKt},
        {"ias_kt", s.indicatedAirspeedKt},
        {"heading_true_deg", s.headingTrueDeg},
        {"engines", s.numEngines},
        {"eng_running", engRunning(s)},
        {"fuel_flow_pph", totalFuelFlow(s)},
        {"beacon", s.lightBeacon != 0},
        {"strobe", s.lightStrobe != 0},
        {"mach", s.mach},
        {"flaps_te_pct", s.flapsTrailingPct},
        {"flaps_handle", s.flapsHandleIndex},
        {"title", std::string(simString(s.title, sizeof(s.title)))},
        {"atc_id", std::string(simString(s.atcId, sizeof(s.atcId)))},
    };
}

TelemetrySample telemetryFromJson(const nlohmann::json& d) {
    TelemetrySample s{};
    if (!d.is_object()) return s;
    s.parkingBrake = flag(d, "parking_brake");
    s.onGround = flag(d, "on_ground");
    s.fuelTotalLb = number(d, "fuel_lb");
    s.fuelTotalGal = number(d, "fuel_gal");
    s.latitudeDeg = number(d, "lat");
    s.longitudeDeg = number(d, "lon");
    s.altitudeFt = number(d, "alt_ft");
    s.altitudeAglFt = number(d, "alt_agl_ft");
    s.indicatedAltitudeFt = number(d, "alt_ind_ft");
    s.verticalSpeedFpm = number(d, "vs_fpm");
    s.groundSpeedKt = number(d, "gs_kt");
    s.indicatedAirspeedKt = number(d, "ias_kt");
    s.headingTrueDeg = number(d, "heading_true_deg");
    s.numEngines = static_cast<std::int32_t>(number(d, "engines"));
    if (const auto it = d.find("eng_running"); it != d.end() && it->is_array()) {
        for (std::size_t i = 0; i < std::min<std::size_t>(it->size(), 4); ++i) {
            s.engCombustion[i] = (*it)[i].is_boolean() && (*it)[i].get<bool>() ? 1 : 0;
        }
    }
    s.lightBeacon = flag(d, "beacon");
    s.lightStrobe = flag(d, "strobe");
    s.mach = number(d, "mach");
    s.flapsTrailingPct = number(d, "flaps_te_pct");
    s.flapsHandleIndex = static_cast<std::int32_t>(number(d, "flaps_handle"));
    copyString(s.title, sizeof(s.title), text(d, "title"));
    copyString(s.atcId, sizeof(s.atcId), text(d, "atc_id"));
    return s;
}

std::optional<BlockEvent> blockEventFromJson(std::string_view type, const nlohmann::json& d) {
    if (!d.is_object() || text(d, "flight_id").empty()) return std::nullopt;
    const auto t = time(d, "time");
    if (!t) return std::nullopt;
    if (type == "block_out") {
        BlockOut e;
        e.flightId = text(d, "flight_id");
        e.time = *t;
        e.latDeg = number(d, "lat");
        e.lonDeg = number(d, "lon");
        e.fuelLb = number(d, "fuel_lb");
        e.fuelGal = number(d, "fuel_gal");
        e.movedM = number(d, "moved_m");
        e.atcId = text(d, "atc_id");
        e.title = text(d, "title");
        e.plannedFlightId = static_cast<std::int64_t>(number(d, "planned_flight_id"));
        e.aircraftFolder = text(d, "aircraft_folder");
        e.parking = text(d, "parking");
        return e;
    }
    if (type == "block_in") {
        BlockIn e;
        e.flightId = text(d, "flight_id");
        e.time = *t;
        e.latDeg = number(d, "lat");
        e.lonDeg = number(d, "lon");
        e.fuelLb = number(d, "fuel_lb");
        e.fuelGal = number(d, "fuel_gal");
        e.shutdownTime = time(d, "shutdown_time").value_or(*t);
        e.shutdownFuelLb = number(d, "shutdown_fuel_lb");
        e.blockTimeS = static_cast<long long>(number(d, "block_time_s"));
        e.parking = text(d, "parking");
        return e;
    }
    return std::nullopt;
}

}  // namespace blocktime
