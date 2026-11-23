// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The probe's PMDG part (D-116): subscribes to the PMDG 737 and 777 SDK data broadcasts and picks out the switches in
// docs/notes/pmdg-variables-from-spad.md by their SDK header names. The headers ship with each PMDG aircraft and
// aren't committed: the build finds them through BLOCKTIME_PMDG_NG3_SDK_DIR and BLOCKTIME_PMDG_777X_SDK_DIR. Without
// one, its register function returns false and nothing is requested.

#pragma once

#include <cstddef>

#include <nlohmann/json.hpp>

namespace blocktime::probe {

// `handle` is the SimConnect HANDLE. True if the broadcast was subscribed (the header was there at build time).
bool registerPmdg737(void* handle, unsigned long requestId);
bool registerPmdg777(void* handle, unsigned long requestId);

// The fields of interest from one data block, or null if the block is too small to be that aircraft's.
nlohmann::json decodePmdg737(const void* data, std::size_t size);
nlohmann::json decodePmdg777(const void* data, std::size_t size);

}  // namespace blocktime::probe
