// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// GSX Pro's service states as log events (recorder.md "GSX Pro", D-073, D-138). The recorder reads the L:vars once a
// second and hands them here; each change of a service's state becomes a gsx_service event. A 0 means GSX isn't
// running and is ignored, and so is a service's first state when it's "available" (every service sits there once a
// flight is loaded). Pure logic, so it can be tested without the sim.

#pragma once

#include <array>
#include <chrono>
#include <vector>

#include "log_event.h"

namespace blocktime {

// The L:vars in the order SimConnect writes them (kGsxServiceVars in sim_connection.cpp), all "number".
#pragma pack(push, 1)
struct GsxValues {
    double catering = 0, refuelling = 0, boarding = 0, deboarding = 0, pushback = 0;  // states 1..6, 0 if not running
    double boardedPax = 0, deboardedPax = 0;  // NUMPASSENGERS_BOARDING_TOTAL / _DEBOARDING_TOTAL
};
#pragma pack(pop)

class GsxWatcher {
public:
    std::vector<LogEvent> onValues(const GsxValues& v, std::chrono::system_clock::time_point now);
    void reset() { last_ = {}; }  // a new sim session: forget the last states

private:
    std::array<int, 5> last_{};  // 0 = nothing seen yet
};

}  // namespace blocktime
