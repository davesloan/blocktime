// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Stand-ins for a PMDG SDK header that wasn't found at build time (D-116): the probe then logs GSX, the SimVars and
// facilities only. BLOCKTIME_PMDG_NO_NG3 / BLOCKTIME_PMDG_NO_777X say which halves this file stands in for.

#include "pmdg_probe.h"

namespace blocktime::probe {

#ifdef BLOCKTIME_PMDG_NO_NG3
bool registerPmdg737(void*, unsigned long) { return false; }
nlohmann::json decodePmdg737(const void*, std::size_t) { return nullptr; }
#endif

#ifdef BLOCKTIME_PMDG_NO_777X
bool registerPmdg777(void*, unsigned long) { return false; }
nlohmann::json decodePmdg777(const void*, std::size_t) { return nullptr; }
#endif

}  // namespace blocktime::probe
