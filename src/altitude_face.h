// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Which face the in-flight panel's altitude gauge shows (D-203, D-209): the radio altitude below 300 ft above the
// ground, the altimeter above 500 ft (the radio dial's top). The 200 ft between keep terrain under a short final from
// flipping it back and forth. A go-around shows the altimeter at once; the radio altitude comes back below 300 ft
// once the airplane has climbed above 500 ft or reached the next final approach marker.

#pragma once

namespace blocktime {

inline constexpr double kRadioFaceBelowFt = 300, kAltimeterFaceAboveFt = 500;

class AltitudeFace {
public:
    void reset();                  // block out: the radio face (on the ground), armed
    void onSample(double aglFt);   // armed and below 300 → radio; above 500 → altimeter, and armed again
    void onGoAround();             // the altimeter at once, disarmed
    void onFinalApproach();        // armed (at 1,000 ft the radio face would sit pegged: the 300 ft rule shows it)
    bool radio() const { return radio_; }

private:
    bool radio_ = true, armed_ = true;
};

}  // namespace blocktime
