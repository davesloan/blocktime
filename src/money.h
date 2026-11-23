// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Money: integer cents, every posted line rounded to the nearest $5, and the two display forms (D-080).

#pragma once

#include <cstdint>
#include <string>

namespace blocktime {

using Cents = std::int64_t;

// To the nearest multiple of 500 cents ($5). Halves round away from zero: $2.50 → $5, -$2.50 → -$5.
Cents roundToFiveDollars(Cents cents);

// Whole dollars with thousands commas, for the ledger and summary lines: "$1,400,000", "-$900,000".
std::string formatMoneyFull(Cents cents);

// The short form for tiles and the cash figure: "$1.4M", "$12.48M", "$382.5k", "$950".
std::string formatMoneyShort(Cents cents);

}  // namespace blocktime
