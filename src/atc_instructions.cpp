// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atc_instructions.h"

#include <cctype>
#include <regex>

#include "clearance.h"
#include "money.h"
#include "sayintentions.h"

namespace blocktime {

namespace {

constexpr auto kIcase = std::regex::ECMAScript | std::regex::icase;

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

// "FL220", or "5,000" from "5000" / "5,000".
std::string altitudeText(bool flightLevel, const std::string& number) {
    std::string digits;
    for (char c : number)
        if (std::isdigit(static_cast<unsigned char>(c))) digits += c;
    if (digits.empty()) return {};
    if (flightLevel) return "FL" + digits;
    return formatNumber(std::stoll(digits));
}

// "Terminal Alpha Gate 4" → "TERMINAL A GATE 4": taxiway and terminal letters as ATC spells them.
std::string withLetters(const std::string& text) {
    static const std::regex word(
        R"(\b(alpha|alfa|bravo|charlie|delta|echo|foxtrot|golf|hotel|india|juliett?|kilo|lima|mike|november|oscar|papa|quebec|romeo|sierra|tango|uniform|victor|whiskey|x-?ray|yankee|zulu)\b)",
        kIcase);
    std::string out;
    auto from = text.cbegin();
    for (std::sregex_iterator it(text.begin(), text.end(), word), end; it != end; ++it) {
        out.append(from, (*it)[0].first);
        out += static_cast<char>(std::toupper(static_cast<unsigned char>((*it)[1].str()[0])));
        from = (*it)[0].second;
    }
    out.append(from, text.cend());
    return upper(out);
}

// A taxi route: letters, and "A 7" joined to "A7".
std::string taxiRoute(const std::string& text) {
    static const std::regex letterNumber(R"(\b([A-Z]) (\d+)\b)");
    return std::regex_replace(withLetters(text), letterNumber, "$1$2");
}

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == ',' || s.back() == '.')) s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    return s;
}

// "ILS", "RNAV" + "Y" → "RNAV-Y", "Sweetwater" + "Visual" → "SWEETWATER VISUAL" (groups 1-3 of kApproach).
std::string approachType(const std::smatch& m) {
    std::string type = upper(m[2].str());
    if (m[3].matched) type += "-" + upper(m[3].str());
    if (m[1].matched) type = upper(m[1].str()) + " " + type;
    return type;
}

// The clearance's ALTITUDE and ROUTE (D-197): the initial altitude with the expected level as its aside.
std::vector<SlotChange> fromClearance(const Clearance& c, const RoutePlan& plan) {
    std::vector<SlotChange> out;
    bool changed = false;
    if (const std::string sid = planSidForm(c.sid, plan.sid, plan.sidTransition, &changed); !sid.empty()) {
        out.push_back({Slot::Route, "ROUTE", sid, ""});
    }
    std::string altitude = upper(c.initialAltitude);
    if (altitude.size() > 3 && altitude.ends_with(" FT")) altitude.resize(altitude.size() - 3);
    std::string aside;
    if (!c.expectLevel.empty()) {
        aside = "EXP " + c.expectLevel;
        if (c.expectMinutes > 0) aside += " +" + std::to_string(c.expectMinutes) + " MIN";
    }
    if (!altitude.empty() || !aside.empty()) out.push_back({Slot::Altitude, "ALTITUDE", altitude, aside});
    return out;
}

// A named visual approach's name ("Sweetwater Visual"), the type, its letter, the runway (0.15.1).
constexpr const char* kApproach =
    R"((?:(?!(?:a|an|the|expect|cleared|for|and|or) )(\w+) (?=visual))?)"  // not an article or a verb
    R"((ILS|LOC|RNAV|GPS|VOR|VISUAL)(?:[- ]([A-Z]))?\s+(?:runway )?(\w+) approach)";

// Built once (review 5.2): MSVC's std::regex is slow to construct, and the ATC card reads every message.
const std::regex kCross(R"(\bcross[- ]runway (\w+))", kIcase);
const std::regex kTaxiToVia(R"(\btaxi to (.+?) via (.+?)(,\s*hold short\b.*)?$)", kIcase);
const std::regex kContinueTaxi(R"(\bcontinue taxi via (.+?)(,\s*hold short\b.*)?$)", kIcase);
const std::regex kRouteTo(R"(^(.+) to (.+)$)", kIcase);
const std::regex kHoldShort(R"(\bhold short (?:of )?runway (\w+))", kIcase);
const std::regex kHeading(R"(\b(?:turn (left|right) )?(?:fly )?heading (\d{3})\b)", kIcase);
const std::regex kVia(R"(\b(climb|descend) via (?:the )?(\w+)(?:,?\s+(\w+) transition)?)", kIcase);
const std::regex kMaintain(R"(\b(?:climb|descend)(?: and)? maintain (flight level |FL ?)?([\d,]+)( until established)?)",
                           kIcase);
const std::regex kKnots(R"(\bmaintain (\d{3}) knots\b)", kIcase);
const std::regex kResumeSpeed(R"(\bresume normal speed\b)", kIcase);
const std::regex kDirect(R"(\bdirect (\w+))", kIcase);
const std::regex kOwnNav(R"(\bresume own navigation\b)", kIcase);
const std::regex kExpectApproach(std::string(R"(\bexpect (?:the |an? )?)") + kApproach +
                                     R"((?:.*?\bvia (?:the )?(\w+) transition)?)",
                                 kIcase);
const std::regex kClearedApproach(std::string(R"(\bcleared (?:for )?(?:the |an? )?)") + kApproach +
                                      R"((?:,?\s+(?:via )?(?:the )?(\w+) transition|,?\s+via (?:the )?(\w+))?)",
                                  kIcase);
