// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// PMDG's SDK data broadcasts for the 737 and the 777. SimConnection subscribes on every session (D-159) and decodes
// the few values the debrief needs (decodeCockpit*); with --probe the same block is also decoded into the switches in
// docs/notes/pmdg-variables-from-spad.md by their SDK header names (D-116). The headers ship with each PMDG aircraft
// and aren't committed: the build finds them through BLOCKTIME_PMDG_NG3_SDK_DIR and BLOCKTIME_PMDG_777X_SDK_DIR.
// Without one, its register function returns false, nothing is requested and the debrief marks those checks as not
// checked.

#pragma once

#include <cstddef>
#include <optional>

#include <nlohmann/json.hpp>

#include "cockpit_monitor.h"

namespace blocktime::probe {

// `handle` is the SimConnect HANDLE. True if the broadcast was subscribed (the header was there at build time).
bool registerPmdg737(void* handle, unsigned long requestId);
bool registerPmdg777(void* handle, unsigned long requestId);

// The fields of interest from one data block, or null if the block is too small to be that aircraft's.
nlohmann::json decodePmdg737(const void* data, std::size_t size);
nlohmann::json decodePmdg777(const void* data, std::size_t size);

// The debrief's values from one data block (cockpit_monitor.h), or nullopt if the block is too small.
std::optional<PmdgCockpit> decodeCockpit737(const void* data, std::size_t size);
std::optional<PmdgCockpit> decodeCockpit777(const void* data, std::size_t size);

}  // namespace blocktime::probe
