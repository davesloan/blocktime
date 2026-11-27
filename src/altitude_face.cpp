// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "altitude_face.h"

namespace blocktime {

void AltitudeFace::reset() {
    radio_ = true;
    armed_ = true;
}

void AltitudeFace::onSample(double aglFt) {
    if (aglFt > kAltimeterFaceAboveFt) {
        radio_ = false;
        armed_ = true;
    } else if (armed_ && aglFt < kRadioFaceBelowFt) {
        radio_ = true;
    }
}

void AltitudeFace::onGoAround() {
    radio_ = false;
    armed_ = false;
}

void AltitudeFace::onFinalApproach() { armed_ = true; }

}  // namespace blocktime
