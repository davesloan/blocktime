// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The PMDG 777 half of the probe (D-116). Built only when BLOCKTIME_PMDG_777X_SDK_DIR points at PMDG_777X_SDK.h.

#include "pmdg_probe.h"
#include "pmdg_probe_common.h"

#include <cmath>
#include <cstring>

#include <windows.h>

#include <SimConnect.h>

#include <PMDG_777X_SDK.h>

namespace blocktime::probe {

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
    nlohmann::json out = {
        {"AircraftModel", d.AircraftModel},
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
        {"ICE_WindowHeat_Sw_ON", list(d.ICE_WindowHeat_Sw_ON)},
        {"ICE_WingAntiIceSw", d.ICE_WingAntiIceSw},
        {"ICE_EngAntiIceSw", list(d.ICE_EngAntiIceSw)},
        {"GEAR_Lever", d.GEAR_Lever},
        {"MCP_annunAP", list(d.MCP_annunAP)},
        {"MCP_annunAT", d.MCP_annunAT},
        {"MCP_annunLNAV", d.MCP_annunLNAV},
        {"MCP_annunVNAV", d.MCP_annunVNAV},
        {"MCP_annunAPP", d.MCP_annunAPP},
        {"BRAKES_ParkingBrakeLeverOn", d.BRAKES_ParkingBrakeLeverOn},
        {"EFIS_BaroSelHPA", list(d.EFIS_BaroSelHPA)},
        {"EFIS_BaroSTD_Sw_Pushed", list(d.EFIS_BaroSTD_Sw_Pushed)},
        {"DOOR_state", list(d.DOOR_state)},
        {"APURunning", d.APURunning},
        {"WheelChocksSet", d.WheelChocksSet},
    };
    out.update(fmcFields(d));
    return out;
}

std::optional<PmdgCockpit> decodeCockpit777(const void* data, std::size_t size) {
    if (size < sizeof(PMDG_777X_Data)) return std::nullopt;
    PMDG_777X_Data d;
    std::memcpy(&d, data, sizeof d);
    PmdgCockpit c;
    c.seatBelts = d.SIGNS_SeatBeltsSelector;  // 0 off, 1 auto, 2 on (seen on the 2026-11-24 777 probe flight)
    for (const auto v : d.LTS_LandingLights_Sw_ON) c.landingLights |= v != 0;
    c.vrefKt = d.FMC_LandingVREF;
    c.landingFlaps = d.FMC_LandingFlaps;
    for (const auto v : d.MCP_annunAP) c.autopilot |= v != 0;
    c.apuSwitch = d.ELEC_APU_Selector != 0;  // 0 off, 1 on, 2 start
    c.apuRunning = d.APURunning;
    return c;
}

}  // namespace blocktime::probe
