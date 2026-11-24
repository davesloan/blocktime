// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Events the app (GUI thread) wants in the event log: the SayIntentions ATIS and ATC exchanges (D-123). The recorder
// thread drains the queue into the log on every loop, so the log keeps a single writer (D-127).

#pragma once

#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "flight_tracker.h"  // LogEvent

namespace blocktime {

class AppEventQueue {
public:
    void push(std::string type, nlohmann::json data);
    std::vector<LogEvent> drain();

private:
    std::mutex mutex_;
    std::vector<LogEvent> events_;
};

}  // namespace blocktime
