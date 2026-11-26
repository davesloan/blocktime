// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debrief_measure.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>

#include "event_log.h"

namespace blocktime {

namespace {

using Clock = std::chrono::system_clock;
using Time = Clock::time_point;

// One timed value of a series: telemetry, cockpit and altimeter lines by when they were written.
struct Point {
    Time t;
    const nlohmann::json* d;
};
using Series = std::vector<Point>;

constexpr double kInHgToHpa = 33.8639;

// When the thing happened: data.time if it has one (D-125's readers order by it), else the line's ts.
Time when(const LogLine& l) {
    if (l.data.is_object() && l.data.contains("time") && l.data["time"].is_string()) {
        if (auto t = parseUtc(l.data["time"].get<std::string>())) return *t;
    }
    return l.ts;
}

const nlohmann::json* stateAt(const Series& s, Time t) {
    const nlohmann::json* d = nullptr;
    for (const Point& p : s) {
        if (p.t > t) break;
        d = p.d;
    }
    return d;
}

bool flag(const nlohmann::json* d, const char* key) { return d && d->contains(key) && (*d)[key].get<bool>(); }
double number(const nlohmann::json* d, const char* key) {
    return d && d->contains(key) && (*d)[key].is_number() ? (*d)[key].get<double>() : 0.0;
}

double seconds(Clock::duration d) { return std::chrono::duration<double>(d).count(); }
int whole(double x) { return static_cast<int>(std::lround(x)); }

// Seconds inside [from, to] where `bad` holds for the state then in force (the state before `from` counts from it).
double secondsWhere(const Series& s, Time from, Time to, const std::function<bool(const nlohmann::json&)>& bad) {
    if (to <= from) return 0;
    double total = 0;
    const nlohmann::json* state = stateAt(s, from);
    Time since = from;
    for (const Point& p : s) {
        if (p.t <= from) continue;
        if (p.t >= to) break;
        if (state && bad(*state)) total += seconds(p.t - since);
        state = p.d;
        since = p.t;
    }
    if (state && bad(*state)) total += seconds(to - since);
    return total;
}

// Seconds from `from` until `good` first holds (0 if it already does then); to `end` if it never does.
double secondsUntil(const Series& s, Time from, Time end, const std::function<bool(const nlohmann::json&)>& good) {
    if (const nlohmann::json* d = stateAt(s, from); d && good(*d)) return 0;
    for (const Point& p : s) {
        if (p.t <= from) continue;
        if (good(*p.d)) return seconds(p.t - from);
    }
    return seconds(end - from);
}

std::optional<double> hpa(const nlohmann::json& atis) {
    if (!atis.contains("altimeter")) return std::nullopt;
    double v = 0;
    if (atis["altimeter"].is_string()) {
        try {
            v = std::stod(atis["altimeter"].get<std::string>());
        } catch (...) {
            return std::nullopt;
        }
    } else if (atis["altimeter"].is_number()) {
        v = atis["altimeter"].get<double>();
    } else {
        return std::nullopt;
    }
    return v > 800 ? v : v * kInHgToHpa;  // "30.22" is inches; "1013" is already hPa
}

class Measurer {
public:
    Measurer(const std::vector<LogLine>& lines, bool diverted) : diverted_(diverted), start_(flightStart(lines)) {
        for (const LogLine& l : lines) {
            const Time t = when(l);
            if (l.type == "telemetry") telemetry_.push_back({l.ts, &l.data});
            else if (l.type == "cockpit") cockpit_.push_back({l.ts, &l.data});
            else if (l.type == "altimeter") altimeter_.push_back({l.ts, &l.data});
            else if (l.type == "atis") atis_.push_back({l.ts, &l.data});
            else if (l.type == "phase_changed") phases_.emplace_back(l.data.value("phase", ""), t);
            else if (l.type == "flight_marker") markers_[l.data.value("marker", "")].push_back(t);
            else if (l.type == "lined_up") linedUp_.push_back(t);
            else if (l.type == "touchdown") touchdown_ = {t, &l.data};
            else if (l.type == "bounces") bounces_ = l.data.value("bounces", 0);
            else if (l.type == "landing_runway") runway_ = &l.data;
            else if (l.type == "block_out") {
                blockOut_ = t;
                m_.family = familyOf(l.data.value("aircraft_folder", ""));
            } else if (l.type == "block_in") {
                blockIn_ = t;
                // When the last engine stopped (the block_in line's own time when it doesn't say).
                shutdown_ = l.ts;
                if (l.data.contains("shutdown_time") && l.data["shutdown_time"].is_string()) {
                    if (auto s = parseUtc(l.data["shutdown_time"].get<std::string>())) shutdown_ = *s;
                }
            }
        }
        auto byTime = [](const Point& a, const Point& b) { return a.t < b.t; };
        for (Series* s : {&telemetry_, &cockpit_, &altimeter_, &atis_}) std::stable_sort(s->begin(), s->end(), byTime);
        for (const Point& p : cockpit_) pmdg_ |= flag(p.d, "pmdg");
    }

