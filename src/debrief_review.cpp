// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debrief_review.h"

#include <algorithm>
#include <cmath>
#include <format>
#include <set>

#include <nlohmann/json.hpp>

#include "debrief_score.h"
#include "event_log.h"

namespace blocktime {

namespace {

using Time = std::chrono::system_clock::time_point;

constexpr const char* kSystemPrompt = R"(You debrief airline flights flown by Dave, a home flight simulator pilot, in the PMDG 737 or 777 on Microsoft Flight Simulator 2020 with SayIntentions for ATC. He wants honest feedback on how well each flight was flown and what to work on next.

You get the flight's checks (already judged by fixed rules), the landing's facts and rating, a timeline of the flight, the ATC exchanges, the ATIS reports and the checks he missed on his last few flights. Write the review from those facts only; never invent a value that isn't there.

- summary: two to four sentences on the flight as a whole: what went well, and the pattern behind what didn't.
- work_on: the things to work on, most important first, each one short sentence with the practical fix. Leave it empty if there's nothing.
- landing: explain the landing from the facts: the approach into it (the state at 1,000 ft, the autopilot disconnect, the sink on short final) and the touchdown itself.
- atc: ATC compliance, for information only: compare clearances with what was flown. SayIntentions is sometimes wrong or late: when an instruction looks wrong or came late, say so instead of calling it a bust. Empty if there's nothing worth saying.

Connect cause and effect where the timeline shows it. A go-around is never a mistake. Runway centreline and touchdown-zone figures don't count. Point out a habit when a miss repeats on earlier flights. Don't repeat the score or the star rating, and no other numeric grades. Plain text, no markdown, about 300 words in all.)";

std::string hhmmss(Time t) { return formatUtc(t).substr(11, 8); }

Time when(const LogLine& l) {
    if (l.data.is_object() && l.data.contains("time") && l.data["time"].is_string()) {
        if (auto t = parseUtc(l.data["time"].get<std::string>())) return *t;
    }
    return l.ts;
}

std::string str(const nlohmann::json& d, const char* key) {
    if (!d.contains(key)) return "";
    const auto& v = d[key];
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<long long>());
    if (v.is_number()) return std::format("{:g}", v.get<double>());
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    return "";
}

long long whole(const nlohmann::json& d, const char* key) {
    return d.contains(key) && d[key].is_number() ? std::llround(d[key].get<double>()) : 0;
}

std::string markerName(const std::string& m) {
    if (m == "above_10000") return "passing 10,000 ft climbing";
    if (m == "below_10000") return "passing 10,000 ft descending";
    if (m == "transition_altitude") return "transition altitude";
    if (m == "transition_level") return "transition level";
    if (m == "within_200nm") return "200 nm from the destination";
    if (m == "final_approach") return "1,000 ft above the runway";
    return m;
}

const char* beltsText(int v) { return v == 2 ? "on" : v == 1 ? "auto" : "off"; }

int enginesRunning(const nlohmann::json& d) {
    int n = 0;
    if (d.contains("eng_running"))
        for (const auto& e : d["eng_running"]) n += e.get<bool>() ? 1 : 0;
    return n;
}

struct Line {
    Time t;
    std::string text;
};

