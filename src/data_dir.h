// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one folder all of BlockTime's data lives in (D-226): the event log, settings.json, blocktime.db, the config
// files and errors.log. It is %LOCALAPPDATA%\BlockTime unless the app chose another at start: the debug build's own
// copy, --data-dir, or a scratch copy for --demo and --grab, so development never writes to Dave's airline.

#pragma once

#include <filesystem>
#include <string>

namespace blocktime {

std::filesystem::path realDataDir();  // %LOCALAPPDATA%\BlockTime
std::filesystem::path dataDir();      // the folder set by setDataDir, else realDataDir()

// Call once at start, before anything reads a default path (the app's main).
void setDataDir(std::filesystem::path dir);

// Fills an empty `to` from `from` for a development run: blocktime.db through VACUUM INTO (a consistent copy even
// while a recording copy has it open), settings.json and the .yaml config files copied, and the event logs copied
// when `withEvents`. Files `from` doesn't have are skipped. False with `error` if something couldn't be copied.
bool seedDataDir(const std::filesystem::path& from, const std::filesystem::path& to, bool withEvents,
                 std::string* error);

}  // namespace blocktime
