// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_tracker.h"

#include <cmath>
#include <type_traits>
#include <variant>

#include "event_log.h"
#include "header_text.h"

namespace blocktime {

namespace {

double round1(double x) { return std::round(x * 10.0) / 10.0; }

bool descending(Phase p) { return p == Phase::Descent || p == Phase::Approach || p == Phase::GoAround; }

AltimeterSample rounded(const AltimeterSample& a) { return {round1(a.captMb), round1(a.foMb), a.captStd, a.foStd}; }

bool same(const AltimeterSample& a, const AltimeterSample& b) {
    return a.captMb == b.captMb && a.foMb == b.foMb && a.captStd == b.captStd && a.foStd == b.foStd;
}

}  // namespace

void FlightTracker::setAirport(AirportFacilities airport) {
    const std::string icao = airport.icao;
    airports_[icao] = std::move(airport);
}

std::string FlightTracker::parkingAt(double lat, double lon) const {
    std::vector<ParkingSpot> spots;
    for (const auto& [icao, a] : airports_) spots.insert(spots.end(), a.spots.begin(), a.spots.end());
    const auto s = nearestSpot(spots, lat, lon);
    return s ? spotName(*s) : "";
}

std::vector<AirportRunways> FlightTracker::runwayList() const {
    std::vector<AirportRunways> out;
    for (const auto& [icao, a] : airports_) out.push_back(AirportRunways{a.icao, a.runways});
    return out;
}

bool FlightTracker::flightLevel(Phase phase, int altFt) const {
    if (!detector_) return false;
    const PhasePlan& p = detector_->plan();
    return isFlightLevel(phaseCode(phase), altFt, p.transitionAltitudeFt, p.transitionLevelFt);
}

LogEvent FlightTracker::toLog(const PhaseEvent& e) const {
    return std::visit(
        [this](const auto& x) -> LogEvent {
            using T = std::decay_t<decltype(x)>;
            nlohmann::json d{{"flight_id", flightId_}, {"time", formatUtc(x.time)}};
            if constexpr (std::is_same_v<T, PhaseChanged>) {
                d["phase"] = phaseCode(x.phase);
                d["previous"] = phaseCode(x.previous);
                d["alt_ft"] = x.altFt;
                d["flight_level"] = flightLevel(x.phase, x.altFt);
                return {"phase_changed", d};
            } else if constexpr (std::is_same_v<T, LevelOff> || std::is_same_v<T, LevelOffEnded>) {
                d["phase"] = phaseCode(x.phase);
                d["alt_ft"] = x.altFt;
                d["flight_level"] = flightLevel(x.phase, x.altFt);
                return {std::is_same_v<T, LevelOff> ? "level_off" : "level_off_ended", d};
            } else if constexpr (std::is_same_v<T, StepClimb>) {
                d["from_ft"] = x.fromFt;
                d["flight_level"] = flightLevel(Phase::Cruise, x.fromFt);
                return {"step_climb", d};
            } else if constexpr (std::is_same_v<T, StepClimbEnded>) {
                d["alt_ft"] = x.altFt;
                d["flight_level"] = flightLevel(Phase::Cruise, x.altFt);
                return {"step_climb_ended", d};
            } else {
                d["marker"] = markerCode(x.kind);
                d["alt_ft"] = x.altFt;
                return {"flight_marker", d};
            }
        },
        e);
}

LogEvent FlightTracker::altimeterEvent(const AltimeterSample& a) const {
    return {"altimeter",
            {{"flight_id", flightId_},
             {"capt_mb", a.captMb},
             {"fo_mb", a.foMb},
             {"capt_std", a.captStd},
             {"fo_std", a.foStd}}};
}

std::vector<LogEvent> FlightTracker::onBlockOut(const BlockOut& out, const std::optional<FlightTarget>& target,
                                                double altFt) {
    onFlightEnded();
    PhasePlan plan;
    if (target) {
        plan.cruiseAltitudeFt = target->cruiseAltitudeFt;
        plan.transitionAltitudeFt = target->transitionAltitudeFt;
        plan.transitionLevelFt = target->transitionLevelFt;
        plan.hasDestination = target->destinationLat != 0 || target->destinationLon != 0;
        plan.destinationLat = target->destinationLat;
        plan.destinationLon = target->destinationLon;
    }
    flightId_ = out.flightId;
    destinationIcao_ = target ? target->destinationIcao : "";
    detector_.emplace(plan, out.time, altFt);
    std::vector<LogEvent> events{toLog(detector_->started())};
    if (altimeter_) {
        logged_ = rounded(*altimeter_);
        events.push_back(altimeterEvent(*logged_));
    }
    return events;
}

std::vector<LogEvent> FlightTracker::onSample(const PhaseSample& s) {
    std::vector<LogEvent> events;
    if (!detector_) return events;
    const auto phaseEvents = detector_->onSample(s);
    const Phase phase = detector_->phase();

    if (phase == Phase::Takeoff && s.onGround && s.groundSpeedKt >= kRunwayRollKt)
        takeoffRoll_.push_back({s.lat, s.lon});
    if (phase == Phase::Landing && s.onGround && s.groundSpeedKt >= kRunwayRollKt)
        landingRoll_.push_back({s.lat, s.lon});

    for (const auto& e : phaseEvents) {
        events.push_back(toLog(e));
        const auto* change = std::get_if<PhaseChanged>(&e);
        if (change && change->phase == Phase::TaxiIn) {
            const RunwayName n = nameRunway(runwayList(), landingRoll_);
            events.push_back({"landing_runway",
                              {{"flight_id", flightId_},
                               {"airport", n.icao},
                               {"runway", n.runway},
                               {"off_centreline_m", round1(n.offCentrelineM)},
                               {"off_heading_deg", round1(n.offHeadingDeg)}}});
            landingRoll_.clear();
        }
    }

    if (!liftoffLogged_ && !takeoffRoll_.empty() && !s.onGround) {
        const RunwayName n = nameRunway(runwayList(), takeoffRoll_);
        events.push_back({"liftoff",
                          {{"flight_id", flightId_},
                           {"time", formatUtc(s.time)},
                           {"lat", s.lat},
                           {"lon", s.lon},
                           {"ias_kt", std::round(s.iasKt)},
                           {"airport", n.icao},
                           {"runway", n.runway},
                           {"off_centreline_m", round1(n.offCentrelineM)},
                           {"off_heading_deg", round1(n.offHeadingDeg)}}});
        liftoffLogged_ = true;
        takeoffRoll_.clear();
    }

    // "Final approach": lined up with a destination runway (D-143, D-145). Built once the runways are known.
    if (!linedUp_) {
        const auto it = airports_.find(destinationIcao_);
        if (it != airports_.end() && !it->second.runways.empty()) linedUp_.emplace(it->second.runways);
    }
    if (linedUp_) {
        if (const auto l = linedUp_->onSample(s, phase)) {
            events.push_back({"lined_up",
                              {{"flight_id", flightId_},
                               {"time", formatUtc(l->time)},
                               {"airport", destinationIcao_},
                               {"runway", l->runway},
                               {"alt_ft", l->altFt},
                               {"distance_nm", l->distanceNm}}});
        }
    }

    // A rollout slowed to taxi before the bounce window ended: the count so far (D-158).
    if (!bouncesDone_ && detector_->phase() == Phase::TaxiIn) {
        if (const auto b = touchdown_.finishBounces()) events.push_back(bouncesEvent(s.time, *b));
    }
    updateFastSampling(s);
    return events;
}

LogEvent FlightTracker::bouncesEvent(TimePoint at, int bounces) {
    bouncesDone_ = true;
    fast_ = false;
    return {"bounces", {{"flight_id", flightId_}, {"time", formatUtc(at)}, {"bounces", bounces}}};
}

void FlightTracker::updateFastSampling(const PhaseSample& s) {
    const Phase phase = detector_->phase();
    if (bouncesDone_ || phase == Phase::TaxiIn) {
        fast_ = false;
    } else if (!fast_) {
        fast_ = descending(phase) && !s.onGround && s.aglFt < kTouchdownArmAglFt;
    } else if (!s.onGround && s.aglFt > kTouchdownDisarmAglFt) {
        fast_ = false;
    }
}

std::vector<LogEvent> FlightTracker::onFastSample(const FastSample& s) {
    if (!detector_ || !fast_) return {};
    std::vector<LogEvent> events;
    if (const auto t = touchdown_.onSample(s)) {
        events.push_back({"touchdown",
                          {{"flight_id", flightId_},
                           {"time", formatUtc(t->time)},
                           {"lat", t->lat},
                           {"lon", t->lon},
                           {"vs_fpm", std::round(t->vsFpm)},
                           {"g", std::round(t->gForce * 100.0) / 100.0},
                           {"ias_kt", std::round(t->iasKt)},
                           {"sim_pitch_deg", round1(t->pitchDeg)},
                           {"bank_deg", round1(t->bankDeg)},
                           {"heading_true_deg", std::lround(t->headingTrueDeg) % 360},
                           {"wind_dir_deg", std::lround(t->windDirDeg) % 360},
                           {"wind_kt", std::lround(t->windKt)}}});
    }
    // Fast sampling stays on after the touchdown until the bounce count is out (D-158).
    if (const auto b = touchdown_.bouncesDue(s)) events.push_back(bouncesEvent(s.time, *b));
    return events;
}

std::vector<LogEvent> FlightTracker::onAltimeter(const AltimeterSample& a) {
    altimeter_ = a;
    if (!detector_) return {};
    const AltimeterSample r = rounded(a);
    if (logged_ && same(*logged_, r)) return {};
    logged_ = r;
    return {altimeterEvent(r)};
}

std::vector<LogEvent> FlightTracker::onBlockIn(const BlockIn& in, double altFt) {
    if (!detector_) return {};
    std::vector<LogEvent> events{toLog(detector_->onBlockIn(in.time, altFt))};
    onFlightEnded();
    return events;
}

void FlightTracker::onFlightEnded() {
    detector_.reset();
    flightId_.clear();
    destinationIcao_.clear();
    linedUp_.reset();
    takeoffRoll_.clear();
    landingRoll_.clear();
    liftoffLogged_ = false;
    touchdown_.reset();
    fast_ = false;
    bouncesDone_ = false;
    logged_.reset();
}

}  // namespace blocktime
