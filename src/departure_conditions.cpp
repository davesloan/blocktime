// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "departure_conditions.h"

#include <cctype>
#include <charconv>
#include <format>
#include <regex>
#include <sstream>
#include <vector>

namespace blocktime {

namespace {

constexpr auto kIcase = std::regex::ECMAScript | std::regex::icase;

// The METAR's groups up to the remarks, which use the same letters for other things ("RAB15": rain began at :15).
std::vector<std::string> metarGroups(std::string_view metar) {
    std::istringstream in{std::string(metar)};
    std::vector<std::string> groups;
    for (std::string g; in >> g;) {
        if (g == "RMK") break;
        groups.push_back(g);
    }
    return groups;
}

int toInt(const std::string& s) {
    int v = 0;
    std::from_chars(s.data(), s.data() + s.size(), v);
    return v;
}

}  // namespace

std::optional<Wind> atisWind(std::string_view atisText) {
    const std::string text(atisText);
    std::smatch m;
    if (!std::regex_search(text, m,
                           std::regex(R"(\bWINDS? (CALM|VRB AT (\d+)|(\d{3}) AT (\d+)(?: GUSTS? (\d+))?))", kIcase))) {
        return std::nullopt;
    }
    Wind w;
    if (m[2].matched) {
        w.variable = true;
        w.speedKt = toInt(m[2].str());
    } else if (m[3].matched) {
        w.directionDeg = toInt(m[3].str());
        w.speedKt = toInt(m[4].str());
        if (m[5].matched) w.gustKt = toInt(m[5].str());
    } else {
        w.calm = true;
    }
    return w;
}

std::optional<Wind> metarWind(std::string_view metar) {
    static const std::regex group(R"(^(\d{3}|VRB)(\d{2,3})(?:G(\d{2,3}))?KT$)");
    for (const std::string& g : metarGroups(metar)) {
        std::smatch m;
        if (!std::regex_match(g, m, group)) continue;
        Wind w;
        w.speedKt = toInt(m[2].str());
        if (m[3].matched) w.gustKt = toInt(m[3].str());
        if (m[1].str() == "VRB") {
            w.variable = true;
        } else {
            w.directionDeg = toInt(m[1].str());
            w.calm = w.speedKt == 0;
        }
        return w;
    }
    return std::nullopt;
}

std::string windText(const Wind& w) {
    if (w.calm) return "Calm";
    const std::string speed = w.gustKt > 0 ? std::format("{}G{} kt", w.speedKt, w.gustKt) : std::format("{} kt", w.speedKt);
    if (w.variable) return "Variable / " + speed;
    return std::format("{:03}° / ", w.directionDeg) + speed;
}

std::optional<int> metarTemperatureC(std::string_view metar) {
    static const std::regex group(R"(^(M?)(\d{2})/(M?\d{2})?$)");
    for (const std::string& g : metarGroups(metar)) {
        std::smatch m;
        if (std::regex_match(g, m, group)) return (m[1].matched && m[1].length() > 0 ? -1 : 1) * toInt(m[2].str());
    }
    return std::nullopt;
}

bool metarPrecipitation(std::string_view metar) {
    static const std::regex group(R"(^[-+]?(MI|BC|PR|DR|BL|SH|TS|FZ)?(DZ|RA|SN|SG|PL|GR|GS|UP)+$)");
    for (const std::string& g : metarGroups(metar)) {
        if (std::regex_match(g, group)) return true;
    }
    return false;
}

std::string altimeterText(std::string_view altimeter) {
    if (altimeter.empty()) return {};
    double value = 0;
    std::from_chars(altimeter.data(), altimeter.data() + altimeter.size(), value);
    return std::string(altimeter) + (value > 100 ? " hPa" : " inHg");
}

std::string phoneticWord(std::string_view letter) {
    static constexpr const char* kWords[] = {"ALFA",  "BRAVO",  "CHARLIE", "DELTA",   "ECHO",  "FOXTROT", "GOLF",
                                             "HOTEL", "INDIA",  "JULIETT", "KILO",    "LIMA",  "MIKE",    "NOVEMBER",
                                             "OSCAR", "PAPA",   "QUEBEC",  "ROMEO",   "SIERRA", "TANGO",  "UNIFORM",
                                             "VICTOR", "WHISKEY", "XRAY",  "YANKEE",  "ZULU"};
    if (letter.size() != 1) return "";
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(letter[0])));
    if (c < 'A' || c > 'Z') return "";
    return kWords[c - 'A'];
}

std::string atisApproaches(std::string_view atisText) {
    const std::string text(atisText);
    static const std::regex approach(R"(\b(ILS|LOC|RNAV(?:-[A-Z])?|GPS|VOR) (?:RWY|RUNWAY) (\w+))", kIcase);
    std::string out;
    for (std::sregex_iterator it(text.begin(), text.end(), approach), end; it != end; ++it) {
        std::string type = (*it)[1].str();
        for (char& c : type) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        out += (out.empty() ? "" : " · ") + type + " " + (*it)[2].str();
    }
    return out;
}

std::string runwayList(std::string_view atisRunways) {
    std::string out;
    for (char c : atisRunways) {
        if (c == ',' || c == ' ') {
            if (!out.empty() && out.back() != ' ') out += ' ';
        } else {
            out += c;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

}  // namespace blocktime
