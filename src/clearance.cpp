// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "clearance.h"

#include <format>
#include <cctype>
#include <cstdlib>
#include <regex>

#include "money.h"
#include "sayintentions.h"

namespace blocktime {

namespace {

constexpr auto kIcase = std::regex::ECMAScript | std::regex::icase;

// The first capture of `pattern` in `text`, or "".
std::string capture(const std::string& text, const char* pattern) {
    std::smatch m;
    return std::regex_search(text, m, std::regex(pattern, kIcase)) ? m[1].str() : std::string();
}

// "5000" or "5,000" → "5,000 ft"; a flight level → "FL190".
std::string initialAltitude(const std::string& words) {
    // Only what comes before the first "expect": "Expect FL350" is the cruise ATC expects, not the climb.
    std::smatch expect;
    const std::string before =
        std::regex_search(words, expect, std::regex(R"(\bexpect\b)", kIcase)) ? expect.prefix().str() : words;
    if (std::regex_search(before, std::regex(R"(climb via the sid)", kIcase))) return "Climb via SID";
    std::smatch m;
    if (!std::regex_search(before, m, std::regex(R"(maintain (flight level |FL ?)?([\d,]+))", kIcase))) return {};
    std::string digits;
    for (char c : m[2].str()) {
        if (c != ',') digits += c;
    }
    if (digits.empty()) return {};
    if (m[1].matched) return "FL" + digits;
    return formatNumber(std::stoll(digits)) + " ft";
}

// "one-zero" → 10, "five" → 5, "10" → 10; 0 when it can't be read.
int spokenNumber(const std::string& words) {
    static const char* kDigits[] = {"zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "niner"};
    if (!words.empty() && std::isdigit(static_cast<unsigned char>(words[0]))) return std::atoi(words.c_str());
    if (words == "ten") return 10;
    if (words == "nine") return 9;
    int n = 0;
    std::size_t start = 0;
    while (start <= words.size()) {
        const std::size_t dash = words.find('-', start);
        const std::string digit = words.substr(start, dash == std::string::npos ? std::string::npos : dash - start);
        int d = -1;
        for (int i = 0; i < 10; ++i)
            if (digit == kDigits[i] || (i == 9 && digit == "nine")) d = i;
        if (d < 0) return 0;
        n = n * 10 + d;
        if (dash == std::string::npos) break;
        start = dash + 1;
    }
    return n;
}

}  // namespace

std::optional<Clearance> parseClearance(std::string_view atcWords) {
    const std::string words(atcWords);
    if (!std::regex_search(words, std::regex(R"(\bcleared to\b)", kIcase))) return std::nullopt;
    Clearance c;
    c.squawk = capture(words, R"(squawk (\d{4}))");
    if (c.squawk.empty()) return std::nullopt;
    c.sid = capture(words, R"(via the (\w+) departure)");
    c.initialAltitude = initialAltitude(words);
    c.departureFrequency = capture(words, R"(departure on (\d{3}\.\d{1,3}))");
    c.runway = normalizeRunway(capture(words, R"(expect runway (\w+))"));
    std::smatch expect;
    if (std::regex_search(words, expect,
                          std::regex(R"(\bexpect (flight level |FL ?)(\d+)(?: ([\w-]+) minutes? after departure)?)", kIcase))) {
        c.expectLevel = "FL" + expect[2].str();
        if (expect[3].matched) {
            std::string spoken = expect[3].str();
            for (char& ch : spoken) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            c.expectMinutes = spokenNumber(spoken);
        }
    }
    return c;
}

std::optional<TaxiInstruction> parseTaxi(std::string_view atcWords) {
    const std::string words(atcWords);
    std::smatch m;
    if (!std::regex_search(words, m, std::regex(R"(runway (\w+).*?\btaxi\b.*?\bvia\b\s*(.+))", kIcase))) {
        return std::nullopt;
    }
    std::string route = m[2].str();
    while (!route.empty() && (route.back() == '.' || route.back() == ' ')) route.pop_back();
    if (route.empty()) return std::nullopt;
    return TaxiInstruction{normalizeRunway(m[1].str()), route};
}

Countdown clearanceCountdown(TimePoint clearedAt, TimePoint now) {
    using namespace std::chrono;
    const auto left = clearedAt + kClearanceValidity - now;
    const long long s = std::max<long long>(0, duration_cast<seconds>(left).count());
    return Countdown{std::format("{}:{:02}", s / 60, s % 60), left <= minutes(5), left <= seconds(0)};
}

std::string planSidForm(std::string_view atcSid, std::string_view planSid, std::string_view planTransition,
                        bool* changed) {
    *changed = false;
    if (!atcSid.empty() && atcSid != planSid) {
        *changed = true;
        return std::string(atcSid);
    }
    std::string form(planSid);
    if (!form.empty() && !planTransition.empty()) form += " " + std::string(planTransition);
    return form;
}

}  // namespace blocktime