    FlightMeasurements run() {
        beacon();
        beltsDeparture();
        taxi();
        originBaro();
        takeoffLights();
        over250();
        stdLate();
        beltsDescent();
        destinationBaro();
        landingLights();
        stable();
        strobes();
        apuTransition();
        apuGate();
        landing();
        return std::move(m_);
    }

private:
    void put(const char* part, double v) { m_.values.push_back({part, whole(v)}); }
    void skip(const char* part, const char* reason) { m_.skipped.push_back({part, reason}); }

    std::optional<Time> phaseStart(const std::string& phase, bool last = false) const {
        std::optional<Time> t;
        for (const auto& [p, at] : phases_) {
            if (p != phase) continue;
            t = at;
            if (!last) break;
        }
        return t;
    }
    std::optional<Time> marker(const std::string& name, bool last = false) const {
        const auto it = markers_.find(name);
        if (it == markers_.end() || it->second.empty()) return std::nullopt;
        return last ? it->second.back() : it->second.front();
    }
    Time end() const { return blockIn_.value_or(telemetry_.empty() ? Time{} : telemetry_.back().t); }

    // The seat belt sign needs PMDG's data on the 737: its plain SimVar never moves (D-159).
    const char* beltsMissing() const {
        if (cockpit_.empty() || !beltsReadable(m_.family, pmdg_)) return "no_pmdg_data";
        return nullptr;
    }
    // So do the 737's landing lights: LIGHT LANDING misses its retractable ones (D-159).
    const char* landingLightsMissing() const { return beltsMissing(); }

    void beacon() {
        if (!blockOut_ || telemetry_.empty() || !start_) return skip("beacon_late_s", "no_marker");
        const Time deadline = *start_;
        put("beacon_late_s",
            secondsUntil(telemetry_, deadline, end(), [](const nlohmann::json& d) { return d.value("beacon", false); }));
    }

    void beltsDeparture() {
        if (const char* r = beltsMissing()) return skip("belts_off_departure_s", r);
        const auto from = phaseStart("taxi_out");
        auto to = marker("transition_altitude");  // D-200 (it was above_10000)
        if (!to) to = phaseStart("descent");
        if (!to && touchdown_) to = touchdown_->t;
        if (!from || !to) return skip("belts_off_departure_s", "no_marker");
        put("belts_off_departure_s", secondsWhere(cockpit_, *from, *to, [](const nlohmann::json& d) { return !beltsOn(d); }));
    }

    void taxi() {
        const auto out = phaseStart("taxi_out"), takeoff = phaseStart("takeoff"), in = phaseStart("taxi_in", true);
        if (!out && !in) return skip("taxi_max_kt", "no_marker");
        double top = 0;
        for (const Point& p : telemetry_) {
            const bool taxiOut = out && p.t >= *out && (!takeoff || p.t < *takeoff);
            const bool taxiIn = in && p.t >= *in && p.t <= end();
            if ((taxiOut || taxiIn) && p.d->value("on_ground", false)) top = std::max(top, number(p.d, "gs_kt"));
        }
        put("taxi_max_kt", top);
    }

    // The larger of the two altimeters' distance from the ATIS in force at `at` (or the one before), in tenths.
    std::optional<double> baroOff(const std::string& role, Time at) const {
        std::vector<double> qnh;
        for (const Point& p : atis_) {
            if (p.t > at) break;
            if (p.d->value("role", "") != role) continue;
            if (auto q = hpa(*p.d)) qnh.push_back(*q);
        }
        const nlohmann::json* a = stateAt(altimeter_, at);
        if (qnh.empty() || !a) return std::nullopt;
        double best = 1e9;
        for (std::size_t i = qnh.size() >= 2 ? qnh.size() - 2 : 0; i < qnh.size(); ++i) {
            best = std::min(best, std::max(std::abs(number(a, "capt_mb") - qnh[i]), std::abs(number(a, "fo_mb") - qnh[i])));
        }
        return best * 10;
    }

    void originBaro() {
        const auto t = phaseStart("takeoff");
        if (!t) return skip("origin_baro_off_tenths", "no_marker");
        if (const auto off = baroOff("origin", *t)) return put("origin_baro_off_tenths", *off);
        skip("origin_baro_off_tenths", "no_atis");
    }