// The flight's own events, one line each.
std::optional<std::string> eventText(const LogLine& l) {
    const auto& d = l.data;
    const std::string& t = l.type;
    if (t == "block_out") {
        return std::format("block out (parking brake released){}, {} lb of fuel",
                           str(d, "parking").empty() ? "" : " at " + str(d, "parking"), whole(d, "fuel_lb"));
    }
    if (t == "block_in") {
        const long long s = whole(d, "block_time_s");
        return std::format("block in (parking brake set){}, block time {}:{:02}",
                           str(d, "parking").empty() ? "" : " at " + str(d, "parking"), s / 3600, s / 60 % 60);
    }
    if (t == "phase_changed") return std::format("phase: {} at {} ft", str(d, "phase"), whole(d, "alt_ft"));
    if (t == "flight_marker") return std::format("{} ({} ft)", markerName(str(d, "marker")), whole(d, "alt_ft"));
    if (t == "level_off") return std::format("levelled at {} ft", whole(d, "alt_ft"));
    if (t == "level_off_ended") return std::format("left the level, at {} ft", whole(d, "alt_ft"));
    if (t == "liftoff") return std::format("liftoff, runway {}, {} kt", str(d, "runway"), whole(d, "ias_kt"));
    if (t == "lined_up") {
        return std::format("lined up with runway {}, {} nm out at {} ft", str(d, "runway"), str(d, "distance_nm"),
                           whole(d, "alt_ft"));
    }
    if (t == "touchdown") {
        return std::format("touchdown, {} fpm, {} G, {} kt", std::llabs(whole(d, "vs_fpm")), str(d, "g"),
                           whole(d, "ias_kt"));
    }
    if (t == "bounces") return std::format("{} bounce(s) after touchdown", whole(d, "bounces"));
    if (t == "landing_runway") return "landed on runway " + str(d, "runway");
    if (t == "altimeter") {
        return std::format("altimeters set: captain {} hPa, first officer {} hPa", str(d, "capt_mb"), str(d, "fo_mb"));
    }
    if (t == "arrival_gate") {
        return std::format("arrival gate: {} ({}); SayIntentions' own: {}", str(d, "gate"), str(d, "result"),
                           str(d, "si_gate"));
    }
    if (t == "gsx_service" && str(d, "state") == "completed") return "GSX " + str(d, "service") + " completed";
    return std::nullopt;
}

// What changed in the cockpit since the last state, one line per change.
std::vector<std::string> cockpitChanges(const nlohmann::json* before, const nlohmann::json& now) {
    std::vector<std::string> out;
    const auto changed = [&](const char* key) {
        return now.contains(key) && (!before || !before->contains(key) || (*before)[key] != now[key]);
    };
    if (changed("seat_belts")) out.push_back(std::string("seat belt sign ") + beltsText(now["seat_belts"].get<int>()));
    if (changed("landing_lights")) out.push_back(now["landing_lights"].get<bool>() ? "landing lights on" : "landing lights off");
    if (changed("gear_down")) out.push_back(now["gear_down"].get<bool>() ? "gear down" : "gear up");
    if (changed("autopilot")) out.push_back(now["autopilot"].get<bool>() ? "autopilot on" : "autopilot off");
    if ((changed("vref_kt") || changed("landing_flaps")) && now.value("vref_kt", 0) > 0) {
        out.push_back(std::format("FMC: VREF {} kt, landing flaps {}", now.value("vref_kt", 0), now.value("landing_flaps", 0)));
    }
    return out;
}

std::string group(long long n) {
    std::string s = std::to_string(std::llabs(n));
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3) s.insert(static_cast<std::size_t>(i), ",");
    return (n < 0 ? "-" : "") + s;
}

}  // namespace

