// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Where the windowed app's last-resort errors go (D-067). A Windows GUI program has no console, so std::cerr goes
// nowhere: LOST: events (D-049) and the errors EventLog and SimConnection print would vanish. ErrorLog points a
// stream (std::cerr in the app) at an append-only file while it lives.

#pragma once

#include <filesystem>
#include <fstream>
#include <ostream>
#include <streambuf>
#include <string_view>
#include <vector>

namespace blocktime {

class ErrorLog {
public:
    // Opens the first of `candidates` that can be opened for appending, writes `startLine`, and sends `stream` to it
    // until destroyed, flushing every write. If none opens, `stream` is left alone.
    ErrorLog(const std::vector<std::filesystem::path>& candidates, std::ostream& stream, std::string_view startLine);
    ~ErrorLog();
    ErrorLog(const ErrorLog&) = delete;
    ErrorLog& operator=(const ErrorLog&) = delete;

    // The file in use, or empty if none could be opened.
    const std::filesystem::path& path() const { return path_; }

private:
    std::ofstream file_;
    std::ostream& stream_;
    std::streambuf* previous_ = nullptr;
    std::ios_base::fmtflags previousFlags_{};
    std::filesystem::path path_;
};

}  // namespace blocktime
