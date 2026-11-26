// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One ATC exchange in a plain form (D-168). `atc_exchange` keeps SayIntentions' entry verbatim; dashboard code reads it
// only through atcMessage(), so another ATC system would only have to fill this form.

#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace blocktime {

struct AtcMessage {
    long long id = -1;
    std::string time;       // ATC's own time, stamp_zulu ("2026-11-25T18:39:45Z"); "" if none
    std::string airport;    // ident: "KSAN"
    std::string position;   // "CLEARANCE", "GROUND", "TOWER", ...; "" if none
    std::string station;    // "Lindbergh Clearance"
    std::string frequency;  // "125.9"
    std::string atc;        // what ATC said (outgoing_message); "" when only the pilot spoke
    std::string pilot;      // what Dave or his co-pilot said (incoming_message)
};

// From an atc_exchange line's data ({"entry": {...}, ...}). nullopt without an entry object, or for a channel that
// isn't COM (cabin crew and PA are on INTERCOM). SayIntentions' nulls read as "".
std::optional<AtcMessage> atcMessage(const nlohmann::json& atcExchangeData);

}  // namespace blocktime
