// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "money.h"

#include <format>

namespace blocktime {

namespace {

// `value` / 10^places, with trailing zeros (and a bare point) dropped: (140, 2) → "1.4", (500, 2) → "5".
std::string trimmedDecimal(std::int64_t value, int places) {
    std::int64_t scale = 1;
    for (int i = 0; i < places; ++i) scale *= 10;
    std::string out = std::to_string(value / scale);
    std::int64_t frac = value % scale;
    int digits = places;
    while (digits > 0 && frac % 10 == 0) {
        frac /= 10;
        --digits;
    }
    if (digits > 0) out += std::format(".{:0{}}", frac, digits);
    return out;
}

}  // namespace

Cents roundToFiveDollars(Cents cents) {
    const Cents magnitude = cents < 0 ? -cents : cents;
    const Cents rounded = (magnitude + 250) / 500 * 500;
    return cents < 0 ? -rounded : rounded;
}

std::string formatMoneyFull(Cents cents) {
    const std::int64_t dollars = (cents < 0 ? -cents : cents) / 100;
    std::string digits = std::to_string(dollars);
    for (auto i = static_cast<std::ptrdiff_t>(digits.size()) - 3; i > 0; i -= 3) digits.insert(static_cast<std::size_t>(i), ",");
    return (cents < 0 ? "-$" : "$") + digits;
}

std::string formatMoneyShort(Cents cents) {
    const std::int64_t dollars = (cents < 0 ? -cents : cents) / 100;
    const std::string sign = cents < 0 ? "-$" : "$";
    const auto millions = [&] { return sign + trimmedDecimal((dollars + 5'000) / 10'000, 2) + "M"; };
    if (dollars >= 1'000'000) return millions();
    if (dollars >= 1'000) {
        const std::int64_t tenthsOfK = (dollars + 50) / 100;
        if (tenthsOfK >= 10'000) return millions();  // 999,950 would read "1000k"
        return sign + trimmedDecimal(tenthsOfK, 1) + "k";
    }
    return sign + std::to_string(dollars);
}

}  // namespace blocktime
