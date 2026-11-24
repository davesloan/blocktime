// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The touchdown (D-125), from samples taken every sim frame while the recorder has fast sampling on (below 300 ft on
// the way down). The rate is the vertical speed of the last airborne frame, the g the peak in the second after the
// first contact. A bounce isn't a second touchdown.

#pragma once

#include <chrono>
#include <optional>

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

struct FastSample {
    TimePoint time{};
    bool onGround = false;
    double vsFpm = 0, aglFt = 0, gForce = 0, iasKt = 0, lat = 0, lon = 0;
};

struct Touchdown {
    TimePoint time{};  // the first frame on the ground
    double vsFpm = 0;  // the last airborne frame's
    double gForce = 0;  // the peak from contact to contact + 1 s
    double iasKt = 0;   // the last airborne frame's
    double lat = 0, lon = 0;  // the first frame on the ground
};

inline constexpr std::chrono::seconds kTouchdownGWindow{1};

class TouchdownCapture {
public:
    // The touchdown, once, with the first frame at or after contact + 1 s.
    std::optional<Touchdown> onSample(const FastSample& s);
    void reset();
    bool captured() const { return captured_; }

private:
    std::optional<FastSample> lastAirborne_;
    std::optional<Touchdown> contact_;
    bool captured_ = false;
};

}  // namespace blocktime
