// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the two PMDG halves of the probe share (D-116). The FMC fields have the same names in PMDG_NG3_SDK.h and
// PMDG_777X_SDK.h, so one template reads them from either data struct.

#pragma once

#include <cmath>
#include <cstddef>
#include <string>

#include <nlohmann/json.hpp>

namespace blocktime::probe {

template <class T, std::size_t N>
nlohmann::json list(const T (&values)[N]) {
    nlohmann::json out = nlohmann::json::array();
    for (const T& v : values) out.push_back(static_cast<int>(v));
    return out;
}

// The FMC's own figures, for D-120 (can Live show the FMC's predictions?). The distances are rounded to whole nm,
// since the probe logs on every change: a fraction of a mile would log every second of the flight.
template <class Data>
nlohmann::json fmcFields(const Data& d) {
    std::size_t n = 0;
    while (n < sizeof d.FMC_flightNumber && d.FMC_flightNumber[n] != '\0') ++n;
    return {
        {"FMC_flightNumber", std::string(d.FMC_flightNumber, n)},
        {"FMC_PerfInputComplete", d.FMC_PerfInputComplete},
        {"FMC_CruiseAlt", d.FMC_CruiseAlt},
        {"FMC_TransitionAlt", d.FMC_TransitionAlt},
        {"FMC_TransitionLevel", d.FMC_TransitionLevel},
        {"FMC_TakeoffFlaps", d.FMC_TakeoffFlaps},
        {"FMC_V1", d.FMC_V1},
        {"FMC_VR", d.FMC_VR},
        {"FMC_V2", d.FMC_V2},
        {"FMC_LandingFlaps", d.FMC_LandingFlaps},
        {"FMC_LandingVREF", d.FMC_LandingVREF},
        {"FMC_LandingAltitude", d.FMC_LandingAltitude},
        {"FMC_DistanceToTOD_nm", std::lround(d.FMC_DistanceToTOD)},
        {"FMC_DistanceToDest_nm", std::lround(d.FMC_DistanceToDest)},
    };
}

}  // namespace blocktime::probe
