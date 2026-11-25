// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The PMDG 737 half of the probe (D-116). Built only when BLOCKTIME_PMDG_NG3_SDK_DIR points at PMDG_NG3_SDK.h.

#include "pmdg_probe.h"
#include "pmdg_probe_common.h"

#include <cmath>
#include <cstring>

#include <windows.h>

#include <SimConnect.h>

#include <PMDG_NG3_SDK.h>

namespace blocktime::probe {

bool registerPmdg737(void* handle, unsigned long requestId) {
    HANDLE h = static_cast<HANDLE>(handle);
    return SUCCEEDED(SimConnect_MapClientDataNameToID(h, PMDG_NG3_DATA_NAME, PMDG_NG3_DATA_ID)) &&
           SUCCEEDED(SimConnect_AddToClientDataDefinition(h, PMDG_NG3_DATA_DEFINITION, 0, sizeof(PMDG_NG3_Data), 0, 0)) &&
           SUCCEEDED(SimConnect_RequestClientData(h, PMDG_NG3_DATA_ID, requestId, PMDG_NG3_DATA_DEFINITION,
                                                  SIMCONNECT_CLIENT_DATA_PERIOD_ON_SET,
                                                  SIMCONNECT_CLIENT_DATA_REQUEST_FLAG_CHANGED, 0, 0, 0));
}

nlohmann::json decodePmdg737(const void* data, std::size_t size) {
    if (size < sizeof(PMDG_NG3_Data)) return nullptr;
    PMDG_NG3_Data d;
    std::memcpy(&d, data, sizeof d);
    nlohmann::json out = {
        {"AircraftModel", d.AircraftModel},
        {"ELEC_BatSelector", d.ELEC_BatSelector},
        {"COMM_FastenBeltsSelector", d.COMM_FastenBeltsSelector},
        {"APU_Selector", d.APU_Selector},
        // Rounded to 10°: whole degrees logged a line every second the APU ran (2026-11-23 probe flights).
        {"APU_EGTNeedle_10", std::lround(d.APU_EGTNeedle / 10.0) * 10},
        {"APU_annunMAINT", d.APU_annunMAINT},
        {"APU_annunLOW_OIL_PRESSURE", d.APU_annunLOW_OIL_PRESSURE},
        {"APU_annunFAULT", d.APU_annunFAULT},
        {"WARN_annunAPU", d.WARN_annunAPU},
        {"LTS_LandingLtRetractableSw", list(d.LTS_LandingLtRetractableSw)},
        {"LTS_LandingLtFixedSw", list(d.LTS_LandingLtFixedSw)},
        {"LTS_RunwayTurnoffSw", list(d.LTS_RunwayTurnoffSw)},
        {"LTS_TaxiSw", d.LTS_TaxiSw},
        {"LTS_LogoSw", d.LTS_LogoSw},
        {"LTS_PositionSw", d.LTS_PositionSw},
        {"LTS_AntiCollisionSw", d.LTS_AntiCollisionSw},
        {"LTS_WingSw", d.LTS_WingSw},
        {"LTS_WheelWellSw", d.LTS_WheelWellSw},
        {"ICE_WindowHeatSw", list(d.ICE_WindowHeatSw)},
        {"ICE_ProbeHeatSw", list(d.ICE_ProbeHeatSw)},
        {"ICE_WingAntiIceSw", d.ICE_WingAntiIceSw},
        {"ICE_EngAntiIceSw", list(d.ICE_EngAntiIceSw)},
        {"DOOR_annun", {d.DOOR_annunFWD_ENTRY, d.DOOR_annunFWD_SERVICE, d.DOOR_annunAIRSTAIR,
                        d.DOOR_annunLEFT_FWD_OVERWING, d.DOOR_annunRIGHT_FWD_OVERWING, d.DOOR_annunFWD_CARGO,
                        d.DOOR_annunEQUIP, d.DOOR_annunLEFT_AFT_OVERWING, d.DOOR_annunRIGHT_AFT_OVERWING,
                        d.DOOR_annunAFT_CARGO, d.DOOR_annunAFT_ENTRY, d.DOOR_annunAFT_SERVICE}},
        {"MCP_annunCMD_A", d.MCP_annunCMD_A},
        {"MCP_annunCMD_B", d.MCP_annunCMD_B},
        {"MCP_annunATArm", d.MCP_annunATArm},
        {"MCP_annunLNAV", d.MCP_annunLNAV},
        {"MCP_annunVNAV", d.MCP_annunVNAV},
        {"MAIN_annunGEAR_transit", list(d.MAIN_annunGEAR_transit)},
        {"MAIN_annunGEAR_locked", list(d.MAIN_annunGEAR_locked)},
        {"PED_annunParkingBrake", d.PED_annunParkingBrake},
        {"EFIS_BaroSelHPA", list(d.EFIS_BaroSelHPA)},
    };
    out.update(fmcFields(d));
    return out;
}

std::optional<PmdgCockpit> decodeCockpit737(const void* data, std::size_t size) {
    if (size < sizeof(PMDG_NG3_Data)) return std::nullopt;
    PMDG_NG3_Data d;
    std::memcpy(&d, data, sizeof d);
    PmdgCockpit c;
    c.seatBelts = d.COMM_FastenBeltsSelector;  // 0 off, 1 auto, 2 on
    // Fixed lights: 0 off, 1 on. Retractable: 0 retract, 1 extend, 2 on (PMDG_NG3_SDK.h).
    for (const auto v : d.LTS_LandingLtFixedSw) c.landingLights |= v != 0;
    for (const auto v : d.LTS_LandingLtRetractableSw) c.landingLights |= v == 2;
    c.vrefKt = d.FMC_LandingVREF;
    c.landingFlaps = d.FMC_LandingFlaps;
    c.autopilot = d.MCP_annunCMD_A || d.MCP_annunCMD_B;
    return c;
}

}  // namespace blocktime::probe
