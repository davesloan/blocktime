// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_event_queue.h"

namespace blocktime {

void AppEventQueue::push(std::string type, nlohmann::json data) {
    std::lock_guard lock(mutex_);
    events_.push_back({std::move(type), std::move(data)});
}

std::vector<LogEvent> AppEventQueue::drain() {
    std::lock_guard lock(mutex_);
    std::vector<LogEvent> out;
    out.swap(events_);
    return out;
}

}  // namespace blocktime
