// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sim_probe.h"

#include <windows.h>

#include <SimConnect.h>

#include <cstdint>
#include <cstring>
#include <format>

#include "pmdg_probe.h"

namespace blocktime {

namespace {

// Ids of their own, well clear of SimConnection's (definitions 1, requests 1-11).
enum : SIMCONNECT_DATA_DEFINITION_ID { DEF_GSX = 101, DEF_SIMVARS = 102, DEF_FACILITY = 103 };
enum : SIMCONNECT_DATA_REQUEST_ID {
    REQ_GSX = 101,
    REQ_SIMVARS = 102,
    REQ_PMDG_737 = 103,
    REQ_PMDG_777 = 104,
    REQ_FACILITY_FIRST = 110,  // one per airport asked for, up to 189
    REQ_LAST = 189
};

// GSX Pro's L:vars as community tools name them; the probe is how we find out (recorder.md, "GSX Pro").
constexpr const char* kGsxVars[] = {
    "FSDT_GSX_COUATL_STARTED",
    "FSDT_GSX_CATERING_STATE",
    "FSDT_GSX_REFUELING_STATE",
    "FSDT_GSX_BOARDING_STATE",
    "FSDT_GSX_DEBOARDING_STATE",
    "FSDT_GSX_DEPARTURE_STATE",
    "FSDT_GSX_NUMPASSENGERS",
    "FSDT_GSX_NUMPASSENGERS_BOARDING_TOTAL",
    "FSDT_GSX_NUMPASSENGERS_DEBOARDING_TOTAL",
    "FSDT_GSX_BOARDING_CARGO_PERCENT",
    "FSDT_GSX_DEBOARDING_CARGO_PERCENT",
    "FSDT_GSX_FUELHOSE_CONNECTED",
};

struct SimVar {
    const char* name;
    const char* units;
};
constexpr SimVar kSimVars[] = {
    {"LIGHT BEACON", "bool"},  {"LIGHT STROBE", "bool"},           {"LIGHT LANDING", "bool"},
    {"LIGHT TAXI", "bool"},    {"LIGHT NAV", "bool"},              {"CABIN SEATBELTS ALERT SWITCH", "bool"},
    {"APU SWITCH", "bool"},    {"GEAR HANDLE POSITION", "bool"},  {"GEAR TOTAL PCT EXTENDED", "percent"},
};

// Facility fields, in the order the data comes back. Types from the SimConnect_AddToFacilityDefinition docs; each
// record's raw bytes are logged too, in case one of them is wrong.
constexpr const char* kFacilityFields[] = {
    "OPEN AIRPORT", "LATITUDE", "LONGITUDE", "ALTITUDE",
    "OPEN RUNWAY", "LATITUDE", "LONGITUDE", "ALTITUDE", "HEADING", "LENGTH", "WIDTH", "PRIMARY_NUMBER",
    "PRIMARY_DESIGNATOR", "SECONDARY_NUMBER", "SECONDARY_DESIGNATOR", "CLOSE RUNWAY",
    "OPEN TAXI_PARKING", "TYPE", "NAME", "SUFFIX", "NUMBER", "HEADING", "RADIUS", "BIAS_X", "BIAS_Z",
    "CLOSE TAXI_PARKING", "CLOSE AIRPORT",
};

#pragma pack(push, 1)
struct AirportRecord {
    double lat, lon, alt;
};
struct RunwayRecord {
    double lat, lon, alt;
    float heading, length, width;
    std::int32_t primaryNumber, primaryDesignator, secondaryNumber, secondaryDesignator;
};
struct ParkingRecord {
    std::int32_t type, name, suffix;
    std::uint32_t number;
    float heading, radius, biasX, biasZ;
};
#pragma pack(pop)

std::string hex(const void* data, std::size_t size) {
    std::string out;
    out.reserve(size * 2);
    const auto* b = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < size; ++i) out += std::format("{:02x}", b[i]);
    return out;
}

template <class T>
bool read(T& out, const void* data, std::size_t size) {
    if (size < sizeof(T)) return false;
    std::memcpy(&out, data, sizeof(T));
    return true;
}

}  // namespace

bool SimProbe::owns(unsigned long requestId) { return requestId >= REQ_GSX && requestId <= REQ_LAST; }

bool SimProbe::setup(void* handle) {
    HANDLE h = static_cast<HANDLE>(handle);
    bool ok = true;
    for (const char* v : kGsxVars) {
        ok &= SUCCEEDED(SimConnect_AddToDataDefinition(h, DEF_GSX, std::format("L:{}", v).c_str(), "number"));
    }
    ok &= SUCCEEDED(SimConnect_RequestDataOnSimObject(h, REQ_GSX, DEF_GSX, SIMCONNECT_OBJECT_ID_USER,
                                                      SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED));
    for (const SimVar& v : kSimVars) ok &= SUCCEEDED(SimConnect_AddToDataDefinition(h, DEF_SIMVARS, v.name, v.units));
    ok &= SUCCEEDED(SimConnect_RequestDataOnSimObject(h, REQ_SIMVARS, DEF_SIMVARS, SIMCONNECT_OBJECT_ID_USER,
                                                      SIMCONNECT_PERIOD_SECOND, SIMCONNECT_DATA_REQUEST_FLAG_CHANGED));
    for (const char* f : kFacilityFields) ok &= SUCCEEDED(SimConnect_AddToFacilityDefinition(h, DEF_FACILITY, f));
    const bool pmdg737 = probe::registerPmdg737(handle, REQ_PMDG_737);
    const bool pmdg777 = probe::registerPmdg777(handle, REQ_PMDG_777);
    log_.write("probe_started", {{"ok", ok}, {"pmdg_737", pmdg737}, {"pmdg_777", pmdg777}});
    return ok;
}

