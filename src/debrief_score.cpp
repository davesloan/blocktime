// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debrief_score.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <functional>
#include <map>
#include <numbers>

namespace blocktime {

namespace {

// The tolerances (D-160, docs/debrief.md section 2).
constexpr int kBeaconLateS = 10;
constexpr int kTaxiMaxKt = 30;
constexpr int kBaroOffTenths = 10;  // 1 hPa
constexpr int kOver250S = 10;
constexpr int kStdLateS = 60;
constexpr int kStableSinkFpm = 1000;
constexpr int kStableVrefBelowKt = -5, kStableVrefAboveKt = 20;
constexpr int kStrobesOffS = 180;
constexpr int kMissedPoints = -6;

// The landing (D-158).
constexpr int kStarBandsFpm[] = {240, 360, 600, 900, 1200};  // up to each: 5, 4, 3, 2, 1 stars; above: 0
constexpr int kCrosswindKt = 15;
constexpr int kBankTenths = 50;
constexpr int kPitchMaxTenths737 = 80, kPitchMaxTenths777 = 75;
// PLANE PITCH DEGREES' sign: nose up is negative, as the SDK documents. Confirmed in the milestone 10 sim test
// (2026-11-25): a 100 fpm, 1.11 G touchdown in the BBJ2 logged -1.9. If it ever proves wrong, flip this and every
// stored flight re-scores (D-160).
constexpr int kSimPitchNoseUp = -1;
constexpr int kVrefSpreadKt = 10;
constexpr int kGCapTwo = 170, kGCapOne = 200;  // hundredths of G
constexpr int kLandingPoints[] = {-30, -20, -14, -8, -4, 0};  // by stars, 0-5

std::string duration(int s) {
    if (s < 60) return std::format("{} s", s);
    if (s % 60 == 0) return std::format("{} min", s / 60);
    return std::format("{} min {} s", s / 60, s % 60);
}

std::string thousands(int n) {
    const std::string digits = std::to_string(std::abs(n));
    std::string out;
    for (std::size_t i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0) out += ',';
        out += digits[i];
    }
    return n < 0 ? "-" + out : out;
}

std::string hpaOff(int tenths) { return std::format("{}.{} hPa off", tenths / 10, tenths % 10); }

std::string reasonText(const std::string& reason) {
    if (reason == "no_pmdg_data") return "No PMDG data";
    if (reason == "no_vref") return "VREF not entered";
    if (reason == "no_atis") return "No ATIS logged";
    if (reason == "diverted") return "Diverted";
    return "Not reached";
}

class Scorer {
public:
    explicit Scorer(const FlightMeasurements& m) : m_(m) {
        for (const auto& v : m.values) values_[v.part] = v.value;
        for (const auto& s : m.skipped) skips_[s.part] = s.reason;
    }

    ScoredFlight run() {
        ScoredFlight s;
        add(s, "Beacon on before pushback", {"beacon_late_s"}, [&] {
            const int v = at("beacon_late_s");
            return std::pair{v <= kBeaconLateS, v == 0 ? std::string("On before pushback") : duration(v) + " late"};
        });
        add(s, "Seat belt sign, taxi to 10,000 ft", {"belts_off_departure_s"},
            [&] { return offFor("belts_off_departure_s"); });
        add(s, "Taxi speed", {"taxi_max_kt"}, [&] {
            const int v = at("taxi_max_kt");
            return std::pair{v <= kTaxiMaxKt, std::format("{} kt", v)};
        });
        add(s, "Origin altimeter", {"origin_baro_off_tenths"}, [&] { return baro("origin_baro_off_tenths"); });
        add(s, "Lights for takeoff", {"takeoff_strobes", "takeoff_landing_lights"}, [&] { return takeoffLights(); });
        add(s, "250 kt below 10,000 ft", {"over_250_s"}, [&] {
            const int v = at("over_250_s");
            return std::pair{v <= kOver250S, v == 0 ? std::string("Never above 260 kt") : duration(v) + " above 260 kt"};
        });
        add(s, "STD by the transition altitude", {"std_late_s"}, [&] {
            const int v = at("std_late_s");
            return std::pair{v <= kStdLateS, v == 0 ? std::string("Set by the transition altitude")
                                                    : duration(v) + " after the transition altitude"};
        });
        add(s, "Seat belt sign for the descent", {"belts_off_descent_s"}, [&] { return offFor("belts_off_descent_s"); });
        add(s, "Destination altimeter", {"destination_baro_off_tenths"},
            [&] { return baro("destination_baro_off_tenths"); });
        add(s, "Landing lights below 10,000 ft", {"landing_lights_off_s"}, [&] { return offFor("landing_lights_off_s"); });
        add(s, "Stable at 1,000 ft",
            {"stable_gear_down", "stable_landing_flaps", "stable_sink_fpm", "stable_vref_offset_kt", "stable_lined_up"},
            [&] { return stable(); });
        add(s, "Strobes off after landing", {"strobes_on_after_landing_s"}, [&] {
            const int v = at("strobes_on_after_landing_s");
            return std::pair{v <= kStrobesOffS,
                             v == 0 ? std::string("Off before the taxi") : "Off " + duration(v) + " into the taxi"};
        });
        landing(s);
        int score = 100 + s.landingPoints;
        for (const auto& c : s.checks) score += c.points;
        s.score = std::max(0, score);
        return s;
    }

private:
    using Judge = std::function<std::pair<bool, std::string>()>;

    bool has(const char* part) const { return values_.contains(part); }
    int at(const char* part) const { return values_.at(part); }

