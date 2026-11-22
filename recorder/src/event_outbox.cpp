// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "event_outbox.h"

namespace blocktime {

EventOutbox::EventOutbox(WriteFn write) : write_(std::move(write)) {}

bool EventOutbox::send(std::string type, nlohmann::json data) {
    held_.emplace_back(std::move(type), std::move(data));
    return flush();
}

bool EventOutbox::flush() {
    while (!held_.empty()) {
        const auto& [type, data] = held_.front();
        if (!write_(type, data)) return false;
        held_.pop_front();
    }
    return true;
}

std::vector<std::pair<std::string, nlohmann::json>> EventOutbox::held() const {
    return {held_.begin(), held_.end()};
}

}  // namespace blocktime
