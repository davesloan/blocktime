// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "event_log.h"

#include "data_dir.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <iostream>
#include <sstream>

namespace blocktime {

std::string formatUtc(std::chrono::system_clock::time_point t) {
    return std::format("{:%FT%T}Z", std::chrono::floor<std::chrono::milliseconds>(t));
}

std::string utcTimestamp() { return formatUtc(std::chrono::system_clock::now()); }

std::optional<std::chrono::system_clock::time_point> parseUtc(std::string_view iso) {
    if (iso.size() < 20 || iso.back() != 'Z') return std::nullopt;
    std::istringstream in{std::string(iso)};
    std::chrono::sys_time<std::chrono::milliseconds> t;
    in >> std::chrono::parse("%FT%TZ", t);
    if (in.fail()) return std::nullopt;
    return t;
}

namespace {

// events-YYYY-MM-DD.jsonl, as EventLog names its daily files (D-020). Wide strings: path::string() throws for a name
// the ANSI code page can't hold, and anyone can drop a file into the log folder.
bool isDailyLog(const std::wstring& name) {
    constexpr std::wstring_view prefix = L"events-";
    constexpr std::wstring_view suffix = L".jsonl";
    if (name.size() != prefix.size() + 10 + suffix.size()) return false;
    if (!name.starts_with(prefix) || !name.ends_with(suffix)) return false;
    const std::wstring_view date = std::wstring_view(name).substr(prefix.size(), 10);
    for (std::size_t i = 0; i < date.size(); ++i) {
        const bool dash = i == 4 || i == 7;
        if (dash ? date[i] != L'-' : (date[i] < L'0' || date[i] > L'9')) return false;
    }
    return true;
}

// For error messages: UTF-8 bytes, which never throw (unlike path::string()).
std::string printable(const std::filesystem::path& p) {
    const std::u8string u8 = p.u8string();
    return {u8.begin(), u8.end()};
}

}  // namespace

std::vector<std::filesystem::path> dailyLogFiles(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    std::error_code ec;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && isDailyLog(it->path().filename().wstring())) files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    return files;
}

std::filesystem::path EventLog::defaultDir() { return dataDir() / L"events"; }

EventLog::EventLog(std::filesystem::path dir) : dir_(std::move(dir)) {}

bool EventLog::ensureOpenFor(const std::string& utcDate) {
    if (out_.is_open() && utcDate == openDate_) return true;
    out_.close();
    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    path_ = dir_ / std::format("events-{}.jsonl", utcDate);
    // MSVC's ofstream opens with sharing allowed, so the Flights screen (D-060) can read while we append.
    out_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
    if (!out_) {
        std::cerr << "ERROR: cannot open event log " << printable(path_) << "\n";
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
        std::cerr << "ERROR: failed writing event log " << printable(path_) << "\n";
        out_.close();
        openDate_.clear();
        return false;
    }
    return true;
}

}  // namespace blocktime
