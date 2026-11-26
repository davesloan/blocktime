// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One ATC transmission to the ATC card's slot changes (D-197, docs/dashboard.md "The ATC card"). Each sentence is
// read with its own pattern; a sentence that matches none changes nothing, so a traffic call's "4,000" or an
// "Altimeter 30.19" never becomes an altitude (a guessed clearance is an anti-goal).

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace blocktime {

enum class Slot { Taxi, HoldShort, Altitude, Route, Speed, Approach };

struct SlotChange {
    Slot slot = Slot::Altitude;
    std::string title;  // "ROUTE" or "HEADING" for Slot::Route; the slot's name otherwise ("HOLD SHORT")
    std::string value;  // "" clears the slot ("resume normal speed")
    std::string aside;  // a qualifier, or "" (the card shows the time instead)
    bool operator==(const SlotChange&) const = default;
};

struct RoutePlan {
    std::string sid, sidTransition;  // the plan's, for the clearance's ROUTE
};

// Everything one transmission changed, in the order said. Empty when nothing is read.
std::vector<SlotChange> readInstructions(std::string_view atcWords, const RoutePlan& plan);

}  // namespace blocktime
