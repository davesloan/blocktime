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
#include <vector>

namespace blocktime {

struct DispatchRequest {
    std::string origin, destination, airlineIcao, flightNumber, airframeId;
    std::string originRunway, destinationRunway;  // "" = leave out, SimBrief chooses
    int passengers = 0;
    int cargoLb = 0;  // freight on top of the bags SimBrief adds itself
    int additionalFuelLb = 0;  // extra fuel; 0 = none sent
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
    // The loads SimBrief planned, for the import check (review 4.7): weights.pax_count, weights.freight_added (freight
    // without bags; weights.cargo includes them) and general.flight_number. 0 / "" when missing. Not stored.
    int passengers = 0, freightLb = 0;
    std::string flightNumber;
    int extraFuelLb = 0;  // fuel_extra.bucket's EXTRA entry; 0 when missing
};

// A point on the planned route, in degrees.
struct RoutePoint {
    double lat = 0, lon = 0;
};

// A navlog fix with the plan's times, fuel and speed there, for the predictions (D-192, D-212).
struct PlanFix {
    RoutePoint at;
    std::string ident;  // "TOC", "TOD", "KSJC"
    int elapsedS = -1;  // time_total: from takeoff; -1 = missing
    int fuelLb = -1;    // fuel_plan_onboard; -1 = missing
    int iasKt = 0;      // ind_airspeed; 0 = missing
    std::string stage;  // "CLB", "CRZ", "DSC"
};

// The navlog's fixes with their plan values: the same fixes, in the same order, as parseNavlog.
std::vector<PlanFix> parsePlanFixes(std::string_view ofpJson);

// An OFP duration: seconds ("9660") or "02:41:00", as Dave's stored OFPs have it; nullopt when it can't be read.
std::optional<int> ofpSeconds(std::string_view s);

// The fixes of a plan's navlog, in order (D-182). Dave's stored plans have `navlog` as an array of fixes; `{"fix":
// [...]}` is read too. Fixes whose position doesn't parse are skipped; no navlog or bad JSON gives none.
std::vector<RoutePoint> parseNavlog(std::string_view ofpJson);

// Reads the fields BlockTime uses. On failure, *error is SimBrief's own status ("Error: Unknown UserID"), "SimBrief
// plan has no <path>", or "SimBrief's answer wasn't a plan".
std::optional<SimbriefPlan> parseOfp(std::string_view json, std::string* error);

struct PlanKey {
    std::string origin, destination, airframeId, registration, plannedAt /* UTC instant */;
};

// The route as Navigraph Charts takes it: "KBFI/14R <route> KSAN/27". An airport without a planned runway is the
// bare ICAO code.
std::string routeWithRunways(const SimbriefPlan& plan);

struct ExpectedLoad {
    int passengers = 0;
    int cargoLb = 0;
    std::string flightNumber;
};

// nullopt when SimBrief planned this flight's loads and flight number, else what to regenerate (review 4.7):
// passengers exactly, cargo within max(100 lb, 1% of the flight's cargo) (D-251). Run after planMismatch.
std::optional<std::string> loadMismatch(const SimbriefPlan& plan, const ExpectedLoad& flight);

// nullopt when the plan belongs to the planned flight, else why not (D-105).
std::optional<std::string> planMismatch(const SimbriefPlan& plan, const PlanKey& flight);

}  // namespace blocktime
