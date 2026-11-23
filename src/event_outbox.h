// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Holds block events (block_out, block_in, flight_interrupted) whose log write failed, and writes them later,
// oldest first, so a full disk or a locked file doesn't lose a flight (D-049). The event's own times are in its
// data (D-043), so a late write only moves the line's "ts".

#pragma once

#include <cstddef>
#include <deque>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

class EventOutbox {
public:
    using WriteFn = std::function<bool(std::string_view type, const nlohmann::json& data)>;

    explicit EventOutbox(WriteFn write);

    // Queues the event behind any held ones, then flush(). Returns true if nothing is held afterwards.
    bool send(std::string type, nlohmann::json data);

    // Writes held events oldest first and stops at the first failure. Returns true if nothing is held afterwards.
    bool flush();

    std::size_t pending() const { return held_.size(); }

    // The held events, oldest first (for the LOST: lines at exit).
    std::vector<std::pair<std::string, nlohmann::json>> held() const;

private:
    WriteFn write_;
    std::deque<std::pair<std::string, nlohmann::json>> held_;
};

}  // namespace blocktime
