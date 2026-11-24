// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gsx_watcher.h"

#include <cmath>

#include "event_log.h"

namespace blocktime {

namespace {

constexpr const char* kServices[] = {"catering", "refuelling", "boarding", "deboarding", "pushback"};
constexpr const char* kStates[] = {"", "available", "not_available", "bypassed", "requested", "performing",
                                   "completed"};

}  // namespace

std::vector<LogEvent> GsxWatcher::onValues(const GsxValues& v, std::chrono::system_clock::time_point now) {
    const double states[] = {v.catering, v.refuelling, v.boarding, v.deboarding, v.pushback};
    std::vector<LogEvent> out;
    for (std::size_t i = 0; i < last_.size(); ++i) {
        const int s = static_cast<int>(std::lround(states[i]));
        if (s < 1 || s > 6 || s == last_[i]) continue;
        const bool first = last_[i] == 0;
        last_[i] = s;
        if (first && s == 1) continue;
        nlohmann::json d{{"service", kServices[i]}, {"state", kStates[s]}, {"time", formatUtc(now)}};
        if (i == 2) d["passengers"] = static_cast<int>(std::lround(v.boardedPax));
        if (i == 3) d["passengers"] = static_cast<int>(std::lround(v.deboardedPax));
        out.push_back({"gsx_service", std::move(d)});
    }
    return out;
}

}  // namespace blocktime
