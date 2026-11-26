// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Which face the in-flight panel's altitude gauge shows (D-203): the radio altitude below 2,000 ft above the ground,
// the altimeter above 3,000 ft. The 1,000 ft between keep terrain under an approach from flipping it back and forth.
// A go-around shows the altimeter at once, and the radio altitude comes back at the next short final.

#pragma once

namespace blocktime {

inline constexpr double kRadioFaceBelowFt = 2000, kAltimeterFaceAboveFt = 3000;

class AltitudeFace {
public:
    void reset();                  // block out: the radio face (on the ground), armed
    void onSample(double aglFt);   // armed and below 2,000 → radio; above 3,000 → altimeter, and armed again
    void onGoAround();             // the altimeter at once, disarmed
    void onFinalApproach();        // the radio face, armed
    bool radio() const { return radio_; }

private:
    bool radio_ = true, armed_ = true;
};

}  // namespace blocktime