    void takeoffLights() {
        const auto t = phaseStart("takeoff");
        if (!t) {
            skip("takeoff_strobes", "no_marker");
            return skip("takeoff_landing_lights", "no_marker");
        }
        // A few seconds' grace: the takeoff's time is back-dated to the start of the roll, and lights switched on as
        // thrust comes up still count (cockpit lines are written at most once a second).
        const Time by = *t + kTakeoffLightsGrace;
        put("takeoff_strobes", flag(stateAt(telemetry_, by), "strobe") ? 1 : 0);
        if (const char* r = landingLightsMissing()) return skip("takeoff_landing_lights", r);
        const nlohmann::json* cp = stateAt(cockpit_, by);
        put("takeoff_landing_lights", cp && landingLightsOn(*cp) ? 1 : 0);
    }

    void over250() {
        if (!blockOut_ || telemetry_.empty()) return skip("over_250_s", "no_marker");
        put("over_250_s", secondsWhere(telemetry_, *blockOut_, end(), [](const nlohmann::json& d) {
                return !d.value("on_ground", true) && d.value("alt_ind_ft", 0.0) < 10000 && d.value("ias_kt", 0.0) > 260;
            }));
    }

    void stdLate() {
        const auto ta = marker("transition_altitude");
        if (!ta || altimeter_.empty()) return skip("std_late_s", "no_marker");
        put("std_late_s", secondsUntil(altimeter_, *ta, end(), [](const nlohmann::json& d) { return altimetersStd(d); }));
    }

    void beltsDescent() {
        if (const char* r = beltsMissing()) return skip("belts_off_descent_s", r);
        const auto tl = marker("transition_level", true);
        if (!tl) return skip("belts_off_descent_s", "no_marker");
        put("belts_off_descent_s", secondsWhere(cockpit_, *tl, end(), [](const nlohmann::json& d) { return !beltsOn(d); }));
    }

    void destinationBaro() {
        if (diverted_) return skip("destination_baro_off_tenths", "diverted");
        const auto tl = marker("transition_level", true);
        if (!tl) return skip("destination_baro_off_tenths", "no_marker");
        if (const auto off = baroOff("destination", *tl + kDestinationBaroDelay)) {
            return put("destination_baro_off_tenths", *off);
        }
        skip("destination_baro_off_tenths", "no_atis");
    }

    void landingLights() {
        if (const char* r = landingLightsMissing()) return skip("landing_lights_off_s", r);
        const auto below = marker("below_10000", true);
        if (!below || !touchdown_) return skip("landing_lights_off_s", "no_marker");
        // Only the approach that landed: after a go-around the window starts again (docs/debrief.md section 2).
        Time from = *below + kLandingLightsDelay;
        for (const auto& [p, at] : phases_)
            if (p == "go_around" && at <= touchdown_->t) from = std::max(from, at);
        put("landing_lights_off_s", secondsWhere(cockpit_, from, touchdown_->t,
                                                 [](const nlohmann::json& d) { return !landingLightsOn(d); }));
    }

    void stable() {
        const auto fa = marker("final_approach", true);
        if (!fa) {
            for (const char* p : {"stable_gear_down", "stable_landing_flaps", "stable_sink_fpm", "stable_vref_offset_kt",
                                  "stable_lined_up"}) {
                skip(p, "no_marker");
            }
            return;
        }
        const nlohmann::json* tel = stateAt(telemetry_, *fa);
        const nlohmann::json* cp = stateAt(cockpit_, *fa);
        if (!cp) {
            skip("stable_gear_down", "no_pmdg_data");
        } else {
            put("stable_gear_down", gearDown(*cp) ? 1 : 0);
        }
        const int flaps = cp ? static_cast<int>(number(cp, "landing_flaps")) : 0;
        const int vref = cp ? static_cast<int>(number(cp, "vref_kt")) : 0;
        if (!pmdg_ || !cp || !cp->contains("landing_flaps")) {
            skip("stable_landing_flaps", "no_pmdg_data");
        } else if (flaps <= 0) {
            skip("stable_landing_flaps", "no_vref");
        } else {
            put("stable_landing_flaps",
                flapDetentDegrees(m_.family, static_cast<int>(number(tel, "flaps_handle"))) == flaps ? 1 : 0);
        }
        put("stable_sink_fpm", -number(tel, "vs_fpm"));
        if (!pmdg_ || !cp || !cp->contains("vref_kt")) {
            skip("stable_vref_offset_kt", "no_pmdg_data");
        } else if (vref <= 0) {
            skip("stable_vref_offset_kt", "no_vref");
        } else {
            put("stable_vref_offset_kt", number(tel, "ias_kt") - vref);
        }
        // Lined up since the last go-around (or block out) and by the marker.
        std::optional<Time> since = blockOut_;
        for (const auto& [p, at] : phases_)
            if (p == "go_around" && at <= *fa) since = at;
        bool lined = false;
        for (const Time t : linedUp_) lined |= (!since || t >= *since) && t <= *fa;
        put("stable_lined_up", lined ? 1 : 0);
    }

