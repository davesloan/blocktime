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
            contact_ = Touchdown{s.time,     lastAirborne_->vsFpm, s.gForce,         lastAirborne_->iasKt,
                                 s.lat,      s.lon,                s.pitchDeg,       s.bankDeg,
                                 s.headingTrueDeg, s.windDirDeg,   s.windKt};
            contactTime_ = s.time;
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

std::optional<int> TouchdownCapture::bouncesDue(const FastSample& s) {
    if (!contactTime_ || bouncesReported_) return std::nullopt;
    if (!s.onGround) {
        if (!airborneSince_) airborneSince_ = s.time;
    } else if (airborneSince_) {
        if (s.time - *airborneSince_ >= kBounceMinAirborne) ++bounces_;
        airborneSince_.reset();
    }
    if (s.time - *contactTime_ < kBounceWindow) return std::nullopt;
    return finishBounces();
}

std::optional<int> TouchdownCapture::finishBounces() {
    if (!contactTime_ || bouncesReported_) return std::nullopt;
    bouncesReported_ = true;
    return bounces_;
}

void TouchdownCapture::reset() {
    lastAirborne_.reset();
    contact_.reset();
    captured_ = false;
    contactTime_.reset();
    airborneSince_.reset();
    bounces_ = 0;
    bouncesReported_ = false;
}

}  // namespace blocktime
