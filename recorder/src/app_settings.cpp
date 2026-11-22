// BlockTime Recorder
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app_settings.h"

#include <fstream>
#include <iterator>
#include <string_view>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "event_log.h"

namespace blocktime {

AppSettings loadAppSettings(const std::filesystem::path& file) {
    AppSettings settings;
    std::ifstream in(file, std::ios::binary);
    if (!in) return settings;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    constexpr std::string_view bom = "\xEF\xBB\xBF";
    if (content.starts_with(bom)) content.erase(0, bom.size());

    const auto j = nlohmann::json::parse(content, nullptr, false);
    if (j.is_discarded() || !j.is_object()) return settings;
    const auto key = j.find("carto_key");
    if (key != j.end() && key->is_string()) settings.cartoKey = key->get<std::string>();
    return settings;
}

// Next to the event log folder: %LOCALAPPDATA%\BlockTime\events → %LOCALAPPDATA%\BlockTime.
std::filesystem::path defaultSettingsFile() { return EventLog::defaultDir().parent_path() / "settings.json"; }
std::filesystem::path defaultTileCacheDir() { return EventLog::defaultDir().parent_path() / "tiles"; }

std::size_t pruneOldFiles(const std::filesystem::path& dir, std::chrono::hours maxAge) {
    const auto cutoff = std::filesystem::file_time_type::clock::now() - maxAge;
    std::vector<std::filesystem::path> old;
    std::error_code ec;
    for (std::filesystem::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        std::error_code fileEc;
        if (!it->is_regular_file(fileEc)) continue;
        const auto written = it->last_write_time(fileEc);
        if (!fileEc && written < cutoff) old.push_back(it->path());
    }
    std::size_t removed = 0;
    for (const auto& path : old) {
        std::error_code rmEc;
        if (std::filesystem::remove(path, rmEc)) ++removed;
    }
    return removed;
}

}  // namespace blocktime
