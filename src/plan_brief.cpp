// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "plan_brief.h"

#include <charconv>

#include <nlohmann/json.hpp>

#include "sayintentions.h"

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
    return b;
}

}  // namespace blocktime
