// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "plan_brief.h"

#include <charconv>
#include <cmath>

#include <nlohmann/json.hpp>

#include "sayintentions.h"
#include "simbrief.h"

namespace blocktime {

namespace {

using nlohmann::json;

// The object at a chain of keys, or nullptr.
const json* at(const json& root, std::initializer_list<const char*> path) {
    const json* j = &root;
    for (const char* key : path) {
        if (!j->is_object()) return nullptr;
        const auto it = j->find(key);
        if (it == j->end()) return nullptr;
        j = &*it;
    }
    return j;
}

// OFP values are strings ("112967"), but a number is accepted too.
std::string text(const json* j) {
    if (!j) return {};
    if (j->is_string()) return j->get<std::string>();
    if (j->is_number_integer()) return std::to_string(j->get<long long>());
    return {};
}

int number(const json* j) {
    const std::string s = text(j);
    int value = 0;
    std::from_chars(s.data(), s.data() + s.size(), value);
    return value;
}

}  // namespace

PlanBrief parsePlanBrief(std::string_view ofpJson) {
    const json root = json::parse(ofpJson, nullptr, false);
    PlanBrief b;
    if (!root.is_object()) return b;
    b.sid = text(at(root, {"general", "sid_ident"}));
    b.sidTransition = text(at(root, {"general", "sid_trans"}));
    b.zfwLb = number(at(root, {"weights", "est_zfw"}));
    b.cargoLb = number(at(root, {"weights", "cargo"}));
    b.surface = text(at(root, {"tlr", "takeoff", "conditions", "surface_condition"}));
    if (const json* runways = at(root, {"tlr", "takeoff", "runway"}); runways && runways->is_array()) {
        for (const json& r : *runways) {
            const std::string ident = normalizeRunway(text(at(r, {"identifier"})));
            if (!ident.empty()) b.runways.push_back({ident, number(at(r, {"magnetic_course"}))});
        }
    }
    b.star = text(at(root, {"general", "star_ident"}));
    b.starTransition = text(at(root, {"general", "star_trans"}));
    b.landingRunway = normalizeRunway(text(at(root, {"destination", "plan_rwy"})));
    b.reserveLb = number(at(root, {"fuel", "reserve"}));
    b.rampLb = number(at(root, {"fuel", "plan_ramp"}));
    b.taxiLb = number(at(root, {"fuel", "taxi"}));
    const auto duration = [&](const char* key) { return ofpSeconds(text(at(root, {"times", key}))).value_or(0); };
    b.enduranceS = duration("endurance");
    b.taxiOutS = duration("taxi_out");
    b.taxiInS = duration("taxi_in");
    b.enrouteS = duration("est_time_enroute");
    return b;
}

std::optional<int> fuelRemainingMinutes(int fuelLb, const PlanBrief& plan) {
    const int takeoffFuel = plan.rampLb - plan.taxiLb;
    if (plan.rampLb <= 0 || takeoffFuel <= 0 || plan.enduranceS <= 0) return std::nullopt;
    return static_cast<int>(std::lround(static_cast<double>(fuelLb) * plan.enduranceS / takeoffFuel / 60.0));
}

}  // namespace blocktime
