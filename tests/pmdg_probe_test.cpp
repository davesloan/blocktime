// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pmdg_probe_common.h"

#include <cstring>

#include <gtest/gtest.h>

using blocktime::probe::fmcFields;

namespace {

// The FMC fields as PMDG's headers declare them (identical in PMDG_NG3_SDK.h and PMDG_777X_SDK.h), which aren't
// committed, so the tests use this stand-in.
struct FakePmdgData {
    unsigned char FMC_TakeoffFlaps = 5;
    unsigned char FMC_V1 = 142;
    unsigned char FMC_VR = 145;
    unsigned char FMC_V2 = 151;
    unsigned char FMC_LandingFlaps = 30;
    unsigned char FMC_LandingVREF = 138;
    unsigned short FMC_CruiseAlt = 37000;
    short FMC_LandingAltitude = 13;
    unsigned short FMC_TransitionAlt = 18000;
    unsigned short FMC_TransitionLevel = 18000;
    bool FMC_PerfInputComplete = true;
    float FMC_DistanceToTOD = 123.6f;
    float FMC_DistanceToDest = -1.0f;
    char FMC_flightNumber[9] = {'X', 'L', 'Y', '2', '1', '2', 0, 0, 0};
};

}  // namespace

TEST(PmdgProbe, FmcFieldsRoundTheDistances) {
    const auto j = fmcFields(FakePmdgData{});
    EXPECT_EQ(j["FMC_DistanceToTOD_nm"], 124);
    EXPECT_EQ(j["FMC_DistanceToDest_nm"], -1) << "negative means not available, and stays negative";
    EXPECT_EQ(j["FMC_CruiseAlt"], 37000);
    EXPECT_EQ(j["FMC_V2"], 151);
    EXPECT_EQ(j["FMC_PerfInputComplete"], true);
}

TEST(PmdgProbe, FmcFlightNumberWithoutATerminator) {
    FakePmdgData d;
    std::memcpy(d.FMC_flightNumber, "XLY123456", 9);  // all nine bytes used, no '\0'
    EXPECT_EQ(fmcFields(d)["FMC_flightNumber"], "XLY123456");
    d.FMC_flightNumber[3] = '\0';
    EXPECT_EQ(fmcFields(d)["FMC_flightNumber"], "XLY");
}
