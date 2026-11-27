// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "simbrief.h"

#include <charconv>
#include <chrono>
#include <format>
#include <sstream>

#include <nlohmann/json.hpp>

#include "url.h"

namespace blocktime {

namespace {

using nlohmann::json;

// "5.234" for 5,234 lb: thousands, exact to the pound, no trailing zeros.
std::string thousands(int lb) {
    std::string s = std::format("{}.{:03}", lb / 1000, lb % 1000);
    while (s.back() == '0') s.pop_back();
    if (s.back() == '.') s.pop_back();
    return s;
}

// A value at "a.b" as text; OFP values are strings, but a number is accepted too.
std::optional<std::string> text(const json& root, const char* section, const char* field) {
    const auto s = root.find(section);
    if (s == root.end() || !s->is_object()) return std::nullopt;
    const auto f = s->find(field);
    if (f == s->end()) return std::nullopt;
    if (f->is_string()) return f->get<std::string>();
    if (f->is_number_integer()) return std::to_string(f->get<long long>());
    return std::nullopt;
}

std::optional<int> number(std::string_view s) {
    int value = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || end != s.data() + s.size()) return std::nullopt;
    return value;
}

// "02:31:00" → 9060.
std::optional<int> hms(std::string_view s) {
    if (s.size() != 8 || s[2] != ':' || s[5] != ':') return std::nullopt;
    const auto h = number(s.substr(0, 2)), m = number(s.substr(3, 2)), sec = number(s.substr(6, 2));
    if (!h || !m || !sec) return std::nullopt;
    return *h * 3600 + *m * 60 + *sec;
}

std::optional<std::chrono::sys_seconds> instant(const std::string& s) {
    std::istringstream in(s);
    std::chrono::sys_seconds t;
    in >> std::chrono::parse("%Y-%m-%dT%H:%M:%SZ", t);
    if (in.fail()) return std::nullopt;
    return t;
}

// A navlog position: a string ("32.798008") or a number.
std::optional<double> degrees(const json& fix, const char* key) {
    const auto f = fix.find(key);
    if (f == fix.end()) return std::nullopt;
    if (f->is_number()) return f->get<double>();
    if (!f->is_string()) return std::nullopt;
    const std::string& s = f->get_ref<const std::string&>();
    double value = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc() || end != s.data() + s.size()) return std::nullopt;
    return value;
}

}  // namespace

std::string dispatchUrl(const DispatchRequest& r) {
    std::string url = "https://dispatch.simbrief.com/options/custom?orig=" + percentEncode(r.origin) +
                      "&dest=" + percentEncode(r.destination) + "&airline=" + percentEncode(r.airlineIcao) +
                      "&fltnum=" + percentEncode(r.flightNumber) + "&type=" + percentEncode(r.airframeId) +
                      "&pax=" + std::to_string(r.passengers) + "&cargo=" + thousands(r.cargoLb) + "&units=LBS";
    if (!r.originRunway.empty()) url += "&origrwy=" + percentEncode(r.originRunway);
    if (!r.destinationRunway.empty()) url += "&destrwy=" + percentEncode(r.destinationRunway);
    return url;
}

std::string ofpFetchUrl(std::string_view userId) {
    return "https://www.simbrief.com/api/xml.fetcher.php?userid=" + percentEncode(userId) + "&json=v2&client=BlockTime";
}

std::optional<int> ofpSeconds(std::string_view s) {
    if (s.find(':') == std::string_view::npos) return number(s);
    int total = 0;
    std::size_t start = 0;
    while (true) {
        const std::size_t colon = s.find(':', start);
        const auto part = number(s.substr(start, colon == std::string_view::npos ? std::string_view::npos : colon - start));
        if (!part) return std::nullopt;
        total = total * 60 + *part;
        if (colon == std::string_view::npos) return total;
        start = colon + 1;
    }
}

std::vector<PlanFix> parsePlanFixes(std::string_view ofpJson) {
    const json root = json::parse(ofpJson, nullptr, false);
    std::vector<PlanFix> out;
    if (!root.is_object()) return out;
    const auto nav = root.find("navlog");
    if (nav == root.end()) return out;
    const json* fixes = &*nav;
    if (nav->is_object()) {
        const auto f = nav->find("fix");
        if (f == nav->end()) return out;
        fixes = &*f;
    }
    if (!fixes->is_array()) return out;
    // A fix's value as text: OFP values are strings, but a number is accepted too.
    const auto field = [](const json& fix, const char* key) -> std::string {
        const auto f = fix.find(key);
        if (f == fix.end()) return {};
        if (f->is_string()) return f->get<std::string>();
        if (f->is_number_integer()) return std::to_string(f->get<long long>());
        return {};
    };
    for (const json& fix : *fixes) {
        if (!fix.is_object()) continue;
        const auto lat = degrees(fix, "pos_lat"), lon = degrees(fix, "pos_long");
        if (!lat || !lon) continue;
        PlanFix p;
        p.at = {*lat, *lon};
        p.ident = field(fix, "ident");
        p.stage = field(fix, "stage");
        p.elapsedS = ofpSeconds(field(fix, "time_total")).value_or(-1);
        p.fuelLb = number(field(fix, "fuel_plan_onboard")).value_or(-1);
        p.iasKt = number(field(fix, "ind_airspeed")).value_or(0);
        out.push_back(std::move(p));
    }
    return out;
}

