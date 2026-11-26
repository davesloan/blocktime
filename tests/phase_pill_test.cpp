// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <vector>

#include <gtest/gtest.h>

#include "phase_pill.h"

using namespace blocktime;

namespace {

using S = StageState;

// Six stages: done up to `current` (exclusive), `current` in progress, the rest upcoming.
std::vector<StageView> stages(int current) {
    const char* names[] = {"Pre-Flight", "Departure", "Climb", "Cruise", "Descent", "Arrival"};
    std::vector<StageView> v;
    for (int i = 0; i < 6; ++i) {
        v.push_back({static_cast<Stage>(i), names[i], i < current ? S::Done : i == current ? S::Current : S::Upcoming,
                     {}});
    }
    return v;
}

}  // namespace

TEST(PhasePill, SixSectionsWithFixedDivisions) {
    const auto p = pillSections(stages(0), "");
    ASSERT_EQ(p.size(), 6u);
    EXPECT_EQ(p[0].name, "Pre-Flight");
    const std::vector<std::size_t> counts{0, 3, 0, 0, 4, 0};
    for (std::size_t i = 0; i < 6; ++i) EXPECT_EQ(p[i].divisions.size(), counts[i]) << i;
    EXPECT_EQ(p[0].state, S::Current);
}

TEST(PhasePill, TaxiOutInDeparture) {
    const auto p = pillSections(stages(1), "taxi_out");
    EXPECT_EQ(p[1].divisions, (std::vector<S>{S::Done, S::Current, S::Upcoming}));
    EXPECT_EQ(p[4].divisions, (std::vector<S>{S::Upcoming, S::Upcoming, S::Upcoming, S::Upcoming}));
}

TEST(PhasePill, GoAroundIsApproach) {
    const auto p = pillSections(stages(4), "go_around");
    EXPECT_EQ(p[4].divisions, (std::vector<S>{S::Done, S::Current, S::Upcoming, S::Upcoming}));
}

TEST(PhasePill, DoneStageAllDone) {
    const auto p = pillSections(stages(3), "cruise");
    EXPECT_EQ(p[1].divisions, (std::vector<S>{S::Done, S::Done, S::Done}));
    EXPECT_EQ(p[3].state, S::Current);
}