std::string buildTimeline(const std::vector<LogLine>& events) {
    std::vector<Line> lines;
    const nlohmann::json* lastCockpit = nullptr;
    const nlohmann::json* lastTel = nullptr;
    std::optional<Time> finalApproach, touchdown;
    for (const LogLine& l : events) {
        if (l.type == "flight_marker" && l.data.value("marker", "") == "final_approach") finalApproach = when(l);
        if (l.type == "touchdown") touchdown = when(l);
    }
    for (const LogLine& l : events) {
        if (auto text = eventText(l)) lines.push_back({when(l), *text});
        if (l.type == "cockpit") {
            for (auto& c : cockpitChanges(lastCockpit, l.data)) lines.push_back({l.ts, std::move(c)});
            lastCockpit = &l.data;
        }
        if (l.type == "telemetry") {
            const auto& d = l.data;
            const auto flip = [&](const char* key, const char* on, const char* off) {
                if (!d.contains(key) || (lastTel && lastTel->contains(key) && (*lastTel)[key] == d[key])) return;
                if (!lastTel && !d[key].get<bool>()) return;  // the first sample says nothing new when it's off
                lines.push_back({l.ts, d[key].get<bool>() ? on : off});
            };
            flip("beacon", "beacon on", "beacon off");
            flip("strobe", "strobes on", "strobes off");
            flip("parking_brake", "parking brake set", "parking brake released");
            const int before = lastTel ? enginesRunning(*lastTel) : 0;
            if (const int n = enginesRunning(d); n != before) {
                lines.push_back({l.ts, n == 0 ? std::string("engines off") : std::format("{} engine(s) running", n)});
            }
            lastTel = &d;
        }
    }
    // Condensed from the telemetry around the landing: the state at the 1,000 ft marker, then the sink to touchdown.
    if (finalApproach) {
        const nlohmann::json* at = nullptr;
        double maxSink = 0, sinkAgl = 0, iasLo = 1e9, iasHi = 0;
        for (const LogLine& l : events) {
            if (l.type != "telemetry") continue;
            if (l.ts <= *finalApproach) at = &l.data;
            if (l.ts > *finalApproach && touchdown && l.ts <= *touchdown && !l.data.value("on_ground", true)) {
                const double sink = -l.data.value("vs_fpm", 0.0);
                if (sink > maxSink) {
                    maxSink = sink;
                    sinkAgl = l.data.value("alt_agl_ft", 0.0);
                }
                iasLo = std::min(iasLo, l.data.value("ias_kt", 0.0));
                iasHi = std::max(iasHi, l.data.value("ias_kt", 0.0));
            }
        }
        if (at) {
            lines.push_back({*finalApproach, std::format("at the 1,000 ft marker: {} kt, sinking {} fpm, flaps handle {}",
                                                          whole(*at, "ias_kt"), group(-whole(*at, "vs_fpm")),
                                                          whole(*at, "flaps_handle"))});
        }
        if (touchdown && iasHi > 0) {
            lines.push_back({*touchdown - std::chrono::milliseconds(1),
                             std::format("from 1,000 ft to touchdown: {}-{} kt, highest sink {} fpm at {} ft above "
                                         "the ground",
                                         std::lround(iasLo), std::lround(iasHi), group(std::lround(maxSink)),
                                         std::lround(sinkAgl))});
        }
    }
    std::stable_sort(lines.begin(), lines.end(), [](const Line& a, const Line& b) { return a.t < b.t; });
    std::string out;
    for (const Line& l : lines) out += hhmmss(l.t) + " " + l.text + "\n";
    return out;
}

