// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "preflight_log.h"

#include <algorithm>
#include <cctype>
#include <format>

#include "atc_message.h"
#include "departure_conditions.h"
#include "event_log.h"
#include "sayintentions.h"

namespace blocktime {

namespace {

std::string field(const nlohmann::json& data, const char* key) {
    const auto it = data.find(key);
    return it != data.end() && it->is_string() ? it->get<std::string>() : std::string();
}

// The ATIS's departing runway: the plan's when the list has it ("30L,30R" with a plan of 30R keeps 30R, D-189), else
// the first ("30L").
std::string atisRunway(const std::string& list, const std::string& planRunway) {
    std::string first;
    for (std::size_t start = 0; start <= list.size();) {
        const std::size_t comma = std::min(list.find(',', start), list.size());
        const std::string runway = normalizeRunway(list.substr(start, comma - start));
        if (!runway.empty() && runway == planRunway) return runway;
        if (first.empty()) first = runway;
        start = comma + 1;
    }
    return first;
}

}  // namespace

void PreFlightLog::setFlight(std::string originIcao, std::string plannedAt) {
    if (originIcao == origin_ && plannedAt == plannedAt_) return;
    origin_ = std::move(originIcao);
    plannedAt_ = std::move(plannedAt);
    plannedTime_ = parseUtc(plannedAt_);
    reset();
}

void PreFlightLog::reset() {
    clearance_.reset();
    clearedAt_.reset();
    taxi_.reset();
    taxiAt_.reset();
    atis_.reset();
    blockedOut_ = airborne_ = false;
}

void PreFlightLog::onEvent(std::string_view type, const nlohmann::json& data) {
    if (type == "block_in" || type == "flight_interrupted") {
        reset();
    } else if (type == "block_out") {
        blockedOut_ = true;
    } else if (type == "liftoff") {
        airborne_ = true;
    } else if (type == "atis") {
        if (data.is_object() && !origin_.empty() && field(data, "icao") == origin_) atis_ = data;
    } else if (type == "atc_exchange") {
        const auto m = atcMessage(data);
        if (!m || origin_.empty() || m->airport != origin_) return;
        const auto at = parseUtc(m->time);
        if (!at || (plannedTime_ && *at < *plannedTime_)) return;
        // The latest by ATC's own time wins; the same exchange logged again (a late reply, block out's re-log) is equal.
        if (auto c = parseClearance(m->atc); c && (!clearedAt_ || *at >= *clearedAt_)) {
            clearance_ = std::move(c);
            clearedAt_ = at;
        }
        if (airborne_) return;
        if (auto t = parseTaxi(m->atc); t && (!taxiAt_ || *at >= *taxiAt_)) {
            taxi_ = std::move(t);
            taxiAt_ = at;
        }
    }
}

std::string PreFlightLog::atcRunway() const {
    if (taxi_ && !taxi_->runway.empty()) return taxi_->runway;
    return clearance_ ? clearance_->runway : std::string();
}

DepartureView departureView(const PlannedFlightView& flight, const PreFlightLog& log) {
    DepartureView d;
    d.planRunway = normalizeRunway(flight.plan ? flight.plan->originRunway : "");
    if (d.planRunway.empty()) d.planRunway = normalizeRunway(flight.originRunway);
    const nlohmann::json* atis = log.atis();
    d.runway = log.atcRunway();
    if (d.runway.empty() && atis) d.runway = atisRunway(field(*atis, "departing"), d.planRunway);
    if (d.runway.empty()) d.runway = d.planRunway;
    d.runwayChanged = !d.runway.empty() && !d.planRunway.empty() && d.runway != d.planRunway;

    if (flight.brief) {
        const auto& runways = flight.brief->runways;
        const auto it = std::find_if(runways.begin(), runways.end(),
                                     [&](const PlanRunway& r) { return r.ident == d.runway; });
        if (it != runways.end()) d.headingDeg = it->magneticCourseDeg;
        if (!flight.brief->surface.empty()) {
            d.condition = flight.brief->surface;
            d.condition[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(d.condition[0])));
        }
    }
    if (!atis) return d;
    const std::string metar = field(*atis, "metar");
    if (metarPrecipitation(metar)) d.condition = "Wet";
    auto wind = atisWind(field(*atis, "text"));
    if (!wind) wind = metarWind(metar);
    if (wind) d.wind = windText(*wind);
    if (const auto t = metarTemperatureC(metar)) d.temperature = std::format("{} °C", *t);
    d.altimeter = altimeterText(field(*atis, "altimeter"));
    d.atisLetter = field(*atis, "letter");
    return d;
}

ClearanceView clearanceView(const PlannedFlightView& flight, const PreFlightLog& log) {
    ClearanceView c;
    const std::string planSid = flight.brief ? flight.brief->sid : std::string();
    if (!planSid.empty()) {
        c.sid = planSid;
        if (!flight.brief->sidTransition.empty()) c.sid += " " + flight.brief->sidTransition;
    }
    if (log.taxi()) c.taxiRoute = log.taxi()->route;
    const auto& clearance = log.clearance();
    if (!clearance) return c;
    c.received = true;
    if (!clearance->sid.empty() && clearance->sid != planSid) {
        c.sid = clearance->sid;
        c.sidChanged = true;
    }
    c.initialAltitude = clearance->initialAltitude;
    c.squawk = clearance->squawk;
    c.departureFrequency = clearance->departureFrequency;
    return c;
}

}  // namespace blocktime