void SimProbe::onSimObjectData(unsigned long requestId, const void* data, std::size_t size) {
    nlohmann::json values = nlohmann::json::object();
    if (requestId == REQ_GSX) {
        constexpr std::size_t n = std::size(kGsxVars);
        if (size < n * sizeof(double)) return;
        for (std::size_t i = 0; i < n; ++i) {
            double v = 0;
            std::memcpy(&v, static_cast<const char*>(data) + i * sizeof(double), sizeof v);
            values[kGsxVars[i]] = v;
        }
        if (values == lastGsx_) return;
        lastGsx_ = values;
        log_.write("probe_gsx", values);
    } else if (requestId == REQ_SIMVARS) {
        constexpr std::size_t n = std::size(kSimVars);
        if (size < n * sizeof(double)) return;
        for (std::size_t i = 0; i < n; ++i) {
            double v = 0;
            std::memcpy(&v, static_cast<const char*>(data) + i * sizeof(double), sizeof v);
            values[kSimVars[i].name] = v;
        }
        if (values == lastSimVars_) return;
        lastSimVars_ = values;
        log_.write("probe_simvars", values);
    }
}

void SimProbe::onClientData(unsigned long requestId, const void* data, std::size_t size) {
    if (requestId == REQ_PMDG_737) offer(pmdg737_, probe::decodePmdg737(data, size));
    if (requestId == REQ_PMDG_777) offer(pmdg777_, probe::decodePmdg777(data, size));
}

void SimProbe::offer(Pending& p, nlohmann::json values) {
    if (values.is_null() || values == p.last) return;
    p.waiting = std::move(values);
    tick();
}

void SimProbe::tick() {
    const auto now = std::chrono::steady_clock::now();
    for (Pending* p : {&pmdg737_, &pmdg777_}) {
        if (p->waiting.is_null() || now - p->lastWritten < std::chrono::seconds(1)) continue;
        log_.write(p->type, p->waiting);
        p->last = std::move(p->waiting);
        p->waiting = nullptr;
        p->lastWritten = now;
    }
}

void SimProbe::requestFacilities(void* handle, const std::vector<std::string>& icaos) {
    unsigned long id = REQ_FACILITY_FIRST;
    for (const std::string& icao : icaos) {
        if (icao.empty() || id > REQ_LAST) continue;
        facilities_[id] = {{"icao", icao}, {"runways", nlohmann::json::array()}, {"parkings", nlohmann::json::array()}};
        SimConnect_RequestFacilityData(static_cast<HANDLE>(handle), DEF_FACILITY, id, icao.c_str());
        ++id;
    }
}

void SimProbe::onFacilityData(const void* recv) {
    const auto* r = static_cast<const SIMCONNECT_RECV_FACILITY_DATA*>(recv);
    const auto it = facilities_.find(r->UserRequestId);
    if (it == facilities_.end()) return;
    const std::size_t offset = offsetof(SIMCONNECT_RECV_FACILITY_DATA, Data);
    const std::size_t size = r->dwSize > offset ? r->dwSize - offset : 0;
    const void* data = &r->Data;
    nlohmann::json& f = it->second;
    switch (r->Type) {
        case SIMCONNECT_FACILITY_DATA_AIRPORT: {
            AirportRecord a{};
            if (read(a, data, size)) f["airport"] = {{"lat", a.lat}, {"lon", a.lon}, {"alt", a.alt}};
            f["airport_raw"] = hex(data, size);
            break;
        }
        case SIMCONNECT_FACILITY_DATA_RUNWAY: {
            RunwayRecord w{};
            nlohmann::json j = {{"raw", hex(data, size)}};
            if (read(w, data, size)) {
                j.update({{"lat", w.lat}, {"lon", w.lon}, {"alt", w.alt}, {"heading", w.heading}, {"length", w.length},
                          {"width", w.width}, {"primary_number", w.primaryNumber},
                          {"primary_designator", w.primaryDesignator}, {"secondary_number", w.secondaryNumber},
                          {"secondary_designator", w.secondaryDesignator}});
            }
            f["runways"].push_back(std::move(j));
            break;
        }
        case SIMCONNECT_FACILITY_DATA_TAXI_PARKING: {
            ParkingRecord p{};
            nlohmann::json j = {{"raw", hex(data, size)}};
            if (read(p, data, size)) {
                j.update({{"type", p.type}, {"name", p.name}, {"suffix", p.suffix}, {"number", p.number},
                          {"heading", p.heading}, {"radius", p.radius}, {"bias_x", p.biasX}, {"bias_z", p.biasZ}});
            }
            f["parkings"].push_back(std::move(j));
            break;
        }
        default:
            break;
    }
}

void SimProbe::onFacilityDataEnd(unsigned long requestId) {
    const auto it = facilities_.find(requestId);
    if (it == facilities_.end()) return;
    log_.write("probe_facility", it->second);
    facilities_.erase(it);
}

}  // namespace blocktime