std::string buildReviewRequest(const ReviewInput& in) {
    const ScoredFlight scored = scoreFlight(in.measurements);
    std::string user = "Flight: " + in.header + "\n\nChecks (judged by BlockTime's rules):\n";
    for (const CheckOutcome& c : scored.checks) {
        const char* state = c.state == CheckState::Passed ? "passed" : c.state == CheckState::Missed ? "missed"
                                                                                                    : "not checked";
        user += std::format("- {}: {} ({})\n", c.label, state, c.detail);
    }
    if (const auto& f = in.measurements.landing) {
        user += std::format("\nLanding: {} of 5 stars. {} fpm, {}.{:02} G, {} bounce(s), {} kt", *scored.stars, f->vsFpm,
                            f->gHundredths / 100, f->gHundredths % 100, f->bounces, f->iasKt);
        if (f->vrefKt) user += std::format(" (VREF {} kt)", *f->vrefKt);
        if (f->pitchTenths) user += std::format(", pitch {:.1f}°", *f->pitchTenths / 10.0);
        if (f->bankTenths) user += std::format(", bank {:.1f}°", *f->bankTenths / 10.0);
        user += std::format(", wind {:03}° at {} kt ({} kt across the runway)\n", f->windDirDeg, f->windKt,
                            scored.crosswindKt.value_or(0));
    } else {
        user += "\nLanding: no touchdown was logged.\n";
    }
    user += "\nTimeline (UTC):\n" + buildTimeline(in.events);

    user += "\nATC exchanges (UTC; \"Pilot\" is Dave's side, spoken by him or his AI co-pilot):\n";
    std::set<long long> seen;
    for (const LogLine& l : in.events) {
        if (l.type != "atc_exchange" || !l.data.contains("entry")) continue;
        const auto& e = l.data["entry"];
        if (e.value("channel", "").rfind("COM", 0) != 0) continue;  // cabin crew and PA are on INTERCOM
        const long long id = e.contains("id") && e["id"].is_number() ? e["id"].get<long long>() : -1;
        if (id >= 0 && !seen.insert(id).second) continue;
        const std::string at = e.value("stamp_zulu", "").size() >= 19 ? e.value("stamp_zulu", "").substr(11, 8)
                                                                       : hhmmss(l.ts);
        if (e.contains("incoming_message") && e["incoming_message"].is_string() &&
            !e["incoming_message"].get<std::string>().empty()) {
            user += at + " Pilot: " + e["incoming_message"].get<std::string>() + "\n";
        }
        if (e.contains("outgoing_message") && e["outgoing_message"].is_string() &&
            !e["outgoing_message"].get<std::string>().empty()) {
            user += at + " " + e.value("station_name", "ATC") + ": " + e["outgoing_message"].get<std::string>() + "\n";
        }
    }

    user += "\nATIS:\n";
    std::set<std::string> letters;
    for (const LogLine& l : in.events) {
        if (l.type != "atis") continue;
        const std::string key = l.data.value("icao", "") + l.data.value("letter", "");
        if (!letters.insert(key).second) continue;
        user += std::format("{} ({}): {}\n", l.data.value("icao", ""), l.data.value("role", ""), l.data.value("text", ""));
    }

    user += "\nChecks missed on earlier flights:\n";
    if (in.recentMisses.empty()) user += "- no earlier flights\n";
    for (std::size_t i = 0; i < in.recentMisses.size(); ++i) {
        std::string misses;
        for (const auto& m : in.recentMisses[i]) misses += (misses.empty() ? "" : ", ") + m;
        user += std::format("- {} flight{} ago: {}\n", i + 1, i == 0 ? "" : "s", misses.empty() ? "none" : misses);
    }

    const nlohmann::json schema = {
        {"type", "object"},
        {"properties",
         {{"summary", {{"type", "string"}}},
          {"work_on", {{"type", "array"}, {"items", {{"type", "string"}}}}},
          {"landing", {{"type", "string"}}},
          {"atc", {{"type", "string"}}}}},
        {"required", {"summary", "work_on", "landing", "atc"}},
        {"additionalProperties", false},
    };
    const nlohmann::json body = {
        {"model", kReviewModel},
        {"max_tokens", 16000},
        {"thinking", {{"type", "adaptive"}}},
        {"output_config", {{"effort", "medium"}, {"format", {{"type", "json_schema"}, {"schema", schema}}}}},
        {"fallbacks", "default"},
        {"system", kSystemPrompt},
        {"messages", nlohmann::json::array({{{"role", "user"}, {"content", user}}})},
    };
    return body.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

ReviewAnswer parseReviewResponse(int httpStatus, std::string_view body) {
    const auto j = nlohmann::json::parse(body, nullptr, false);
    if (httpStatus >= 400) {
        if (j.is_object() && j.contains("error") && j["error"].is_object() && j["error"].contains("message") &&
            j["error"]["message"].is_string()) {
            return {std::nullopt, "Claude: " + j["error"]["message"].get<std::string>()};
        }
        return {std::nullopt, std::format("Claude answered HTTP {}", httpStatus)};
    }
    const ReviewAnswer unreadable{std::nullopt, "Claude's answer wasn't readable"};
    if (!j.is_object()) return unreadable;
    const std::string stop = j.value("stop_reason", "");
    if (stop == "refusal") return {std::nullopt, "Claude declined to write it"};
    if (stop == "max_tokens") return {std::nullopt, "Claude's answer was cut off"};
    if (!j.contains("content") || !j["content"].is_array()) return unreadable;
    for (const auto& block : j["content"]) {
        if (block.value("type", "") != "text" || !block.contains("text") || !block["text"].is_string()) continue;
        const auto r = nlohmann::json::parse(block["text"].get<std::string>(), nullptr, false);
        if (!r.is_object()) return unreadable;
        for (const char* key : {"summary", "landing", "atc"})
            if (!r.contains(key) || !r[key].is_string()) return unreadable;
        if (!r.contains("work_on") || !r["work_on"].is_array()) return unreadable;
        StoredReview review;
        review.summary = r["summary"].get<std::string>();
        review.landing = r["landing"].get<std::string>();
        review.atc = r["atc"].get<std::string>();
        for (const auto& item : r["work_on"])
            if (item.is_string() && !item.get<std::string>().empty()) review.workOn.push_back(item.get<std::string>());
        review.model = j.value("model", kReviewModel);
        return {review, ""};
    }
    return unreadable;
}

}  // namespace blocktime
