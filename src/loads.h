// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loads (design.md §9, D-089): passengers rolled per cabin, belly or freighter cargo, and the split of bags and
// cargo across the holds by their ratios. Rolled once when New Flight opens and shown exactly in the preview.

#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "catalog.h"

namespace blocktime {

// The kind of flight (design.md §13.1). The codes are planned_flight.kind's.
enum class FlightKind { Passenger, Vip, Charter, Cargo, Delivery };

std::string_view kindCode(FlightKind kind);
std::optional<FlightKind> parseKind(std::string_view code);

// A uniform value in [0, 1). The app uses std::mt19937_64; tests pass fixed sequences.
using Roll = std::function<double()>;

struct CabinLoad {
    std::string cabinClass;
    int passengers = 0;
};

struct LoadRoll {
    std::vector<CabinLoad> cabins;
    int cargoLb = 0;  // a passenger flight's rolled belly cargo (flown only if chosen), or a freighter's load
};

// Rolls each cabin in order (one roll each), then the cargo if the kind has any (one roll). Delivery rolls nothing.
LoadRoll rollLoads(const std::vector<Cabin>& cabins, const std::vector<Hold>& holds, const AirplaneLimits& limits,
                   FlightKind kind, bool daytime, const Roll& roll);

// Fully loaded (D-215, design.md §9 step 5): a passenger flight fills every seat, then adds cargo up to the hold space
// left after bags or max payload less passengers and bags, whichever is less. A freighter carries cargo up to the
// holds or max payload. Other kinds are never fully loaded: they get rollLoads' result for no rolls (empty).
LoadRoll fullLoads(const std::vector<Cabin>& cabins, const std::vector<Hold>& holds, const AirplaneLimits& limits,
                   FlightKind kind);

int bagsLb(const std::vector<CabinLoad>& cabins, const AirplaneLimits& limits);  // passengers x bag weight

struct HoldLoad {
    std::string hold;
    int weightLb = 0;
};

// Splits `totalLb` across the holds by ratio, each capped at its max; what doesn't fit spills to holds with room.
// Pounds left over from rounding go to the open hold with the largest ratio. The parts always add up to `totalLb`,
// which must fit in the holds (cargo is capped when it's rolled).
std::vector<HoldLoad> splitHolds(const std::vector<Hold>& holds, int totalLb);

bool isDaytime(int localHour);  // 07:00-18:59 departs in the day band

}  // namespace blocktime
