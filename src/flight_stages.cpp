// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_stages.h"

#include <algorithm>
#include <cmath>
#include <format>

#include "header_text.h"

namespace blocktime {

namespace {

constexpr const char* kStageNames[] = {"Pre-Flight", "Departure", "Climb", "Cruise", "Descent", "Arrival"};

Stage stageOf(std::string_view phase) {
    if (phase == "pushback" || phase == "taxi_out" || phase == "takeoff") return Stage::Departure;
    if (phase == "climb") return Stage::Climb;
    if (phase == "cruise") return Stage::Cruise;
    if (phase == "descent" || phase == "approach" || phase == "go_around" || phase == "landing" ||
        phase == "taxi_in")
        return Stage::Descent;
    if (phase == "arrived") return Stage::Arrival;
    return Stage::PreDeparture;
}

std::string phaseName(std::string_view phase) {
    if (phase == "pushback") return "Pushback";
    if (phase == "taxi_out") return "Taxi out";
    if (phase == "takeoff") return "Takeoff";
    if (phase == "climb") return "Climb";
    if (phase == "cruise") return "Cruise";
    if (phase == "descent") return "Descent";
    if (phase == "approach") return "Approach";
    if (phase == "go_around") return "Go-around";
    if (phase == "landing") return "Landing";
    if (phase == "taxi_in") return "Taxi in";
    if (phase == "arrived") return "Arrived";
    return "";
}

std::string serviceName(const std::string& service) {
    if (service == "catering") return "Catering";
    if (service == "refuelling") return "Refuelling";
    if (service == "boarding") return "Boarding";
    if (service == "deboarding") return "Deboarding";
    return service;
}

bool anyRunning(const std::vector<ServiceSpan>& spans) {
    return std::any_of(spans.begin(), spans.end(), [](const ServiceSpan& s) { return s.running; });
}
bool anyCompleted(const std::vector<ServiceSpan>& spans) {
    return std::any_of(spans.begin(), spans.end(), [](const ServiceSpan& s) { return !s.running; });
}

std::string hhmmZ(const std::string& iso) { return iso.size() >= 16 ? iso.substr(11, 5) + "Z" : iso; }

std::string str(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_string() ? f->get<std::string>() : std::string();
}

// The arrival_gate event's line (D-147): BlockTime's gate, or SayIntentions' own when it kept it.
std::string arrivalGateText(const nlohmann::json& d) {
    const std::string result = str(d, "result"), gate = str(d, "gate"), si = str(d, "si_gate");
    if (result == "kept") return "Arrival gate: SayIntentions' " + si;
    if (result == "assigned") {
        const std::string reason = str(d, "reason");
        return "Arrival gate: " + gate +
               (reason == "preferred" ? " (preferred)" : reason == "larger" ? " (SayIntentions' was too small)" : "");
    }
    if (result == "ignored") return "Arrival gate: SayIntentions' " + si + " (it didn't take " + gate + ")";
    if (result == "unconfirmed") return "Arrival gate: " + gate + " (not confirmed)";
    if (result == "unknown_gate") return "Arrival gate: SayIntentions' " + si + " (it doesn't know " + gate + ")";
    if (result == "none_fits") return "Arrival gate: SayIntentions' " + si + " (no spot fits)";
    return "Arrival gate: SayIntentions' " + si + " (assignGate failed)";
}
int integer(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_number() ? static_cast<int>(std::lround(f->get<double>())) : 0;
}
bool flag(const nlohmann::json& d, const char* key) {
    const auto f = d.find(key);
    return f != d.end() && f->is_boolean() && f->get<bool>();
}

}  // namespace

std::size_t FlightStages::add(Stage stage, std::string text, const std::string& iso, bool running) {
    lines_.push_back(Line{stage, std::move(text), iso, "", running});
    return lines_.size() - 1;
}

void FlightStages::close(std::optional<std::size_t>& line, const std::string& iso) {
    if (!line) return;
    if (lines_[*line].running) {
        lines_[*line].running = false;
        lines_[*line].endIso = iso;
    }
    line.reset();
}

std::string FlightStages::landedText() const {
    std::string s = "Landed";
    if (!landingRunway_.empty()) s += " on " + landingRunway_;
    if (touchdownVsFpm_) {
        const long vs = std::lround(*touchdownVsFpm_);
        s += ", " + std::string(vs < 0 ? "−" : "") + std::to_string(std::labs(vs)) + " fpm";
    }
    return s;
}

void FlightStages::onPhase(const std::string& phase, const std::string& iso, int altFt, bool flightLevel) {
    close(phaseLine_, iso);
    if (phase != "cruise") {
        close(cruiseLine_, iso);
        close(stepLine_, iso);
    }
    // A level carries on from Descent into Approach (the same stage); any other change ends it.
    if (!(phase_ == "descent" && phase == "approach")) close(levelLine_, iso);
    phase_ = phase;

    if (phase == "pushback") {
        phaseLine_ = add(Stage::Departure, gsx_.pushbackByGsx() ? "Pushback (GSX)" : "Pushback", iso, true);
    } else if (phase == "taxi_out") {
        phaseLine_ = add(Stage::Departure, "Taxiing to runway", iso, true);
    } else if (phase == "takeoff") {
        phaseLine_ = add(Stage::Departure, "Takeoff roll", iso, true);
    } else if (phase == "cruise") {
        cruiseLine_ = add(Stage::Cruise, "Cruising at " + altitudeText(altFt, flightLevel), iso, true);
    } else if (phase == "descent") {
        add(Stage::Descent, "Top of descent", iso, false);
    } else if (phase == "approach") {
        phaseLine_ = add(Stage::Descent, "Approach", iso, true);
    } else if (phase == "go_around") {
        close(finalLine_, iso);
        close(shortFinalLine_, iso);
        phaseLine_ = add(Stage::Descent, "Go-around", iso, true);
    } else if (phase == "landing") {
        close(finalLine_, iso);
        close(shortFinalLine_, iso);
        if (!landedLine_) landedLine_ = add(Stage::Descent, landedText(), iso, false);
    } else if (phase == "taxi_in") {
        phaseLine_ = add(Stage::Descent, "Taxiing to gate", iso, true);
    } else if (phase == "arrived") {
        add(Stage::Arrival, parking_.empty() ? "Parked" : "Parked at " + parking_, iso, false);
        updateReady(iso);
    }
}

void FlightStages::clearFlight() {
    GsxTimeline gsx = std::move(gsx_);
    *this = FlightStages{};
    gsx_ = std::move(gsx);
}

void FlightStages::updateReady(const std::string& iso) {
    if (!showingFlight_ || phase_ != "arrived") return;
    const auto& deboarding = gsx_.arrival();
    // With GSX deboarding, Ready comes when it completes, even if the checks passed first (Dave turns the beacon off
    // before calling GSX).
    if (anyRunning(deboarding)) {
        readyIso_.clear();
        return;
    }
    if (!readyIso_.empty()) return;
    const bool checksPass =
        checks_ && checks_->enginesOff && checks_->parkingBrakeSet && checks_->beaconOff && checks_->strobeOff;
    if (anyCompleted(deboarding) || checksPass) readyIso_ = iso;
}

StageLine FlightStages::readyLine() const {
    const ShutdownChecks c = checks_.value_or(ShutdownChecks{});
    StageLine l{"Ready for shutdown or turnaround", hhmmZ(readyIso_), "", false, {}};
    l.checks.push_back({"Engines off", c.enginesOff});
    l.checks.push_back({"Parking brake set", c.parkingBrakeSet});
    if (!gsx_.arrival().empty()) l.checks.push_back({"Deboarding complete", anyCompleted(gsx_.arrival())});
    l.checks.push_back({"Beacon off", c.beaconOff});
    l.checks.push_back({"Strobes off", c.strobeOff});
    return l;
}

bool FlightStages::onShutdownChecks(const ShutdownChecks& c, const std::string& iso) {
    if (!showingFlight_ || phase_ != "arrived") return false;
    const bool changed = !checks_ || *checks_ != c;
    checks_ = c;
    const bool wasReady = !readyIso_.empty();
    updateReady(iso);
    if (readyIso_.empty()) return false;  // nothing shows the checks yet
    return !wasReady || changed;
}

void FlightStages::showNextLeg() { clearFlight(); }

void FlightStages::onEvent(std::string_view type, const nlohmann::json& d) {
    const std::string iso = str(d, "time");
    if (type == "block_out") {
        clearFlight();
        gsx_.onEvent(type, d);
        showingFlight_ = true;
        return;
    }
    if (type == "flight_interrupted") {  // discarded with its services (D-072); the next leg shows
        gsx_.onEvent(type, d);
        clearFlight();
        return;
    }
    if (type == "gsx_service" || type == "block_in" || type == "sim_stopped" || type == "sim_disconnected") {
        if (type == "block_in") parking_ = str(d, "parking");
        gsx_.onEvent(type, d);
        if (type == "gsx_service") updateReady(iso);
        return;
    }
    if (type == "phase_changed") {
        showingFlight_ = true;
        onPhase(str(d, "phase"), iso, integer(d, "alt_ft"), flag(d, "flight_level"));
    } else if (type == "level_off") {
        close(levelLine_, iso);
        levelFt_ = integer(d, "alt_ft");
        levelFl_ = flag(d, "flight_level");
        levelLine_ = add(stageOf(str(d, "phase")), "Level at " + altitudeText(levelFt_, levelFl_), iso, true);
    } else if (type == "level_off_ended") {
        if (levelLine_) {
            lines_[*levelLine_].text = "Level at " + altitudeText(integer(d, "alt_ft"), flag(d, "flight_level"));
        }
        close(levelLine_, iso);
    } else if (type == "step_climb") {
        close(cruiseLine_, iso);
        stepFromFt_ = integer(d, "from_ft");
        stepLine_ = add(Stage::Cruise, "Step climb", iso, true);
    } else if (type == "step_climb_ended") {
        const int alt = integer(d, "alt_ft");
        const std::string level = altitudeText(alt, flag(d, "flight_level"));
        if (stepLine_) lines_[*stepLine_].text = (alt >= stepFromFt_ ? "Step climb to " : "Descended to ") + level;
        close(stepLine_, iso);
        cruiseLine_ = add(Stage::Cruise, "Cruising at " + level, iso, true);
    } else if (type == "flight_marker") {
        const std::string m = str(d, "marker");
        if (m == "above_10000") add(Stage::Climb, "Above 10,000 ft", iso, false);
        else if (m == "transition_altitude") add(Stage::Climb, "Passed transition altitude", iso, false);
        else if (m == "within_200nm") add(Stage::Cruise, "200 nm from destination", iso, false);
        else if (m == "transition_level") add(Stage::Descent, "Passed transition level", iso, false);
        else if (m == "below_10000") add(Stage::Descent, "Below 10,000 ft", iso, false);
        else if (m == "final_approach") {
            // 1,000 ft AGL: Final approach becomes Short final (D-149).
            close(finalLine_, iso);
            if (!shortFinalLine_) shortFinalLine_ = add(Stage::Descent, "Short final", iso, true);
        }
    } else if (type == "lined_up") {
        if (!finalLine_ && !shortFinalLine_) {
            finalLine_ = add(Stage::Descent, "Final approach, runway " + str(d, "runway"), iso, true);
        }
    } else if (type == "arrival_gate") {
        add(phase_.empty() ? Stage::Cruise : stageOf(phase_), arrivalGateText(d), iso, false);
    } else if (type == "liftoff") {
        if (phase_ == "takeoff") close(phaseLine_, iso);
        add(Stage::Departure, "Takeoff", iso, false);
        const std::string runway = str(d, "runway");
        add(Stage::Climb, runway.empty() ? "Departed" : "Departed runway " + runway, iso, false);
    } else if (type == "touchdown") {
        close(finalLine_, iso);
        close(shortFinalLine_, iso);
        const auto vs = d.find("vs_fpm");
        if (vs != d.end() && vs->is_number()) touchdownVsFpm_ = vs->get<double>();
        if (landedLine_) {
            lines_[*landedLine_].startIso = iso;
            lines_[*landedLine_].text = landedText();
        } else {
            landedLine_ = add(Stage::Descent, landedText(), iso, false);
        }
    } else if (type == "landing_runway") {
        landingRunway_ = str(d, "runway");
        if (landedLine_) lines_[*landedLine_].text = landedText();
    }
}

std::vector<StageView> FlightStages::view() const {
    // The flight's lines plus its GSX services: those it took at block out, and its deboarding. The next leg (before
    // block out, or after showNextLeg) has only the services so far.
    std::vector<Line> all = lines_;
    const auto addSpans = [&](Stage stage, const std::vector<ServiceSpan>& spans) {
        for (const ServiceSpan& sp : spans) all.push_back(Line{stage, serviceName(sp.service), sp.startIso, sp.endIso, sp.running});
    };
    if (showingFlight_) {
        addSpans(Stage::PreDeparture, gsx_.departed());
        addSpans(Stage::Arrival, gsx_.arrival());
    } else {
        addSpans(Stage::PreDeparture, gsx_.nextLeg());
    }
    const int current = static_cast<int>(stageOf(phase_));
    std::vector<StageView> out;
    for (int i = 0; i < 6; ++i) {
        StageView v{static_cast<Stage>(i), kStageNames[i],
                    i < current ? StageState::Done : i == current ? StageState::Current : StageState::Upcoming,
                    {}};
        std::vector<const Line*> mine;
        for (const Line& l : all)
            if (static_cast<int>(l.stage) == i) mine.push_back(&l);
        std::stable_sort(mine.begin(), mine.end(), [](const Line* a, const Line* b) { return a->startIso < b->startIso; });
        for (const Line* l : mine)
            v.lines.push_back({l->text, hhmmZ(l->startIso), l->endIso.empty() ? "" : hhmmZ(l->endIso), l->running, {}});
        if (i == static_cast<int>(Stage::Arrival) && !readyIso_.empty()) v.lines.push_back(readyLine());
        out.push_back(std::move(v));
    }
    return out;
}

std::string FlightStages::phaseLabel() const {
    std::string label = phaseName(phase_);
    if (levelLine_ && lines_[*levelLine_].running &&
        (phase_ == "climb" || phase_ == "descent" || phase_ == "approach")) {
        label += " — level at " + altitudeText(levelFt_, levelFl_);
    } else if (stepLine_ && lines_[*stepLine_].running && phase_ == "cruise") {
        label += " — step climb";
    }
    return label;
}

}  // namespace blocktime
