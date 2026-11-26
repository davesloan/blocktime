// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The ATC card (D-197, docs/dashboard.md "The ATC card"): the gist of what ATC cleared, one slot per kind of
// instruction. Each slot changes only when ATC changes it; LATEST marks the slots the last transmission that changed
// any slot changed, until the next one does (D-201). Rebuilt in ATC's time order on every message, so a repeat or a
// late re-log never moves a slot back.

#pragma once

#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "atc_instructions.h"
#include "atc_message.h"

namespace blocktime {

enum class CardStage { Departure, Airborne, Arrival };  // block out to the takeoff roll; to touchdown; after it

struct AtcSlot {
    std::string title;  // "TAXI", "ALTITUDE", "ROUTE" / "HEADING", "SPEED", "APPROACH", "HOLD SHORT"
    std::string value;  // "" = nothing given yet (the readout shows a dash)
    std::string aside;  // the qualifier; "" = show `time` as a local time
    std::string time;   // ATC's stamp_zulu of the call that set it; "" when empty
    bool latest = false;
    bool operator==(const AtcSlot&) const = default;
};

class AtcCard {
public:
    // Messages before plannedAt don't count (the previous leg's). A different plannedAt starts over.
    void setFlight(std::string plannedAt, RoutePlan plan);
    void onMessage(const AtcMessage& m);   // any airport or center; a repeat of an id replaces it
    void onGoAround(std::string time);     // clears APPROACH (D-201)
    void onTouchdown(std::string time);    // clears TAXI and HOLD SHORT: the taxi in is a new one
    // Departure: ALTITUDE, ROUTE, TAXI. Airborne: ALTITUDE, ROUTE (or HEADING), SPEED, APPROACH. Arrival: TAXI,
    // HOLD SHORT.
    std::vector<AtcSlot> slots(CardStage stage) const;

private:
    void rebuild();

    std::string plannedAt_;
    std::optional<std::chrono::system_clock::time_point> plannedTime_;
    RoutePlan plan_;
    std::map<long long, AtcMessage> messages_;  // by SayIntentions' id
    std::vector<std::string> goArounds_, touchdowns_;
    std::array<AtcSlot, 6> slots_{};  // by Slot
};

}  // namespace blocktime
