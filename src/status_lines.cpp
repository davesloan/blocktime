// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "status_lines.h"

#include <algorithm>
#include <cmath>

#include "header_text.h"

namespace blocktime {
namespace {

constexpr const char* kPreparing = "Preparing aircraft for flight";
constexpr const char* kReady = "Ready for next flight";

std::string str(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_string() ? f->get<std::string>() : std::string();
}
int integer(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_number() ? static_cast<int>(std::lround(f->get<double>())) : 0;
}
bool flag(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_boolean() && f->get<bool>();
}

std::string landedText(const std::string& runway) { return runway.empty() ? "Landed" : "Landed on runway " + runway; }

}  // namespace

std::string arrivalGateName(const nlohmann::json& d) {
    const std::string result = str(d, "result");
    return result == "assigned" || result == "unconfirmed" ? str(d, "gate") : str(d, "si_gate");
}

void StatusLines::push(std::string text, const std::string& iso) {
    lines_.push_back({std::move(text), iso});
    levelRestore_.reset();
}

std::string StatusLines::flightLevelText(int ft, std::string_view phase) const {
    const bool fl = isFlightLevel(phase, ft, effectiveTransitionFt(context_.transitionAltitudeFt),
                                  effectiveTransitionFt(context_.transitionLevelFt));
    return altitudeText(ft, fl);
}

// Every GSX service of the next leg, latest state per service, in the order they started.
std::string StatusLines::preFlightText() const {
    std::vector<const ServiceSpan*> latest;
    for (const ServiceSpan& s : gsx_.nextLeg()) {
        const auto same = std::find_if(latest.begin(), latest.end(),
                                       [&](const ServiceSpan* l) { return l->service == s.service; });
        if (same != latest.end()) *same = &s;
        else latest.push_back(&s);
    }
    if (latest.empty()) return kPreparing;
    std::string text;
    for (const ServiceSpan* s : latest) {
        if (!text.empty()) text += " · ";
        text += serviceName(s->service) + (s->running ? " in progress" : " complete");
    }
    return text;
}

std::string StatusLines::preFlightStart() const {
    return gsx_.nextLeg().empty() ? std::string() : gsx_.nextLeg().front().startIso;
}

void StatusLines::clearFlight() {
    lines_.clear();
    showingFlight_ = false;
    phase_.clear();
    levelRestore_.reset();
    finalShown_ = false;
    finalRunway_.clear();
    arrivalGate_.clear();
    parking_.clear();
    landedLine_.reset();
    taxiInLine_.reset();
    parkedLine_.reset();
    deboardingLine_.reset();
    arrived_ = false;
    ready_ = false;
    checks_.reset();
}

void StatusLines::parked(const std::string& iso) {
    const std::string text = parking_.empty() ? "Parked" : "Parked at " + parking_;
    if (parkedLine_) {
        lines_[*parkedLine_].text = text;
    } else {
        push(text, iso);
        parkedLine_ = lines_.size() - 1;
    }
    arrived_ = true;
}

// D-135, as FlightStages has it: with GSX deboarding, Ready comes when it completes; without, when the checks pass.
void StatusLines::updateReady(const std::string& iso) {
    if (!arrived_) return;
    const auto& deboarding = gsx_.arrival();
    if (anyRunning(deboarding)) {
        ready_ = false;
        return;
    }
    if (ready_) return;
    const bool checksPass =
        checks_ && checks_->enginesOff && checks_->parkingBrakeSet && checks_->beaconOff && checks_->strobeOff;
    if (anyCompleted(deboarding) || checksPass) {
        push(kReady, iso);
        ready_ = true;
    }
}

void StatusLines::onPhase(const std::string& phase, const std::string& iso, const nlohmann::json& d) {
    showingFlight_ = true;
    phase_ = phase;
    const std::string& city = context_.destinationCity;
    if (phase == "pushback") {
        push("Pushback and startup", iso);
    } else if (phase == "taxi_out") {
        push(context_.departureRunway.empty() ? "Taxiing to runway" : "Taxiing to runway " + context_.departureRunway,
             iso);
    } else if (phase == "takeoff") {
        push("Takeoff roll", iso);
    } else if (phase == "climb") {
        const int cruise = context_.cruiseAltitudeFt;
        push(cruise > 0 ? "Climbing to " + flightLevelText(cruise, "climb") : "Climbing", iso);
    } else if (phase == "cruise") {
        push("Cruising at " + altitudeText(integer(d, "alt_ft"), flag(d, "flight_level")), iso);
    } else if (phase == "descent") {
        push(city.empty() ? "Descending" : "Descending into " + city, iso);
    } else if (phase == "approach") {
        push(city.empty() ? "Approach" : "Approach into " + city, iso);
    } else if (phase == "go_around") {
        finalShown_ = false;
        push("Go-around", iso);
    } else if (phase == "landing") {
        if (!landedLine_) {
            push(landedText(finalRunway_), iso);
            landedLine_ = lines_.size() - 1;
        }
    } else if (phase == "taxi_in") {
        push(arrivalGate_.empty() ? "Taxiing to gate" : "Taxiing to " + arrivalGate_, iso);
        taxiInLine_ = lines_.size() - 1;
    } else if (phase == "arrived") {
        parked(iso);
        updateReady(iso);
    }
}

void StatusLines::onEvent(std::string_view type, const nlohmann::json& d) {
    const std::string iso = str(d, "time");
    if (type == "block_out") {
        // The leg's preparation line stays as the flight's first line.
        const StatusLine first{preFlightText(), preFlightStart()};
        gsx_.onEvent(type, d);
        clearFlight();
        lines_.push_back(first);
        showingFlight_ = true;
        return;
    }
    if (type == "flight_interrupted") {
        gsx_.onEvent(type, d);
        clearFlight();
        return;
    }
    if (type == "gsx_service" || type == "sim_stopped" || type == "sim_disconnected") {
        gsx_.onEvent(type, d);
        if (type == "gsx_service" && arrived_ && str(d, "service") == "deboarding") {
            const auto& spans = gsx_.arrival();
            if (anyRunning(spans)) {
                if (!deboardingLine_ || lines_[*deboardingLine_].text != "Deboarding in progress") {
                    push("Deboarding in progress", iso);
                    deboardingLine_ = lines_.size() - 1;
                }
            } else if (anyCompleted(spans) && deboardingLine_) {
                lines_[*deboardingLine_].text = "Deboarding complete";
            }
            updateReady(iso);
        }
        return;
    }
    if (type == "block_in") {
        gsx_.onEvent(type, d);
        parking_ = str(d, "parking");
        parked(iso);
        updateReady(iso);
        return;
    }
    if (type == "phase_changed") {
        onPhase(str(d, "phase"), iso, d);
    } else if (type == "level_off") {
        if (!lines_.empty() && !levelRestore_) {
            const std::string restore = lines_.back().text;
            lines_.back().text = "Level at " + altitudeText(integer(d, "alt_ft"), flag(d, "flight_level"));
            levelRestore_ = restore;
        }
    } else if (type == "level_off_ended") {
        if (levelRestore_ && !lines_.empty()) lines_.back().text = *levelRestore_;
        levelRestore_.reset();
    } else if (type == "step_climb") {
        push("Leaving " + flightLevelText(integer(d, "from_ft"), "cruise"), iso);
    } else if (type == "step_climb_ended") {
        if (!lines_.empty()) {
            lines_.back().text = "Cruising at " + altitudeText(integer(d, "alt_ft"), flag(d, "flight_level"));
        }
    } else if (type == "liftoff") {
        const std::string runway = str(d, "runway");
        push(runway.empty() ? "Departed" : "Departed runway " + runway, iso);
    } else if (type == "lined_up") {
        if (!finalShown_) {
            finalRunway_ = str(d, "runway");
            push(finalRunway_.empty() ? "Final approach" : "Final approach, runway " + finalRunway_, iso);
            finalShown_ = true;
        }
    } else if (type == "touchdown") {
        if (!landedLine_) {
            push(landedText(finalRunway_), iso);
            landedLine_ = lines_.size() - 1;
        }
    } else if (type == "landing_runway") {
        finalRunway_ = str(d, "runway");
        if (landedLine_) lines_[*landedLine_].text = landedText(finalRunway_);
    } else if (type == "arrival_gate") {
        arrivalGate_ = arrivalGateName(d);
        if (taxiInLine_ && !arrivalGate_.empty()) lines_[*taxiInLine_].text = "Taxiing to " + arrivalGate_;
    }
}

bool StatusLines::onShutdownChecks(const ShutdownChecks& c, const std::string& iso) {
    if (!arrived_) return false;
    const bool changed = !checks_ || *checks_ != c;
    checks_ = c;
    const bool wasReady = ready_;
    updateReady(iso);
    return changed || ready_ != wasReady;
}

void StatusLines::showNextLeg() { clearFlight(); }

std::vector<StatusLine> StatusLines::lines() const {
    if (!showingFlight_ || lines_.empty()) return {{preFlightText(), preFlightStart()}};
    std::vector<StatusLine> out;
    for (auto it = lines_.rbegin(); it != lines_.rend() && out.size() < 3; ++it) out.push_back(*it);
    return out;
}

std::vector<ChecklistItem> StatusLines::checklist() const {
    if (!arrived_) return {};
    return shutdownChecklist(checks_.value_or(ShutdownChecks{}), gsx_.arrival());
}

}  // namespace blocktime
