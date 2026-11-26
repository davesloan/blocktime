// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The one reading of the logged cockpit and altimeter lines, shared by the debrief and the in-flight panel's lamps
// (D-159, D-195).

#include <gtest/gtest.h>

#include "cockpit_reading.h"

using namespace blocktime;

TEST(CockpitReading, FamilyFromTheFolderOrTheFamily) {
    EXPECT_EQ(familyOf("pmdg 737-800bbj2"), Family::B737);
    EXPECT_EQ(familyOf("PMDG 777-200ER"), Family::B777);
    EXPECT_EQ(familyOf("777"), Family::B777);
    EXPECT_EQ(familyOf(""), Family::B737);
}

TEST(CockpitReading, SeatBeltsAutoCountsAsOn) {
    EXPECT_FALSE(beltsOn({{"seat_belts", 0}}));
    EXPECT_TRUE(beltsOn({{"seat_belts", 1}}));
    EXPECT_TRUE(beltsOn({{"seat_belts", 2}}));
    EXPECT_FALSE(beltsOn(nlohmann::json::object()));
}

TEST(CockpitReading, LandingLightsAndGear) {
    EXPECT_TRUE(landingLightsOn({{"landing_lights", true}}));
    EXPECT_FALSE(landingLightsOn({{"landing_lights", false}}));
    EXPECT_TRUE(gearDown({{"gear_down", true}}));
    EXPECT_FALSE(gearDown(nlohmann::json::object()));
}

TEST(CockpitReading, ApuSwitchUnknownWithoutPmdgData) {
    EXPECT_EQ(apuSwitchOn({{"apu_switch", true}}), std::optional<bool>(true));
    EXPECT_EQ(apuSwitchOn({{"apu_switch", false}}), std::optional<bool>(false));
    EXPECT_FALSE(apuSwitchOn({{"seat_belts", 2}}).has_value());
}

TEST(CockpitReading, AltimetersStdOnlyWhenBothAre) {
    EXPECT_TRUE(altimetersStd({{"capt_mb", 1013.2}, {"fo_mb", 1013.2}}));
    EXPECT_FALSE(altimetersStd({{"capt_mb", 1013.2}, {"fo_mb", 1019.0}}));
    EXPECT_FALSE(altimetersStd({{"capt_mb", 1019.0}, {"fo_mb", 1019.0}}));
}

TEST(CockpitReading, BeltsNeedPmdgDataOnThe737) {
    EXPECT_FALSE(beltsReadable(Family::B737, false));
    EXPECT_TRUE(beltsReadable(Family::B737, true));
    EXPECT_TRUE(beltsReadable(Family::B777, false));
}
