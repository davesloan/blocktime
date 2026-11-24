// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// What's worth logging from each SayIntentions poll (D-123, D-127). getCommsHistory serves the whole current flight
// each time, and a reply can be written a few seconds late, so an exchange is logged when it's new and again when its
// content changed; a reader keeps the last line per id. An ATIS is logged when its letter or text changes.

#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sayintentions.h"

namespace blocktime {

class CommsDedupe {
public:
    // The entries to log. A different SayIntentions flight id starts over.
    std::vector<nlohmann::json> toLog(const CommsHistory& history);
    void reset();

private:
    std::string flightId_;
    std::map<std::string, std::string> logged_;  // key → the entry as last logged
};

class AtisDedupe {
public:
    bool isNew(const Atis& atis);  // per ICAO
    void reset();

private:
    std::map<std::string, std::string> last_;
};

}  // namespace blocktime
