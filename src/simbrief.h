// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SimBrief (docs/notes/simbrief-api.md): the pre-filled dispatch page (D-070), the OFP fetch URL, reading an OFP
// (JSON v2), and the check that an imported plan belongs to the planned flight (D-101, D-105). No network here: the
// app fetches, this parses.

#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace blocktime {

struct DispatchRequest {
    std::string origin, destination, airlineIcao, flightNumber, airframeId;
    std::string originRunway, destinationRunway;  // "" = leave out, SimBrief chooses
    int passengers = 0;
    int cargoLb = 0;  // freight on top of the bags SimBrief adds itself
};

// https://dispatch.simbrief.com/options/custom?orig=..&dest=..&airline=..&fltnum=..&type=..&pax=..&cargo=..
// &units=LBS[&origrwy=..][&destrwy=..]. `cargo` is in thousands of pounds (checked by Dave, 2026-11-23).
std::string dispatchUrl(const DispatchRequest& request);

std::string ofpFetchUrl(std::string_view userId);

struct SimbriefPlan {
    std::string requestId, generatedAt /* UTC instant */, originIcao, destinationIcao, airframeId, units, route,
        originRunway, destinationRunway;
    int cruiseAltitudeFt = 0, estBlockS = 0, rampFuelLb = 0, enrouteBurnLb = 0, estLandingWeightLb = 0,
        originTransitionAltitudeFt = 0, destinationTransitionLevelFt = 0;
};

// Reads the fields BlockTime uses. On failure, *error is SimBrief's own status ("Error: Unknown UserID"), "SimBrief
// plan has no <path>", or "SimBrief's answer wasn't a plan".
std::optional<SimbriefPlan> parseOfp(std::string_view json, std::string* error);

struct PlanKey {
    std::string origin, destination, airframeId, registration, plannedAt /* UTC instant */;
};

// The route as Navigraph Charts takes it: "KBFI/14R <route> KSAN/27". An airport without a planned runway is the
// bare ICAO code.
std::string routeWithRunways(const SimbriefPlan& plan);

// nullopt when the plan belongs to the planned flight, else why not (D-105).
std::optional<std::string> planMismatch(const SimbriefPlan& plan, const PlanKey& flight);

}  // namespace blocktime
