// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dispatch_report.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <map>

namespace blocktime {

namespace {

bool isCrewLine(std::string_view category) {
    return category == "pilots" || category == "flight_attendants" || category == "maintenance";
}

std::string hoursMinutes(int seconds) {
    const int minutes = (seconds + 30) / 60;
    return std::format("{}:{:02}", minutes / 60, minutes % 60);
}

// Every board id and its amount: the lines, then "profit".
std::map<std::string, Cents> amounts(const Preview& p) {
    std::map<std::string, Cents> out;
    for (const auto* lines : {&p.income, &p.costs}) {
        for (const MoneyLine& m : *lines) out[m.category] += m.cents;
    }
    out["profit"] = p.totalCents;
    return out;
}

std::string signedPounds(int lb) { return (lb >= 0 ? "+" : "-") + formatNumber(std::abs(lb)) + " lb"; }

}  // namespace

std::string boardValue(Cents cents, BoardRow::Tone tone) {
    if (tone == BoardRow::Tone::Income) return "+" + formatMoneyFull(cents);
    return formatMoneyFull(cents);
}

std::vector<BoardRow> boardRows(const Preview& p) {
    std::vector<BoardRow> rows;
    for (const MoneyLine& m : p.income) {
        rows.push_back({m.category, boardLabel(m.category, m.label), boardValue(m.cents, BoardRow::Tone::Income),
                        BoardRow::Tone::Income});
    }
    for (const MoneyLine& m : p.costs) {
        rows.push_back({m.category, boardLabel(m.category, m.label), boardValue(m.cents, BoardRow::Tone::Cost),
                        BoardRow::Tone::Cost});
    }
    const BoardRow::Tone tone = p.totalCents < 0 ? BoardRow::Tone::Loss : BoardRow::Tone::Profit;
    rows.push_back({"profit", "Profit", boardValue(p.totalCents, tone), tone});
    return rows;
}

std::string boardLabel(std::string_view category, std::string_view ledgerLabel) {
    static const std::map<std::string_view, std::string_view> kShort{
        {"flight_attendants", "Cabin crew"}, {"cargo_handling", "Cargo hdlg"}, {"atc", "ATC / nav"},
        {"landing_fee", "Landing"},          {"gate", "Gate"},                 {"handling", "Handling"},
        {"ga_services", "GA services"},
    };
    const auto it = kShort.find(category);
    return std::string(it != kShort.end() ? it->second : ledgerLabel);
}

std::string boardText(const BoardRow& row, int tiles) {
    const std::string value = row.value.substr(0, static_cast<std::size_t>(tiles));
    const int room = std::max(0, tiles - static_cast<int>(value.size()) - 1);
    std::string label = row.label.substr(0, static_cast<std::size_t>(room));
    for (char& ch : label) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    const std::size_t gap = static_cast<std::size_t>(tiles) - label.size() - value.size();
    return label + std::string(gap, ' ') + value;
}

std::string ringFamily(std::string_view category) {
    if (category == "fuel") return "fuel";
    if (category == "profit") return "profit";
    if (category == "landing_fee" || category == "gate" || category == "handling" || category == "ga_services" ||
        category == "cargo_handling") {
        return "airport";
    }
    if (isCrewLine(category)) return "crew";
    return "service";
}

Ring ring(const Preview& p) {
    Ring r;
    Cents income = 0, costs = 0;
    for (const MoneyLine& m : p.income) income += m.cents;
    for (const MoneyLine& m : p.costs) costs -= m.cents;
    r.loss = p.totalCents < 0 || income <= 0;
    const Cents circle = r.loss ? costs : income;
    if (circle <= 0) return r;
    double at = 0;
    const auto slice = [&](const std::string& id, Cents cents) {
        if (cents <= 0) return;
        const double sweep = static_cast<double>(cents) / static_cast<double>(circle);
        r.slices.push_back({id, ringFamily(id), cents, at, sweep});
        at += sweep;
    };
    for (const MoneyLine& m : p.costs) slice(m.category, -m.cents);
    if (!r.loss) slice("profit", p.totalCents);
    return r;
}

std::vector<std::string> workingLines(const MoneyLine& line, const WorkingFacts& f) {
    std::vector<std::string> out;
    if (line.category == "fuel") {
        const int added = std::max(0, f.rampFuelLb - f.fuelOnRecordLb);
        out.push_back(formatNumber(f.rampFuelLb) + " lb block fuel");
        if (f.extraFuelLb > 0) out.push_back("includes " + formatNumber(f.extraFuelLb) + " lb extra fuel");
        out.push_back("− " + formatNumber(f.fuelOnRecordLb) + " lb on board");
        out.push_back("= " + formatNumber(added) + " lb added × " + rateWorkingText(line));
        out.push_back("Price locked when you dispatched");
        return out;
    }
    if (const std::string w = workingText(line); !w.empty()) out.push_back(w);
    if (isCrewLine(line.category) && !f.locked) out.push_back("Charged on the actual block time");
    return out;
}

std::vector<LineChange> changesSince(const Preview& before, const ChangeFacts& b, const Preview& after,
                                     const ChangeFacts& a) {
    std::vector<LineChange> out;
    const auto was = amounts(before);
    // In the board's order: the new preview's lines, a line that went away, then the profit.
    std::vector<std::string> ids;
    for (const BoardRow& r : boardRows(after)) ids.push_back(r.id);
    for (const auto& [id, cents] : was) {
        if (std::find(ids.begin(), ids.end(), id) == ids.end()) ids.insert(ids.end() - 1, id);
    }
    const auto now = amounts(after);
    for (const std::string& id : ids) {
        const Cents w = was.contains(id) ? was.at(id) : 0, n = now.contains(id) ? now.at(id) : 0;
        if (w == n) continue;
        LineChange c{id, w, n, ""};
        if (id == "profit") {
            const Cents delta = n - w;
            c.why = (delta > 0 ? "+" : "") + formatMoneyFull(delta) + " since the last import";
        } else {
            c.why = "Was " + formatMoneyFull(w) + " before the re-import";
            if (id == "fuel" && a.fuelAddedLb != b.fuelAddedLb) {
                const int d = a.fuelAddedLb - b.fuelAddedLb;
                c.why += ": " + formatNumber(std::abs(d)) + " lb " + (d > 0 ? "more" : "less") + " fuel to add";
                if (a.extraFuelLb != b.extraFuelLb) c.why += " (extra fuel " + signedPounds(a.extraFuelLb - b.extraFuelLb) + ")";
            } else if (isCrewLine(id) && a.blockS != b.blockS) {
                c.why += ": planned block " + hoursMinutes(a.blockS) + " (was " + hoursMinutes(b.blockS) + ")";
            }
        }
        out.push_back(std::move(c));
    }
    return out;
}

std::vector<WeightBar> weightBars(const PlannedFlightView& f) {
    WeightBar zfw{"Zero fuel weight", std::nullopt, std::nullopt};
    WeightBar tow{"Takeoff weight", std::nullopt, f.mtowLb};
    WeightBar lw{"Landing weight", std::nullopt, f.limits.mlwLb > 0 ? std::optional<int>(f.limits.mlwLb) : std::nullopt};
    if (f.emptyWeightLb) {
        int people = 0;
        for (const CabinLoad& c : f.cabins) people += c.passengers;
        zfw.valueLb = *f.emptyWeightLb + people * f.limits.passengerWeightLb + bagsLb(f.cabins, f.limits) + f.cargoLb;
        zfw.limitLb = *f.emptyWeightLb + f.limits.maxPayloadLb;
    }
    if (f.plan) {
        if (f.brief && f.brief->zfwLb > 0) zfw.valueLb = f.brief->zfwLb;
        if (zfw.valueLb) tow.valueLb = *zfw.valueLb + f.plan->rampFuelLb - (f.brief ? f.brief->taxiLb : 0);
        lw.valueLb = f.plan->estLandingWeightLb;
    }
    return {zfw, tow, lw};
}

}  // namespace blocktime