    // A check is not checked when none of its parts was measured; its detail is the first part's reason.
    void add(ScoredFlight& s, const char* label, std::initializer_list<const char*> parts, const Judge& judge) {
        CheckOutcome c;
        c.label = label;
        const bool any = std::any_of(parts.begin(), parts.end(), [&](const char* p) { return has(p); });
        if (!any) {
            const auto it = skips_.find(*parts.begin());
            c.detail = reasonText(it == skips_.end() ? "" : it->second);
        } else {
            const auto [passed, detail] = judge();
            c.state = passed ? CheckState::Passed : CheckState::Missed;
            c.detail = detail;
            c.points = passed ? 0 : kMissedPoints;
        }
        s.checks.push_back(std::move(c));
    }

    std::pair<bool, std::string> offFor(const char* part) const {
        const int v = at(part);
        return {v == 0, v == 0 ? std::string("On throughout") : "Off for " + duration(v)};
    }

    std::pair<bool, std::string> baro(const char* part) const {
        const int v = at(part);
        return {v <= kBaroOffTenths, hpaOff(v)};
    }

    std::pair<bool, std::string> takeoffLights() const {
        const bool strobes = !has("takeoff_strobes") || at("takeoff_strobes") == 1;
        const bool lights = !has("takeoff_landing_lights") || at("takeoff_landing_lights") == 1;
        std::string detail;
        if (strobes && lights) detail = "Strobes and landing lights on";
        else if (!strobes && !lights) detail = "Strobes and landing lights off";
        else detail = strobes ? "Landing lights off" : "Strobes off";
        return {strobes && lights, detail};
    }

    std::pair<bool, std::string> stable() const {
        bool ok = true;
        std::vector<std::string> parts;
        if (has("stable_gear_down")) {
            const bool down = at("stable_gear_down") == 1;
            ok &= down;
            parts.push_back(down ? "gear down" : "gear up");
        }
        if (has("stable_landing_flaps")) {
            const bool flaps = at("stable_landing_flaps") == 1;
            ok &= flaps;
            parts.push_back(flaps ? "landing flaps" : "not landing flaps");
        }
        if (has("stable_sink_fpm")) {
            ok &= at("stable_sink_fpm") <= kStableSinkFpm;
            parts.push_back(thousands(at("stable_sink_fpm")) + " fpm");
        }
        if (has("stable_vref_offset_kt")) {
            const int v = at("stable_vref_offset_kt");
            ok &= v >= kStableVrefBelowKt && v <= kStableVrefAboveKt;
            parts.push_back(std::format("VREF {}{}", v >= 0 ? "+" : "", v));
        }
        if (has("stable_lined_up")) {
            const bool lined = at("stable_lined_up") == 1;
            ok &= lined;
            parts.push_back(lined ? "lined up" : "not lined up");
        }
        std::string detail;
        for (const auto& p : parts) detail += (detail.empty() ? "" : ", ") + p;
        return {ok, detail};
    }

    void landing(ScoredFlight& s) const {
        if (!m_.landing) return;
        const LandingFacts& f = *m_.landing;
        int stars = 0;
        for (int i = 0; i < 5; ++i) {
            if (f.vsFpm <= kStarBandsFpm[i]) {
                stars = 5 - i;
                break;
            }
        }
        const double off = (f.windDirDeg - f.headingTrueDeg) * std::numbers::pi / 180.0;
        const int cross = static_cast<int>(std::lround(std::abs(f.windKt * std::sin(off))));
        s.crosswindKt = cross;
        if (cross >= kCrosswindKt) stars = std::min(5, stars + 1);
        stars -= f.bounces;
        if (f.bankTenths && std::abs(*f.bankTenths) > kBankTenths) --stars;
        const int pitchMax = m_.family == Family::B777 ? kPitchMaxTenths777 : kPitchMaxTenths737;
        if (const auto pitch = pitchUpTenths(f); pitch && (*pitch < 0 || *pitch > pitchMax)) --stars;
        if (f.vrefKt && std::abs(f.iasKt - *f.vrefKt) > kVrefSpreadKt) --stars;
        if (f.gHundredths >= kGCapOne) stars = std::min(stars, 1);
        else if (f.gHundredths >= kGCapTwo) stars = std::min(stars, 2);
        stars = std::clamp(stars, 0, 5);
        s.stars = stars;
        s.landingPoints = kLandingPoints[stars];
    }

    const FlightMeasurements& m_;
    std::map<std::string, int, std::less<>> values_;
    std::map<std::string, std::string, std::less<>> skips_;
};

std::optional<double> mean(const std::vector<double>& v) {
    if (v.empty()) return std::nullopt;
    double sum = 0;
    for (double x : v) sum += x;
    return sum / static_cast<double>(v.size());
}

}  // namespace

ScoredFlight scoreFlight(const FlightMeasurements& m) { return Scorer(m).run(); }

Averages averages(const std::vector<ScoredFlight>& newestFirst) {
    std::vector<double> scores, scores10, stars, stars10;
    for (std::size_t i = 0; i < newestFirst.size(); ++i) {
        const ScoredFlight& f = newestFirst[i];
        scores.push_back(f.score);
        if (i < 10) scores10.push_back(f.score);
        if (f.stars) {
            stars.push_back(*f.stars);
            if (i < 10) stars10.push_back(*f.stars);
        }
    }
    return {mean(scores), mean(scores10), mean(stars), mean(stars10)};
}

std::optional<int> pitchUpTenths(const LandingFacts& f) {
    if (!f.simPitchTenths) return std::nullopt;
    return kSimPitchNoseUp * *f.simPitchTenths;
}

std::string starsText(int stars) {
    std::string out;
    for (int i = 0; i < 5; ++i) out += i < stars ? "★" : "☆";
    return out;
}

}  // namespace blocktime
