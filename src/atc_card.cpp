// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "atc_card.h"

#include <algorithm>
#include <regex>
#include <tuple>

#include "departure_conditions.h"
#include "event_log.h"

namespace blocktime {

namespace {

using Time = std::chrono::system_clock::time_point;

constexpr std::array<const char*, 6> kTitles = {"TAXI", "HOLD SHORT", "ALTITUDE", "ROUTE", "SPEED", "APPROACH"};

int index(Slot s) { return static_cast<int>(s); }

}  // namespace

void AtcCard::setFlight(std::string plannedAt, RoutePlan plan) {
    if (plannedAt == plannedAt_ && plan.sid == plan_.sid && plan.sidTransition == plan_.sidTransition) return;
    plannedAt_ = std::move(plannedAt);
    plannedTime_ = parseUtc(plannedAt_);
    plan_ = std::move(plan);
    messages_.clear();
    goArounds_.clear();
    touchdowns_.clear();
    rebuild();
}

void AtcCard::onMessage(const AtcMessage& m) {
    if (m.time.empty()) return;  // no ATC time to order it by
    messages_[m.id] = Read{m, parseUtc(m.time), readInstructions(m.atc, plan_)};
    rebuild();
}

void AtcCard::onGoAround(std::string time) {
    goArounds_.push_back(std::move(time));
    rebuild();
}

void AtcCard::onTouchdown(std::string time) {
    touchdowns_.push_back(std::move(time));
    rebuild();
}

void AtcCard::rebuild() {
    for (int i = 0; i < 6; ++i) slots_[i] = AtcSlot{kTitles[i], "", "", "", false};
    star_.clear();
    approachRunway_.clear();
    landingWind_.clear();

    // Everything in ATC's time order: messages, then go-arounds and touchdowns at their times.
    enum class Kind { Message, GoAround, Touchdown };
    struct Step {
        Time at;
        Kind kind;
        long long id;
        const Read* read;
    };
    std::vector<Step> steps;
    for (const auto& [id, r] : messages_) {
        if (!r.at || (plannedTime_ && *r.at < *plannedTime_)) continue;  // the previous leg's
        steps.push_back({*r.at, Kind::Message, id, &r});
    }
    for (const std::string& g : goArounds_)
        if (const auto t = parseUtc(g)) steps.push_back({*t, Kind::GoAround, 0, nullptr});
    for (const std::string& d : touchdowns_)
        if (const auto t = parseUtc(d)) steps.push_back({*t, Kind::Touchdown, 0, nullptr});
    std::stable_sort(steps.begin(), steps.end(), [](const Step& a, const Step& b) {
        return std::tie(a.at, a.id) < std::tie(b.at, b.id);
    });

    for (const Step& step : steps) {
        if (step.kind == Kind::GoAround) {
            slots_[index(Slot::Approach)] = AtcSlot{kTitles[index(Slot::Approach)], "", "", "", false};
            continue;
        }
        if (step.kind == Kind::Touchdown) {
            for (Slot s : {Slot::Taxi, Slot::HoldShort}) slots_[index(s)] = AtcSlot{kTitles[index(s)], "", "", "", false};
            continue;
        }
        const AtcMessage& message = step.read->message;
        const std::vector<SlotChange>& changes = step.read->changes;
        if (changes.empty()) continue;  // LATEST stays on the last call that changed something
        for (AtcSlot& s : slots_) s.latest = false;
        const bool descendVia = std::any_of(changes.begin(), changes.end(), [](const SlotChange& c) {
            return c.slot == Slot::Altitude && c.value.starts_with("DESCEND VIA ");
        });
        for (const SlotChange& c : changes) {
            if (descendVia && c.slot == Slot::Route) star_ = c.value;
            if (c.slot == Slot::Approach && !c.value.empty()) {
                static const std::regex runway(R"(\b(\d{2}[LRC]?)\b)");
                std::smatch m;
                if (std::regex_search(c.value, m, runway)) approachRunway_ = m[1].str();
                if (c.value.starts_with("CLEARED TO LAND")) {
                    const auto wind = atisWind(message.atc);
                    landingWind_ = wind ? windText(*wind) : "";
                }
            }
        }
        for (const SlotChange& c : changes) {
            AtcSlot& s = slots_[index(c.slot)];
            // A clearance keeps the expected transition only for the approach that was expected (0.15.1).
            const bool sameApproach = c.slot != Slot::Approach ||
                                      (c.value.starts_with("CLEARED ") && s.value == "EXP " + c.value.substr(8));
            if (!c.keepAside || !sameApproach) s.aside = c.aside;
            s.title = c.title;
            s.value = c.value;
            s.time = c.value.empty() ? "" : message.time;
            s.latest = true;
        }
    }
}

std::vector<AtcSlot> AtcCard::slotsFor(CardStage stage) const {
    const auto pick = [this](std::initializer_list<Slot> list) {
        std::vector<AtcSlot> out;
        for (Slot s : list) out.push_back(slots_[index(s)]);
        return out;
    };
    switch (stage) {
        case CardStage::Departure: return pick({Slot::Altitude, Slot::Route, Slot::Taxi});
        case CardStage::Airborne: return pick({Slot::Altitude, Slot::Route, Slot::Speed, Slot::Approach});
        case CardStage::Arrival: return pick({Slot::Taxi, Slot::HoldShort});
    }
    return {};
}

}  // namespace blocktime
