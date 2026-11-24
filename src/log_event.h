// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// One event for the log: its type and its data object. The recorder's pure parts return these, and the recorder
// writes them.

#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace blocktime {

struct LogEvent {
    std::string type;
    nlohmann::json data;
};

}  // namespace blocktime
