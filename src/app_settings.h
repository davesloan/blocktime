// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The app's local settings file and map tile cache (D-059). Both live in %LOCALAPPDATA%\BlockTime, never in the
// repo: settings.json holds the CARTO key, which must never be committed, logged or shown.

#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace blocktime {

struct AppSettings {
    std::string cartoKey;  // "" means no map tiles
};

// Reads `file` (JSON). A missing file, bad JSON or a non-string carto_key gives an empty key. A UTF-8 byte order
// mark is allowed.
AppSettings loadAppSettings(const std::filesystem::path& file);

std::filesystem::path defaultSettingsFile();   // %LOCALAPPDATA%\BlockTime\settings.json
std::filesystem::path defaultTileCacheDir();   // %LOCALAPPDATA%\BlockTime\tiles

// Deletes regular files under `dir` (recursively) whose last write is older than `maxAge`. Returns how many were
// deleted. A missing folder deletes nothing.
std::size_t pruneOldFiles(const std::filesystem::path& dir, std::chrono::hours maxAge);

inline constexpr std::chrono::hours kTileMaxAge{24 * 30};  // CARTO's terms: cache tiles at most 30 days

// `text` with every copy of `secret` replaced by "<key>", so the CARTO key can't reach a log through an error message
// that quotes a tile URL. An empty secret changes nothing.
std::string redact(std::string_view text, std::string_view secret);

}  // namespace blocktime
