// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Append-only JSON Lines event log (D-008, D-020). One file per UTC day:
//   <dir>/events-YYYY-MM-DD.jsonl
// Each line: {"ts":"2026-11-21T14:03:07.123Z","type":"...","data":{...}}
// Every line is flushed before write() returns, and is written before anything else sees the event.

#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace blocktime {

class EventLog {
public:
    explicit EventLog(std::filesystem::path dir);

    // Appends one event. Returns false if the line could not be written (also reported on stderr).
    bool write(std::string_view type, const nlohmann::json& data = nlohmann::json::object());

    const std::filesystem::path& dir() const { return dir_; }

    // Default directory: %LOCALAPPDATA%\BlockTime\events
    static std::filesystem::path defaultDir();

private:
    bool ensureOpenFor(const std::string& utcDate);

    std::filesystem::path dir_;
    std::filesystem::path path_;
    std::string openDate_;
    std::ofstream out_;
};

// UTC timestamp, ISO-8601 with milliseconds, e.g. 2026-11-21T14:03:07.123Z
std::string utcTimestamp();

}  // namespace blocktime
