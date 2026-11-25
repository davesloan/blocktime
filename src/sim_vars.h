// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one place that lists the SimVars the recorder reads. The struct layout MUST match the order of
// kTelemetryVars exactly: SimConnect writes the values back to back with no padding, hence pack(1).
// See docs/notes/simconnect-setup.md.

#pragma once

#include <cstddef>
#include <cstdint>

namespace blocktime {

enum class VarType { Float64, Int32, String256, String64 };

struct SimVarSpec {
    const char* name;
    const char* units;  // nullptr for strings
    VarType type;
    float epsilon;      // change threshold used with SIMCONNECT_DATA_REQUEST_FLAG_CHANGED (D-023)
};

#pragma pack(push, 1)
struct TelemetrySample {
    std::int32_t parkingBrake;   // BRAKE PARKING POSITION, bool (0/1)
    std::int32_t onGround;       // SIM ON GROUND, bool (0/1)
    double fuelTotalLb;          // FUEL TOTAL QUANTITY WEIGHT, pounds
    double fuelTotalGal;         // FUEL TOTAL QUANTITY, gallons
    double latitudeDeg;          // PLANE LATITUDE, degrees
    double longitudeDeg;         // PLANE LONGITUDE, degrees
    double altitudeFt;           // PLANE ALTITUDE (true MSL), feet
    double altitudeAglFt;        // PLANE ALT ABOVE GROUND, feet
    double indicatedAltitudeFt;  // INDICATED ALTITUDE, feet
    double verticalSpeedFpm;     // VERTICAL SPEED, feet per minute
    double groundSpeedKt;        // GROUND VELOCITY, knots
    double indicatedAirspeedKt;  // AIRSPEED INDICATED, knots
    double headingTrueDeg;       // PLANE HEADING DEGREES TRUE, degrees (logged only, D-129)
    std::int32_t numEngines;     // NUMBER OF ENGINES, number
    std::int32_t engCombustion[4];   // GENERAL ENG COMBUSTION:1..4, bool
    double engFuelFlowPph[4];        // ENG FUEL FLOW PPH:1..4, pounds per hour
    std::int32_t lightBeacon;        // LIGHT BEACON, bool: the shutdown checklist (D-135)
    std::int32_t lightStrobe;        // LIGHT STROBE, bool
    double mach;                     // AIRSPEED MACH, mach: Live's airspeed tile at altitude (D-140)
    double flapsTrailingPct;         // TRAILING EDGE FLAPS LEFT PERCENT, percent: Approach (D-142, D-148)
    std::int32_t flapsHandleIndex;   // FLAPS HANDLE INDEX, number: logged to learn whether it follows PMDG's lever
    // The debrief's cockpit state (D-159), carried by the `cockpit` event, not the telemetry line (cockpit_monitor.h).
    std::int32_t lightLanding;       // LIGHT LANDING, bool: the 777's and the 737's fixed landing lights
    std::int32_t seatBeltSign;       // CABIN SEATBELTS ALERT SWITCH, bool: follows the 777's switch, not the 737's
    double gearExtended;             // GEAR TOTAL PCT EXTENDED, "percent" unit but reads 0 to 1 (1 = down)
    char title[256];            // TITLE, string256
    char atcId[64];              // ATC ID, string64 (tail number)
};
#pragma pack(pop)

inline constexpr SimVarSpec kTelemetryVars[] = {
    {"BRAKE PARKING POSITION", "bool", VarType::Int32, 0.0f},
    {"SIM ON GROUND", "bool", VarType::Int32, 0.0f},
    {"FUEL TOTAL QUANTITY WEIGHT", "pounds", VarType::Float64, 1.0f},
    {"FUEL TOTAL QUANTITY", "gallons", VarType::Float64, 0.1f},
    {"PLANE LATITUDE", "degrees", VarType::Float64, 0.00001f},  // ~1 m
    {"PLANE LONGITUDE", "degrees", VarType::Float64, 0.00001f},
    {"PLANE ALTITUDE", "feet", VarType::Float64, 1.0f},
    {"PLANE ALT ABOVE GROUND", "feet", VarType::Float64, 1.0f},
    {"INDICATED ALTITUDE", "feet", VarType::Float64, 1.0f},
    {"VERTICAL SPEED", "feet per minute", VarType::Float64, 10.0f},
    {"GROUND VELOCITY", "knots", VarType::Float64, 0.5f},
    {"AIRSPEED INDICATED", "knots", VarType::Float64, 0.5f},
    {"PLANE HEADING DEGREES TRUE", "degrees", VarType::Float64, 0.5f},
    {"NUMBER OF ENGINES", "number", VarType::Int32, 0.0f},
    {"GENERAL ENG COMBUSTION:1", "bool", VarType::Int32, 0.0f},
    {"GENERAL ENG COMBUSTION:2", "bool", VarType::Int32, 0.0f},
    {"GENERAL ENG COMBUSTION:3", "bool", VarType::Int32, 0.0f},
    {"GENERAL ENG COMBUSTION:4", "bool", VarType::Int32, 0.0f},
    {"ENG FUEL FLOW PPH:1", "pounds per hour", VarType::Float64, 5.0f},
    {"ENG FUEL FLOW PPH:2", "pounds per hour", VarType::Float64, 5.0f},
    {"ENG FUEL FLOW PPH:3", "pounds per hour", VarType::Float64, 5.0f},
    {"ENG FUEL FLOW PPH:4", "pounds per hour", VarType::Float64, 5.0f},
    // Both follow the PMDG 737's and 777's switches (probe flights, docs/notes/probe-flight.md).
    {"LIGHT BEACON", "bool", VarType::Int32, 0.0f},
    {"LIGHT STROBE", "bool", VarType::Int32, 0.0f},
    {"AIRSPEED MACH", "mach", VarType::Float64, 0.002f},
    {"TRAILING EDGE FLAPS LEFT PERCENT", "percent", VarType::Float64, 0.5f},
    {"FLAPS HANDLE INDEX", "number", VarType::Int32, 0.0f},
    {"LIGHT LANDING", "bool", VarType::Int32, 0.0f},
    {"CABIN SEATBELTS ALERT SWITCH", "bool", VarType::Int32, 0.0f},
    {"GEAR TOTAL PCT EXTENDED", "percent", VarType::Float64, 0.01f},  // 0 to 1, so a fine epsilon
    {"TITLE", nullptr, VarType::String256, 0.0f},
    {"ATC ID", nullptr, VarType::String64, 0.0f},
};

// Compile-time guard that the struct and the list agree in size.
constexpr std::size_t telemetryVarsSize() {
    std::size_t n = 0;
    for (const auto& v : kTelemetryVars) {
        switch (v.type) {
            case VarType::Float64: n += 8; break;
            case VarType::Int32: n += 4; break;
            case VarType::String256: n += 256; break;
            case VarType::String64: n += 64; break;
        }
    }
    return n;
}
static_assert(telemetryVarsSize() == sizeof(TelemetrySample),
              "TelemetrySample layout does not match kTelemetryVars");

}  // namespace blocktime
