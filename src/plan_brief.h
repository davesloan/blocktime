// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What the Pre-Flight panel needs from the SimBrief plan beyond SimbriefPlan's columns (D-153, D-188): the SID with
// its transition, ZFW, cargo and the takeoff report's runways. Read from the stored OFP when the view loads, so no
// migration: the OFP is kept verbatim (D-078's exception), as the navlog is (D-182).

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace blocktime {

struct PlanRunway {
    std::string ident;  // normalized, "27"
    int magneticCourseDeg = 0;
};

struct PlanBrief {
    std::string sid, sidTransition;  // general.sid_ident "PADRZ2", general.sid_trans "IKAYE"
    int zfwLb = 0;                   // weights.est_zfw
    int cargoLb = 0;                 // weights.cargo: bags and freight together (2,420 = 44 bags x 55 lb)
    std::vector<PlanRunway> runways; // tlr.takeoff.runway[]: identifier, magnetic_course
    std::string surface;             // tlr.takeoff.conditions.surface_condition, "dry"; "" without a TLR
    std::string star, starTransition;  // general.star_ident "SILCN6", general.star_trans "TROXX"
    std::string landingRunway;       // destination.plan_rwy, normalized "30L"
    int reserveLb = 0;               // fuel.reserve: the fuel gauge's red arc
    int rampLb = 0, taxiLb = 0;      // fuel.plan_ramp, fuel.taxi: the takeoff fuel ENDURANCE is for
    int enduranceS = 0;              // times.endurance: seconds, or "HH:MM:SS" as Dave's stored OFPs have it
    // times.taxi_out, taxi_in, est_time_enroute: the predictions' fallback taxi times and the plan's enroute time
    int taxiOutS = 0, taxiInS = 0, enrouteS = 0;
};

// Missing fields stay empty or 0; anything that isn't JSON gives an empty brief.
PlanBrief parsePlanBrief(std::string_view ofpJson);

// Fuel on board at the plan's own burn: fuelLb × enduranceS / (rampLb − taxiLb), in whole minutes (D-203). nullopt
// when the plan lacks any of the three.
std::optional<int> fuelRemainingMinutes(int fuelLb, const PlanBrief& plan);

}  // namespace blocktime
