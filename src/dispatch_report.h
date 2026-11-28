// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The dispatch report's numbers (docs/dispatch.md, D-234): the split-flap board's rows and their 20-tile text, the
// ring's slices, each line's working, and what a re-import changed. QML only draws these.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "money.h"
#include "pricing.h"

namespace blocktime {

struct BoardRow {
    enum class Tone { Income, Cost, Profit, Loss };
    std::string id;     // the line's category, or "profit"
    std::string label;  // short, for the tiles (boardLabel)
    std::string value;  // "+$15,630", "-$2,420", "$8,760"
    Tone tone = Tone::Cost;
};

// The income lines, the cost lines, then the profit. A preview with no income (a delivery) has no fares row.
std::vector<BoardRow> boardRows(const Preview& money);

// The label that fits 20 tiles: "Cabin crew" for flight_attendants, "Cargo hdlg", "ATC / nav", "Landing", "Gate",
// "Handling", "GA services"; the ledger label for the rest. `ledgerLabel` is the line's own label.
std::string boardLabel(std::string_view category, std::string_view ledgerLabel);

// Exactly `tiles` characters: the label in capitals on the left, cut to leave one space before the value, and the
// value right-aligned.
std::string boardText(const BoardRow& row, int tiles = 20);

// A slice of the ring: start and sweep are fractions of the circle (0 at the top, clockwise).
struct RingSlice {
    std::string id, family;  // family: "fuel", "airport", "crew", "service" or "profit"
    Cents cents = 0;         // its amount, positive
    double start = 0, sweep = 0;
};

struct Ring {
    bool loss = false;
    std::vector<RingSlice> slices;
};

// With a profit, the circle is the income: each cost in line order, then a profit slice for what's left. With a loss
// (or no income), the circle is the costs and there's no profit slice. $0 lines get no slice.
Ring ring(const Preview& money);

std::string ringFamily(std::string_view category);

struct WorkingFacts {
    int rampFuelLb = 0, fuelOnRecordLb = 0, extraFuelLb = 0;
    bool locked = false;  // after block out
};

// How a line was worked out, one line of text each. The fuel line shows the block fuel less the fuel on board, the
// pounds added × the price, and that the price is locked; the others give workingText. Pilots, flight attendants and
// maintenance add "Charged on the actual block time" until locked.
std::vector<std::string> workingLines(const MoneyLine& line, const WorkingFacts& facts);

struct ChangeFacts {
    int blockS = 0, fuelAddedLb = 0, extraFuelLb = 0;
};

struct LineChange {
    std::string id;
    Cents was = 0, now = 0;
    std::string why;
};

// One entry per board id whose amount differs between two imports, the profit included.
std::vector<LineChange> changesSince(const Preview& before, const ChangeFacts& b, const Preview& after,
                                     const ChangeFacts& a);

// "+$15,630" for income, "-$2,420" for a cost, "$8,760" for a profit, "-$1,240" for a loss.
std::string boardValue(Cents cents, BoardRow::Tone tone);

}  // namespace blocktime
