// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Append-only JSON Lines event log (D-008, D-020). One file per UTC day:
//   <dir>/events-YYYY-MM-DD.jsonl
// Each line: {"ts":"2026-11-21T14:03:07.123Z","type":"...","data":{...}}
// Every line is flushed before write() returns, and is written before anything else sees the event.

#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace blocktime {

class EventLog {
public:
    explicit EventLog(std::filesystem::path dir);

    // Appends one event. Returns false if the line could not be written (also reported on stderr).
    bool write(std::string_view type, const nlohmann::json& data = nlohmann::json::object());

    const std::filesystem::path& dir() const { return dir_; }

    // Default directory: dataDir()\events, %LOCALAPPDATA%\BlockTime\events unless the app chose another (D-226)
    static std::filesystem::path defaultDir();

private:
    bool ensureOpenFor(const std::string& utcDate);

    std::filesystem::path dir_;
    std::filesystem::path path_;
    std::string openDate_;
    std::ofstream out_;
};

// UTC timestamp, ISO-8601 with milliseconds (truncated), e.g. 2026-11-21T14:03:07.123Z
std::string formatUtc(std::chrono::system_clock::time_point t);
// formatUtc's inverse: "2026-11-24T23:53:16.837Z", milliseconds optional. nullopt for anything else.
std::optional<std::chrono::system_clock::time_point> parseUtc(std::string_view iso);

// formatUtc() of the current time.
std::string utcTimestamp();

// The events-YYYY-MM-DD.jsonl files in `dir`, oldest first (the date in the name sorts as text). Empty if `dir`
// is missing or unreadable.
std::vector<std::filesystem::path> dailyLogFiles(const std::filesystem::path& dir);

}  // namespace blocktime
