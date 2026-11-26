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

void expectMarker(const PillMarker& m, int section, int part, int parts, const std::string& label) {
    EXPECT_EQ(m.section, section);
    EXPECT_EQ(m.part, part);
    EXPECT_EQ(m.parts, parts);
    EXPECT_EQ(m.label, label);
}

}  // namespace

TEST(PhasePill, SixSections) {
    const auto p = pillSections(stages(3));
    ASSERT_EQ(p.size(), 6u);
    EXPECT_EQ(p[0].name, "Pre-Flight");
    EXPECT_EQ(p[2].state, S::Done);
    EXPECT_EQ(p[3].state, S::Current);
    EXPECT_EQ(p[4].state, S::Upcoming);
}

TEST(PhasePill, PreFlightIsPreparingUntilBoarding) {
    expectMarker(pillMarker(stages(0), "", false), 0, 0, 2, "Preparing");
    expectMarker(pillMarker(stages(0), "", true), 0, 1, 2, "Boarding");
}

TEST(PhasePill, DepartureSlots) {
    expectMarker(pillMarker(stages(1), "pushback", true), 1, 0, 3, "Pushback");
    expectMarker(pillMarker(stages(1), "taxi_out", true), 1, 1, 3, "Taxiing");
    expectMarker(pillMarker(stages(1), "takeoff", true), 1, 2, 3, "Takeoff");
}

TEST(PhasePill, ClimbAndCruiseHaveOneSlotAndNoLabel) {
    expectMarker(pillMarker(stages(2), "climb", false), 2, 0, 1, "");
    expectMarker(pillMarker(stages(3), "cruise", false), 3, 0, 1, "");
}

TEST(PhasePill, DescentSlotsWithGoAroundAsApproach) {
    expectMarker(pillMarker(stages(4), "descent", false), 4, 0, 2, "Descending");
    expectMarker(pillMarker(stages(4), "approach", false), 4, 1, 2, "Approach");
    expectMarker(pillMarker(stages(4), "go_around", false), 4, 1, 2, "Approach");
}

TEST(PhasePill, ArrivalFromLandingToTheGate) {
    expectMarker(pillMarker(stages(5), "landing", false), 5, 0, 3, "Landing");
    expectMarker(pillMarker(stages(5), "taxi_in", false), 5, 1, 3, "Taxiing");
    expectMarker(pillMarker(stages(5), "arrived", false), 5, 2, 3, "At Gate");
}
