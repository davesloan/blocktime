// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "touchdown_capture.h"

#include <algorithm>

namespace blocktime {

std::optional<Touchdown> TouchdownCapture::onSample(const FastSample& s) {
    if (captured_) return std::nullopt;
    if (!contact_) {
        if (!s.onGround) {
            lastAirborne_ = s;
        } else if (lastAirborne_) {
            contact_ = Touchdown{s.time, lastAirborne_->vsFpm, s.gForce, lastAirborne_->iasKt, s.lat, s.lon};
        }
        return std::nullopt;
    }
    if (s.time - contact_->time >= kTouchdownGWindow) {
        captured_ = true;
        return contact_;
    }
    contact_->gForce = std::max(contact_->gForce, s.gForce);
    return std::nullopt;
}

void TouchdownCapture::reset() {
    lastAirborne_.reset();
    contact_.reset();
    captured_ = false;
}

}  // namespace blocktime
