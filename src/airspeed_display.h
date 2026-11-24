// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Live's airspeed tile switches to Mach at M0.70 and back to knots below M0.66 (D-140).

#pragma once

#include <cmath>
#include <string>

namespace blocktime {

inline constexpr double kMachOn = 0.70;
inline constexpr double kMachOff = 0.66;

inline bool showMach(bool wasMach, double mach) {
    if (mach >= kMachOn) return true;
    if (mach < kMachOff) return false;
    return wasMach;
}

// "M.78"; "M1.02" at or above Mach 1.
inline std::string machText(double mach) {
    const long hundredths = std::lround(mach * 100.0);
    const std::string digits = (hundredths % 100 < 10 ? "0" : "") + std::to_string(hundredths % 100);
    return hundredths >= 100 ? "M" + std::to_string(hundredths / 100) + "." + digits : "M." + digits;
}

}  // namespace blocktime