std::vector<RoutePoint> parseNavlog(std::string_view ofpJson) {
    std::vector<RoutePoint> out;
    for (const PlanFix& f : parsePlanFixes(ofpJson)) out.push_back(f.at);
    return out;
}

std::optional<SimbriefPlan> parseOfp(std::string_view body, std::string* error) {
    const auto fail = [&](std::string message) -> std::optional<SimbriefPlan> {
        if (error) *error = std::move(message);
        return std::nullopt;
    };
    const json root = json::parse(body, nullptr, false);
    if (root.is_discarded() || !root.is_object()) return fail("SimBrief's answer wasn't a plan");
    const auto status = text(root, "fetch", "status");
    if (status && *status != "Success") return fail(*status);

    SimbriefPlan p;
    struct Text {
        const char *section, *field;
        std::string* out;
    };
    for (const Text& t : {Text{"params", "request_id", &p.requestId}, Text{"params", "time_generated", &p.generatedAt},
                          Text{"params", "units", &p.units}, Text{"origin", "icao_code", &p.originIcao},
                          Text{"destination", "icao_code", &p.destinationIcao},
                          Text{"aircraft", "internal_id", &p.airframeId}, Text{"general", "route", &p.route},
                          Text{"origin", "plan_rwy", &p.originRunway},
                          Text{"destination", "plan_rwy", &p.destinationRunway}}) {
        const auto value = text(root, t.section, t.field);
        if (!value) return fail(std::format("SimBrief plan has no {}.{}", t.section, t.field));
        *t.out = *value;
    }
    struct Number {
        const char *section, *field;
        int* out;
    };
    for (const Number& n : {Number{"general", "initial_altitude", &p.cruiseAltitudeFt},
                            Number{"fuel", "plan_ramp", &p.rampFuelLb}, Number{"fuel", "enroute_burn", &p.enrouteBurnLb},
                            Number{"weights", "est_ldw", &p.estLandingWeightLb},
                            Number{"origin", "trans_alt", &p.originTransitionAltitudeFt},
                            Number{"destination", "trans_level", &p.destinationTransitionLevelFt}}) {
        const auto value = text(root, n.section, n.field);
        const auto parsed = value ? number(*value) : std::nullopt;
        if (!parsed) return fail(std::format("SimBrief plan has no {}.{}", n.section, n.field));
        *n.out = *parsed;
    }
    const auto block = text(root, "times", "est_block");
    const auto blockS = block ? hms(*block) : std::nullopt;
    if (!blockS) return fail("SimBrief plan has no times.est_block");
    p.estBlockS = *blockS;
    if (error) error->clear();
    return p;
}

std::string routeWithRunways(const SimbriefPlan& p) {
    const auto end = [](const std::string& icao, const std::string& runway) {
        return runway.empty() ? icao : icao + "/" + runway;
    };
    std::string out = end(p.originIcao, p.originRunway);
    if (!p.route.empty()) out += " " + p.route;
    return out + " " + end(p.destinationIcao, p.destinationRunway);
}

std::optional<std::string> planMismatch(const SimbriefPlan& plan, const PlanKey& flight) {
    if (plan.units != "lbs") return "The SimBrief plan is in kilograms; BlockTime needs pounds.";
    if (plan.originIcao != flight.origin || plan.destinationIcao != flight.destination) {
        return std::format("The latest SimBrief plan is {}–{}; this flight is {}–{}.", plan.originIcao,
                           plan.destinationIcao, flight.origin, flight.destination);
    }
    if (plan.airframeId != flight.airframeId) {
        return std::format("The latest SimBrief plan isn't for {}'s SimBrief airframe.", flight.registration);
    }
    const auto generated = instant(plan.generatedAt), planned = instant(flight.plannedAt);
    if (!generated || !planned || *generated < *planned - std::chrono::minutes(5)) {
        const std::string hhmm = plan.generatedAt.size() >= 16 ? plan.generatedAt.substr(11, 5) : plan.generatedAt;
        return std::format(
            "The latest SimBrief plan was generated at {}Z, before this flight was planned. Press Generate on "
            "SimBrief, then import again.",
            hhmm);
    }
    return std::nullopt;
}

}  // namespace blocktime
