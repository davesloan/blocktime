// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flight_stages.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace blocktime {

namespace {

constexpr const char* kStageNames[] = {"Pre-Departure", "Departure", "Climb", "Cruise", "Descent", "Arrival"};

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

// 11000 → "11,000 ft"; with a flight level, 35000 → "FL350".
std::string altitude(int ft, bool flightLevel) {
    if (flightLevel) return std::format("FL{:03}", (ft + 50) / 100);
    std::string digits = std::to_string(std::abs(ft));
    for (int i = static_cast<int>(digits.size()) - 3; i > 0; i -= 3) digits.insert(static_cast<std::size_t>(i), ",");
    return (ft < 0 ? "-" : "") + digits + " ft";
}

std::string hhmmZ(const std::string& iso) { return iso.size() >= 16 ? iso.substr(11, 5) + "Z" : iso; }

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
        phaseLine_ = add(Stage::Departure, "Pushback", iso, true);
    } else if (phase == "taxi_out") {
        phaseLine_ = add(Stage::Departure, "Taxiing to runway", iso, true);
    } else if (phase == "takeoff") {
        phaseLine_ = add(Stage::Departure, "Takeoff roll", iso, true);
    } else if (phase == "cruise") {
        cruiseLine_ = add(Stage::Cruise, "Cruising at " + altitude(altFt, flightLevel), iso, true);
    } else if (phase == "descent") {
        add(Stage::Descent, "Top of descent", iso, false);
    } else if (phase == "approach") {
        phaseLine_ = add(Stage::Descent, "Approach", iso, true);
    } else if (phase == "go_around") {
        phaseLine_ = add(Stage::Descent, "Go-around", iso, true);
    } else if (phase == "landing") {
        if (!landedLine_) landedLine_ = add(Stage::Descent, landedText(), iso, false);
    } else if (phase == "taxi_in") {
        phaseLine_ = add(Stage::Descent, "Taxiing to gate", iso, true);
    } else if (phase == "arrived") {
        add(Stage::Arrival, "Parked", iso, false);
    }
}

void FlightStages::onEvent(std::string_view type, const nlohmann::json& d) {
    const std::string iso = str(d, "time");
    if (type == "phase_changed") {
        onPhase(str(d, "phase"), iso, integer(d, "alt_ft"), flag(d, "flight_level"));
    } else if (type == "level_off") {
        close(levelLine_, iso);
        levelFt_ = integer(d, "alt_ft");
        levelFl_ = flag(d, "flight_level");
        levelLine_ = add(stageOf(str(d, "phase")), "Level at " + altitude(levelFt_, levelFl_), iso, true);
    } else if (type == "level_off_ended") {
        if (levelLine_) lines_[*levelLine_].text = "Level at " + altitude(integer(d, "alt_ft"), flag(d, "flight_level"));
        close(levelLine_, iso);
    } else if (type == "step_climb") {
        close(cruiseLine_, iso);
        stepFromFt_ = integer(d, "from_ft");
        stepLine_ = add(Stage::Cruise, "Step climb", iso, true);
    } else if (type == "step_climb_ended") {
        const int alt = integer(d, "alt_ft");
        const std::string level = altitude(alt, flag(d, "flight_level"));
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
        else if (m == "final_approach") add(Stage::Descent, "Final approach", iso, false);
    } else if (type == "liftoff") {
        if (phase_ == "takeoff") close(phaseLine_, iso);
        add(Stage::Departure, "Takeoff", iso, false);
        const std::string runway = str(d, "runway");
        add(Stage::Climb, runway.empty() ? "Departed" : "Departed runway " + runway, iso, false);
    } else if (type == "touchdown") {
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

void FlightStages::reset() { *this = FlightStages{}; }

std::vector<StageView> FlightStages::view() const {
    const int current = static_cast<int>(stageOf(phase_));
    std::vector<StageView> out;
    for (int i = 0; i < 6; ++i) {
        StageView v{static_cast<Stage>(i), kStageNames[i],
                    i < current ? StageState::Done : i == current ? StageState::Current : StageState::Upcoming,
                    {}};
        std::vector<const Line*> mine;
        for (const Line& l : lines_)
            if (static_cast<int>(l.stage) == i) mine.push_back(&l);
        std::stable_sort(mine.begin(), mine.end(), [](const Line* a, const Line* b) { return a->startIso < b->startIso; });
        for (const Line* l : mine)
            v.lines.push_back({l->text, hhmmZ(l->startIso), l->endIso.empty() ? "" : hhmmZ(l->endIso), l->running});
        out.push_back(std::move(v));
    }
    return out;
}

std::string FlightStages::phaseLabel() const {
    std::string label = phaseName(phase_);
    if (levelLine_ && lines_[*levelLine_].running &&
        (phase_ == "climb" || phase_ == "descent" || phase_ == "approach")) {
        label += " — level at " + altitude(levelFt_, levelFl_);
    } else if (stepLine_ && lines_[*stepLine_].running && phase_ == "cruise") {
        label += " — step climb";
    }
    return label;
}

}  // namespace blocktime
