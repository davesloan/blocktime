// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Dave's own taxi times for the predictions (D-212): the median of his last 5 completed flights at the airport, read
// from the flights and their phases already in the database (docs/database.md).

#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "sqlite_db.h"

namespace blocktime {

struct TaxiHistory {
    std::optional<int> outS, inS;
};

// The median taxi out from `originIcao` and taxi in to `arrivalIcao` over the last 5 completed flights each, among
// flights that blocked in before `beforeInstant` ("" = all). Out: block_out_at to the first `takeoff` phase. In: the
// last `landing` phase to block_in_at. Flights without that phase are skipped. nullopt where none are left.
TaxiHistory taxiHistory(db::Database& db, std::string_view originIcao, std::string_view arrivalIcao,
                        std::string_view beforeInstant);

// An even count gives the mean of the middle two, rounded down. `values` is not empty.
int medianOf(std::vector<int> values);

}  // namespace blocktime
