// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "error_log.h"

namespace blocktime {

ErrorLog::ErrorLog(const std::vector<std::filesystem::path>& candidates, std::ostream& stream,
                   std::string_view startLine)
    : stream_(stream) {
    for (const auto& candidate : candidates) {
        file_.open(candidate, std::ios::binary | std::ios::app);
        if (file_.is_open()) {
            path_ = candidate;
            break;
        }
        file_.clear();
    }
    if (!file_.is_open()) return;

    file_ << startLine << '\n' << std::flush;
    previous_ = stream_.rdbuf(file_.rdbuf());
    // Flush after every write, so a crash right after an error doesn't lose it.
    previousFlags_ = stream_.setf(std::ios_base::unitbuf);
}

ErrorLog::~ErrorLog() {
    if (previous_ == nullptr) return;
    stream_.flush();
    stream_.rdbuf(previous_);
    stream_.flags(previousFlags_);
}

}  // namespace blocktime
