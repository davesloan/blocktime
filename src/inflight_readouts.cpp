// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "inflight_readouts.h"

#include <format>

#include "departure_conditions.h"
#include "money.h"

namespace blocktime {

namespace {

std::string field(const nlohmann::json& j, const char* key) {
    return j.contains(key) && j[key].is_string() ? j[key].get<std::string>() : std::string();
}

ReadoutView readout(std::string title, std::string value, std::string unit = "", std::string tag = "") {
    ReadoutView r;
    r.title = std::move(title);
    r.value = std::move(value);
    r.unit = std::move(unit);
    r.tag = std::move(tag);
    return r;
}

ReadoutView words(ReadoutView r) {
    r.words = true;
    return r;
}

// "30.09 inHg" → {"30.09", "inHg"}: the value and its unit, for a readout.
std::pair<std::string, std::string> valueAndUnit(const std::string& text) {
    const auto space = text.find(' ');
    if (space == std::string::npos) return {text, ""};
    return {text.substr(0, space), text.substr(space + 1)};
}

std::string firstWord(const std::string& s) { return s.substr(0, s.find(' ')); }

// The plan against ATC (as D-189 on Pre-Flight): ATC's when it gave one, else the fallback; flagged when it differs
// from the plan's.
ReadoutView planAgainst(std::string title, const std::string& atc, const std::string& fallback,
                        const char* fallbackTag, const std::string& plan, bool compareFirstWord) {
    const std::string shown = !atc.empty() ? atc : !fallback.empty() ? fallback : plan;
    if (shown.empty()) return readout(std::move(title), "");
    const auto key = [&](const std::string& s) { return compareFirstWord ? firstWord(s) : s; };
    const bool differs = !plan.empty() && key(shown) != key(plan);
    std::string tag = !atc.empty() ? "ATC" : !fallback.empty() ? fallbackTag : "PLAN";
    if (differs) tag = "PLAN " + key(plan);
    ReadoutView r = readout(std::move(title), shown, "", tag);
    r.flagged = differs;
    return r;
}

std::string landingRunway(const InFlightFacts& f, std::string* atisRunway) {
    *atisRunway = f.destinationAtis ? firstWord(runwayList(field(*f.destinationAtis, "arriving"))) : "";
    return f.plan.landingRunway;
}

ReadoutView landingRunwayReadout(const char* title, const InFlightFacts& f) {
    std::string atis;
    const std::string plan = landingRunway(f, &atis);
    return planAgainst(title, f.atcRunway, atis, "ATIS", plan, false);
}

std::string atisWindText(const nlohmann::json& atis) {
    auto wind = atisWind(field(atis, "text"));
    if (!wind) wind = metarWind(field(atis, "metar"));
    return wind ? windText(*wind) : "";
}

std::string cruiseText(int ft) {
    if (ft <= 0) return {};
    if (ft >= 18000) return std::format("FL{}", ft / 100);
    return formatNumber(ft);
}

std::vector<ReadoutView> emergency(const InFlightFacts& f) {
    const std::string crew = std::format("{} CREW", f.crew);
    ReadoutView souls = readout("SOULS", std::to_string(f.passengers + f.crew), "",
                                f.passengers > 0 ? std::format("{} PAX + {}", f.passengers, crew) : crew);
    ReadoutView fuel = readout("FUEL REMAINING", "", "H:MM");
    if (const auto minutes = fuelRemainingMinutes(f.fuelLb, f.plan)) {
        fuel.value = std::format("{}:{:02}", *minutes / 60, *minutes % 60);
    }
    fuel.ticks = true;
    return {souls, fuel};
}

void departure(const InFlightFacts& f, std::vector<ReadoutView>& out) {
    const DepartureView& d = f.departure;
    ReadoutView runway = readout("RUNWAY", d.runway);
    if (d.runwayChanged) {
        runway.tag = d.runwayTag;
        runway.flagged = true;
    } else if (!d.runway.empty()) {
        runway.tag = d.headingDeg ? std::format("{:03}° · {}", *d.headingDeg, d.runwayTag) : d.runwayTag;
    }
    out.push_back(runway);
    out.push_back(readout("SQUAWK", f.clearance.squawk));
    out.push_back(words(readout("WIND", d.wind, "", d.wind.empty() ? "" : "ATIS")));
    const auto [altimeter, unit] = valueAndUnit(d.altimeter);
    out.push_back(readout("ALTIMETER", altimeter, unit));
}

void arrival(const InFlightFacts& f, std::vector<ReadoutView>& out) {
    const nlohmann::json empty = nlohmann::json::object();
    const nlohmann::json& atis = f.destinationAtis ? *f.destinationAtis : empty;
    out.push_back(words(readout("ATIS", phoneticWord(field(atis, "letter")))));
    out.push_back(words(readout("RUNWAYS", runwayList(field(atis, "arriving")))));
    ReadoutView approaches = words(readout("APPROACHES", atisApproaches(field(atis, "text"))));
    approaches.wide = true;
    out.push_back(approaches);
    out.push_back(words(readout("WIND", f.destinationAtis ? atisWindText(atis) : "")));
    const auto temperature = metarTemperatureC(field(atis, "metar"));
    out.push_back(readout("OAT", temperature ? std::to_string(*temperature) : "", "°C"));
    const auto [altimeter, unit] = valueAndUnit(altimeterText(field(atis, "altimeter")));
    out.push_back(readout("ALTIMETER", altimeter, unit, altimeter.empty() ? "" : "SET AT TL"));
    std::string condition;
    if (f.destinationAtis) condition = metarPrecipitation(field(atis, "metar")) ? "WET" : "DRY";
    out.push_back(words(readout("CONDITION", condition)));
    std::string planStar = f.plan.star;
    if (!planStar.empty() && !f.plan.starTransition.empty()) planStar += " " + f.plan.starTransition;
    out.push_back(words(planAgainst("STAR", f.atcStar, "", "", planStar, true)));
    out.push_back(landingRunwayReadout("LANDING RWY", f));
}

void shortFinal(const InFlightFacts& f, std::vector<ReadoutView>& out) {
    if (!f.towerWind.empty()) {
        out.push_back(words(readout("WIND", f.towerWind, "", "TOWER")));
    } else {
        const std::string wind = f.destinationAtis ? atisWindText(*f.destinationAtis) : "";
        out.push_back(words(readout("WIND", wind, "", wind.empty() ? "" : "ATIS")));
    }
    out.push_back(landingRunwayReadout("RUNWAY", f));
    out.push_back(readout("VREF", f.vrefKt > 0 ? std::to_string(f.vrefKt) : "", "KT",
                          f.vrefKt > 0 && f.landingFlaps > 0 ? std::format("FLAPS {}", f.landingFlaps) : ""));
}

void landed(const InFlightFacts& f, std::vector<ReadoutView>& out) {
    ReadoutView landing = readout("LANDING", "");
    if (f.stars) {
        landing.stars = *f.stars;
        landing.tag = std::format("−{} FPM", f.touchdownFpm);
    }
    out.push_back(landing);
    std::string gate = f.gate;
    if (gate.starts_with("Gate ")) gate = gate.substr(5);
    out.push_back(readout("GATE", gate));
}

}  // namespace

PanelPhase panelPhase(std::string_view phase, bool onFinal, bool touchedDown) {
    if (touchedDown || phase == "landing" || phase == "taxi_in" || phase == "arrived") return PanelPhase::Landed;
    if (onFinal) return PanelPhase::ShortFinal;
    if (phase == "climb") return PanelPhase::Climb;
    if (phase == "cruise") return PanelPhase::Cruise;
    if (phase == "descent" || phase == "approach" || phase == "go_around") return PanelPhase::Arrival;
    return PanelPhase::Departure;  // at_gate, pushback, taxi_out, takeoff
}

std::vector<ReadoutView> inFlightReadouts(PanelPhase phase, const InFlightFacts& f) {
    std::vector<ReadoutView> out = emergency(f);
    switch (phase) {
        case PanelPhase::Departure: departure(f, out); break;
        case PanelPhase::Climb: out.push_back(readout("CRUISE", cruiseText(f.cruiseLevelFt))); break;
        case PanelPhase::Cruise: {
            out.push_back(readout("CRUISE", cruiseText(f.cruiseLevelFt)));
            ReadoutView dest = words(readout("DEST ATIS", ""));
            if (f.destinationAtis) {
                dest.value = phoneticWord(field(*f.destinationAtis, "letter"));
                dest.tag = runwayList(field(*f.destinationAtis, "arriving"));
            }
            out.push_back(dest);
            break;
        }
        case PanelPhase::Arrival: arrival(f, out); break;
        case PanelPhase::ShortFinal: shortFinal(f, out); break;
        case PanelPhase::Landed: landed(f, out); break;
    }
    return out;
}

}  // namespace blocktime
