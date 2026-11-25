// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The touchdown (D-125), from samples taken every sim frame while the recorder has fast sampling on (below 300 ft on
// the way down). The rate is the vertical speed of the last airborne frame, the g the peak in the second after the
// first contact. A bounce isn't a second touchdown: it's counted for the landing rating (D-158), in the 15 s after the
// first contact.

#pragma once

#include <chrono>
#include <optional>

namespace blocktime {

using TimePoint = std::chrono::system_clock::time_point;

struct FastSample {
    TimePoint time{};
    bool onGround = false;
    double vsFpm = 0, aglFt = 0, gForce = 0, iasKt = 0, lat = 0, lon = 0;
    // PLANE PITCH DEGREES (raw: the SDK documents nose up as negative; checked in the sim), PLANE BANK DEGREES,
    // PLANE HEADING DEGREES TRUE, AMBIENT WIND DIRECTION (true) and AMBIENT WIND VELOCITY (knots).
    double pitchDeg = 0, bankDeg = 0, headingTrueDeg = 0, windDirDeg = 0, windKt = 0;
};

struct Touchdown {
    TimePoint time{};  // the first frame on the ground
    double vsFpm = 0;  // the last airborne frame's
    double gForce = 0;  // the peak from contact to contact + 1 s
    double iasKt = 0;   // the last airborne frame's
    double lat = 0, lon = 0;  // the first frame on the ground
    double pitchDeg = 0, bankDeg = 0, headingTrueDeg = 0, windDirDeg = 0, windKt = 0;  // the first frame on the ground
};

inline constexpr std::chrono::seconds kTouchdownGWindow{1};
inline constexpr std::chrono::seconds kBounceWindow{15};
inline constexpr std::chrono::milliseconds kBounceMinAirborne{500};

class TouchdownCapture {
public:
    // The touchdown, once, with the first frame at or after contact + 1 s.
    std::optional<Touchdown> onSample(const FastSample& s);
    // Fed every frame after onSample: once, at the first frame at or after contact + 15 s, how many times the wheels
    // left the ground again for at least 0.5 s.
    std::optional<int> bouncesDue(const FastSample& s);
    // The count so far, once, when the bounce window is cut short (taxi in); nullopt before contact or once reported.
    std::optional<int> finishBounces();
    void reset();
    bool captured() const { return captured_; }

private:
    std::optional<FastSample> lastAirborne_;
    std::optional<Touchdown> contact_;
    bool captured_ = false;
    std::optional<TimePoint> contactTime_, airborneSince_;
    int bounces_ = 0;
    bool bouncesReported_ = false;
};

}  // namespace blocktime
