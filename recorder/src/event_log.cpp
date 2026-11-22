// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "event_log.h"

#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>

namespace blocktime {

std::string formatUtc(std::chrono::system_clock::time_point t) {
    return std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::milliseconds>(t));
}

std::string utcTimestamp() { return formatUtc(std::chrono::system_clock::now()); }

std::filesystem::path EventLog::defaultDir() {
    wchar_t* base = nullptr;
    std::size_t len = 0;
    std::filesystem::path root;
    if (_wdupenv_s(&base, &len, L"LOCALAPPDATA") == 0 && base != nullptr) {
        root = base;
        std::free(base);
    } else {
        root = std::filesystem::current_path();
    }
    return root / L"BlockTime" / L"events";
}

EventLog::EventLog(std::filesystem::path dir) : dir_(std::move(dir)) {}

bool EventLog::ensureOpenFor(const std::string& utcDate) {
    if (out_.is_open() && utcDate == openDate_) return true;
    out_.close();
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    path_ = dir_ / std::format("events-{}.jsonl", utcDate);
    // MSVC's ofstream opens with sharing allowed, so other programs (the web app) can read while we append.
    out_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
    if (!out_) {
        std::cerr << "ERROR: cannot open event log " << path_.string() << "\n";
        openDate_.clear();
        return false;
    }
    // If a crash left a torn last line, start on a fresh line so our first event isn't glued to it (D-036).
    std::error_code sizeEc;
    if (const auto size = std::filesystem::file_size(path_, sizeEc); !sizeEc && size > 0) {
        std::ifstream in(path_, std::ios::binary);
        char last = '\n';
        if (in.seekg(-1, std::ios::end) && in.get(last) && last != '\n') out_ << '\n';
    }
    openDate_ = utcDate;
    return true;
}

bool EventLog::write(std::string_view type, const nlohmann::json& data) {
    const std::string ts = utcTimestamp();
    if (!ensureOpenFor(ts.substr(0, 10))) return false;

    nlohmann::ordered_json line;
    line["ts"] = ts;
    line["type"] = type;
    line["data"] = data;
    // Sim strings are not guaranteed to be UTF-8; replace bad bytes instead of throwing.
    out_ << line.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace) << '\n';
    out_.flush();
    if (!out_) {
        std::cerr << "ERROR: failed writing event log " << path_.string() << "\n";
        out_.close();
        openDate_.clear();
        return false;
    }
    return true;
}

}  // namespace blocktime