const std::regex kClearedToLand(R"(\bcleared to land\b)", kIcase);
const std::regex kRunway(R"(\brunway (\w+))", kIcase);
const std::regex kSentenceStop(R"(\.(\s+|$))");

// One sentence's changes, in the order its patterns are checked. `before` is the call up to this sentence.
void readSentence(const std::string& s, const std::string& before, std::vector<SlotChange>& out) {
    std::smatch m;
    // A crossing first, then the route, then the hold short that still applies ("cross-runway 1R, then continue taxi
    // via Bravo, hold short of runway 1L").
    if (std::regex_search(s, m, kCross)) {
        out.push_back({Slot::HoldShort, "HOLD SHORT", "CROSS " + normalizeRunway(m[1].str()), ""});
    }
    if (const auto taxi = parseTaxi(s)) {
        out.push_back({Slot::Taxi, "TAXI", taxiRoute(taxi->route), taxi->runway.empty() ? "" : "RWY " + taxi->runway});
    } else if (std::regex_search(s, m, kTaxiToVia)) {
        out.push_back({Slot::Taxi, "TAXI", taxiRoute(trimmed(m[2].str())), withLetters(trimmed(m[1].str()))});
    } else if (std::regex_search(s, m, kContinueTaxi)) {
        // "… via B, C6 to Terminal 3 Gate E7A" names where it leads; without "to" it still leads where it did.
        const std::string text = trimmed(m[1].str());
        std::smatch to;
        if (std::regex_search(text, to, kRouteTo)) {
            out.push_back({Slot::Taxi, "TAXI", taxiRoute(trimmed(to[1].str())), withLetters(trimmed(to[2].str()))});
        } else {
            SlotChange route{Slot::Taxi, "TAXI", taxiRoute(text), ""};
            route.keepAside = true;
            out.push_back(route);
        }
    }
    if (std::regex_search(s, m, kHoldShort)) {
        out.push_back({Slot::HoldShort, "HOLD SHORT", normalizeRunway(m[1].str()), ""});
    }
    if (std::regex_search(s, m, kHeading)) {
        out.push_back({Slot::Route, "HEADING", m[2].str(), upper(m[1].str())});
    }
    if (std::regex_search(s, m, kVia)) {
        const std::string name = upper(m[2].str());
        const bool procedureWord = name == "SID" || name == "STAR";
        out.push_back({Slot::Altitude, "ALTITUDE", upper(m[1].str()) + " VIA " + name, ""});
        if (!procedureWord) {
            out.push_back({Slot::Route, "ROUTE", m[3].matched ? name + " " + upper(m[3].str()) : name, ""});
        }
    }
    if (std::regex_search(s, m, kMaintain)) {
        out.push_back({Slot::Altitude, "ALTITUDE", altitudeText(m[1].matched, m[2].str()),
                       m[3].matched ? "UNTIL ESTABLISHED" : ""});
    }
    if (std::regex_search(s, m, kKnots)) {
        out.push_back({Slot::Speed, "SPEED", m[1].str() + " KT", ""});
    } else if (std::regex_search(s, kResumeSpeed)) {
        out.push_back({Slot::Speed, "SPEED", "", ""});
    }
    if (std::regex_search(s, m, kDirect)) {
        out.push_back({Slot::Route, "ROUTE", "DIRECT " + upper(m[1].str()), ""});
    } else if (std::regex_search(s, kOwnNav)) {
        out.push_back({Slot::Route, "ROUTE", "OWN NAV", ""});
    }
    if (std::regex_search(s, m, kExpectApproach)) {
        out.push_back({Slot::Approach, "APPROACH", "EXP " + approachType(m) + " " + normalizeRunway(m[4].str()),
                       m[5].matched ? upper(m[5].str()) : ""});
    } else if (std::regex_search(s, m, kClearedApproach)) {
        SlotChange c{Slot::Approach, "APPROACH", "CLEARED " + approachType(m) + " " + normalizeRunway(m[4].str()), ""};
        const std::string transition = m[5].matched ? m[5].str() : m[6].matched ? m[6].str() : "";
        if (!transition.empty()) {
            c.value += " " + upper(transition);  // ATC's transition is always shown when given
        } else {
            c.keepAside = true;  // the expect call's transition still applies (0.15.1)
        }
        out.push_back(c);
    }
    if (std::regex_search(s, kClearedToLand)) {
        // The runway is said before it: "Runway 30L, Winds 360 at 4. Cleared to land."
        const std::string said = before + " " + s;
        std::string runway;
        for (std::sregex_iterator it(said.begin(), said.end(), kRunway), end; it != end; ++it) runway = (*it)[1].str();
        out.push_back({Slot::Approach, "APPROACH",
                       runway.empty() ? "CLEARED TO LAND" : "CLEARED TO LAND " + normalizeRunway(runway), ""});
    }
}

}  // namespace

std::vector<SlotChange> readInstructions(std::string_view atcWords, const RoutePlan& plan) {
    const std::string words(atcWords);
    if (const auto clearance = parseClearance(words)) return fromClearance(*clearance, plan);
    std::vector<SlotChange> out;
    // Sentences end at a full stop followed by a space or the end ("30.19" stays whole).
    std::string before;
    auto from = words.cbegin();
    for (std::sregex_iterator it(words.begin(), words.end(), kSentenceStop), end;; ++it) {
        const bool last = it == end;
        const std::string sentence(from, last ? words.cend() : (*it)[0].first);
        if (!sentence.empty()) readSentence(sentence, before, out);
        before += sentence + ". ";
        if (last) break;
        from = (*it)[0].second;
    }
    return out;
}

}  // namespace blocktime
