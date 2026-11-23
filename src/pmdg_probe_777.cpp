// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The PMDG 777 half of the probe (D-116). Built only when BLOCKTIME_PMDG_777X_SDK_DIR points at PMDG_777X_SDK.h.

#include "pmdg_probe.h"

#include <cstring>

#include <windows.h>

#include <SimConnect.h>

#include <PMDG_777X_SDK.h>

namespace blocktime::probe {

namespace {

template <class T, std::size_t N>
nlohmann::json list(const T (&values)[N]) {
    nlohmann::json out = nlohmann::json::array();
    for (const T& v : values) out.push_back(static_cast<int>(v));
    return out;
}

}  // namespace

bool registerPmdg777(void* handle, unsigned long requestId) {
    HANDLE h = static_cast<HANDLE>(handle);
    return SUCCEEDED(SimConnect_MapClientDataNameToID(h, PMDG_777X_DATA_NAME, PMDG_777X_DATA_ID)) &&
           SUCCEEDED(SimConnect_AddToClientDataDefinition(h, PMDG_777X_DATA_DEFINITION, 0, sizeof(PMDG_777X_Data), 0,
                                                          0)) &&
           SUCCEEDED(SimConnect_RequestClientData(h, PMDG_777X_DATA_ID, requestId, PMDG_777X_DATA_DEFINITION,
                                                  SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET,
                                                  SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0));
}

nlohmann::json decodePmdg777(const void* data, std::size_t size) {
    if (size < sizeof(PMDG_777X_Data)) return nullptr;
    PMDG_777X_Data d;
    std::memcpy(&d, data, sizeof d);
    return {
        {"ELEC_Battery_Sw_ON", d.ELEC_Battery_Sw_ON},
        {"ELEC_APU_Selector", d.ELEC_APU_Selector},
        {"SIGNS_SeatBeltsSelector", d.SIGNS_SeatBeltsSelector},
        {"LTS_LandingLights_Sw_ON", list(d.LTS_LandingLights_Sw_ON)},
        {"LTS_Beacon_Sw_ON", d.LTS_Beacon_Sw_ON},
        {"LTS_NAV_Sw_ON", d.LTS_NAV_Sw_ON},
        {"LTS_Logo_Sw_ON", d.LTS_Logo_Sw_ON},
        {"LTS_Wing_Sw_ON", d.LTS_Wing_Sw_ON},
        {"LTS_RunwayTurnoff_Sw_ON", list(d.LTS_RunwayTurnoff_Sw_ON)},
        {"LTS_Taxi_Sw_ON", d.LTS_Taxi_Sw_ON},
        {"LTS_Strobe_Sw_ON", d.LTS_Strobe_Sw_ON},
        {"ICE_WingAntiIceSw", d.ICE_WingAntiIceSw},
        {"ICE_EngAntiIceSw", list(d.ICE_EngAntiIceSw)},
        {"GEAR_Lever", d.GEAR_Lever},
        {"MCP_annunAP", list(d.MCP_annunAP)},
        {"MCP_annunAT", d.MCP_annunAT},
        {"MCP_annunLNAV", d.MCP_annunLNAV},
        {"MCP_annunVNAV", d.MCP_annunVNAV},
        {"BRAKES_ParkingBrakeLeverOn", d.BRAKES_ParkingBrakeLeverOn},
        {"DOOR_state", list(d.DOOR_state)},
    };
}

}  // namespace blocktime::probe