    void strobes() {
        const auto in = phaseStart("taxi_in", true);
        if (!in) return skip("strobes_on_after_landing_s", "no_marker");
        put("strobes_on_after_landing_s",
            secondsUntil(telemetry_, *in, end(), [](const nlohmann::json& d) { return !d.value("strobe", true); }));
    }

    // The APU is in PMDG's data only (D-172): a flight without it can't be judged on the APU.
    bool hasApu() const {
        return std::any_of(cockpit_.begin(), cockpit_.end(), [](const Point& p) { return apuSwitchOn(*p.d).has_value(); });
    }

    void apuTransition() {
        if (!hasApu()) return skip("apu_on_after_transition_s", "no_pmdg_data");
        const auto ta = marker("transition_altitude");
        if (!ta) return skip("apu_on_after_transition_s", "no_marker");
        put("apu_on_after_transition_s", secondsUntil(cockpit_, *ta, end(), [](const nlohmann::json& d) {
                return apuSwitchOn(d) == std::optional<bool>(false);
            }));
    }

    void apuGate() {
        if (!hasApu()) return skip("apu_on_at_shutdown", "no_pmdg_data");
        if (!shutdown_) return skip("apu_on_at_shutdown", "no_marker");
        const nlohmann::json* cp = stateAt(cockpit_, *shutdown_);
        put("apu_on_at_shutdown", cp && apuSwitchOn(*cp).value_or(false) ? 1 : 0);
    }

    void landing() {
        if (!touchdown_) return;
        const nlohmann::json& d = *touchdown_->d;
        LandingFacts f;
        f.vsFpm = whole(std::abs(d.value("vs_fpm", 0.0)));
        f.gHundredths = whole(d.value("g", 0.0) * 100);
        f.bounces = bounces_;
        f.iasKt = whole(d.value("ias_kt", 0.0));
        f.headingTrueDeg = whole(d.value("heading_true_deg", 0.0));
        f.windDirDeg = whole(d.value("wind_dir_deg", 0.0));
        f.windKt = whole(d.value("wind_kt", 0.0));
        if (d.contains("sim_pitch_deg")) f.simPitchTenths = whole(d["sim_pitch_deg"].get<double>() * 10);
        if (d.contains("bank_deg")) f.bankTenths = whole(d["bank_deg"].get<double>() * 10);
        if (const nlohmann::json* cp = stateAt(cockpit_, touchdown_->t); pmdg_ && cp && number(cp, "vref_kt") > 0) {
            f.vrefKt = whole(number(cp, "vref_kt"));
        }
        if (runway_ && runway_->contains("off_centreline_m")) {
            f.offCentrelineDm = whole(number(runway_, "off_centreline_m") * 10);
            f.offHeadingTenths = whole(number(runway_, "off_heading_deg") * 10);
        }
        m_.landing = f;
    }

    bool diverted_;
    FlightMeasurements m_;
    Series telemetry_, cockpit_, altimeter_, atis_;
    std::vector<std::pair<std::string, Time>> phases_;
    std::map<std::string, std::vector<Time>> markers_;
    std::vector<Time> linedUp_;
    std::optional<Point> touchdown_;
    int bounces_ = 0;
    const nlohmann::json* runway_ = nullptr;
    std::optional<Time> blockOut_, blockIn_, shutdown_;
    bool pmdg_ = false;
    std::optional<Time> start_;
};

}  // namespace

int flapDetentDegrees(Family family, int handleIndex) {
    static constexpr std::array<int, 9> k737 = {0, 1, 2, 5, 10, 15, 25, 30, 40};
    static constexpr std::array<int, 7> k777 = {0, 1, 5, 15, 20, 25, 30};
    if (handleIndex < 0) return 0;
    const auto i = static_cast<std::size_t>(handleIndex);
    if (family == Family::B777) return i < k777.size() ? k777[i] : k777.back();
    return i < k737.size() ? k737[i] : k737.back();
}

std::optional<Clock::time_point> flightStart(const std::vector<LogLine>& lines) {
    std::optional<Time> blockOut;
    for (const LogLine& l : lines)
        if (l.type == "block_out") blockOut = when(l);
    if (!blockOut) return std::nullopt;
    std::optional<Time> started;
    bool running = false;
    for (const LogLine& l : lines) {
        if (l.type != "telemetry") continue;
        if (l.ts > *blockOut) break;
        bool any = false;
        if (l.data.contains("eng_running"))
            for (const auto& e : l.data["eng_running"]) any |= e.get<bool>();
        if (any && !running) started = l.ts;
        if (!any) started.reset();
        running = any;
    }
    return running && started ? *started : *blockOut;
}

FlightMeasurements measureFlight(const std::vector<LogLine>& lines, bool diverted) {
    return Measurer(lines, diverted).run();
}

}  // namespace blocktime
