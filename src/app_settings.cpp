// BlockTime
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
    const auto read = [&](const char* name, std::string& out) {
        const auto value = j.find(name);
        if (value != j.end() && value->is_string()) out = value->get<std::string>();
    };
    read("carto_key", settings.cartoKey);
    read("simbrief_user_id", settings.simbriefUserId);
    read("sayintentions_api_key", settings.sayIntentionsApiKey);
    read("sayintentions_developer_key", settings.sayIntentionsDeveloperKey);
    return settings;
}

std::vector<std::string> AppSettings::secrets() const {
    std::vector<std::string> out;
    for (const std::string* key : {&cartoKey, &sayIntentionsApiKey, &sayIntentionsDeveloperKey}) {
        if (!key->empty()) out.push_back(*key);
    }
    return out;
}

std::string redact(std::string_view text, const std::vector<std::string>& secrets) {
    std::string out(text);
    for (const std::string& secret : secrets) out = redact(out, secret);
    return out;
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

std::string redact(std::string_view text, std::string_view secret) {
    std::string out(text);
    if (secret.empty()) return out;
    constexpr std::string_view mask = "<key>";
    for (std::size_t at = out.find(secret); at != std::string::npos; at = out.find(secret, at + mask.size())) {
        out.replace(at, secret.size(), mask);
    }
    return out;
}

}  // namespace blocktime
