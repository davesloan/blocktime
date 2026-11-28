// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <gtest/gtest.h>

#include "your_flight_stage.h"

using namespace blocktime;
using S = YourFlightStage;

namespace {

StageInputs in(std::initializer_list<bool StageInputs::*> on) {
    StageInputs s;
    for (bool StageInputs::* f : on) s.*f = true;
    return s;
}

}  // namespace

// One row per rule, in the order they win (review 3.1).
TEST(YourFlightStage, Table) {
    using I = StageInputs;
    EXPECT_EQ(yourFlightStage(in({})), S::Empty);
    EXPECT_EQ(yourFlightStage(in({&I::formOpen})), S::Form);
    EXPECT_EQ(yourFlightStage(in({&I::planned})), S::Dispatched);
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::planImported})), S::Imported);
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::planImported, &I::accepted})), S::Accepted);
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::planImported, &I::accepted, &I::flightOpen})), S::Flying);
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::planImported, &I::accepted, &I::settlePending})), S::Arrived);
    EXPECT_EQ(yourFlightStage(in({&I::summaryShowing, &I::finishable})), S::Arrived);
    EXPECT_EQ(yourFlightStage(in({&I::summaryShowing})), S::Summary);
    // Change flight: the form over a dispatched or imported flight.
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::formOpen})), S::Form);
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::planImported, &I::formOpen})), S::Form);
    // Change flight isn't offered after Accept.
    EXPECT_EQ(yourFlightStage(in({&I::planned, &I::planImported, &I::accepted, &I::formOpen})), S::Accepted);
    // A flight open wins over a settle still pending from before.
    EXPECT_EQ(yourFlightStage(in({&I::flightOpen, &I::settlePending})), S::Flying);
    // The form over a summary (At Gate Turnaround).
    EXPECT_EQ(yourFlightStage(in({&I::summaryShowing, &I::formOpen})), S::Summary);
}

TEST(YourFlightStage, Names) {
    EXPECT_EQ(stageName(S::Empty), "empty");
    EXPECT_EQ(stageName(S::Form), "form");
    EXPECT_EQ(stageName(S::Dispatched), "dispatched");
    EXPECT_EQ(stageName(S::Imported), "imported");
    EXPECT_EQ(stageName(S::Accepted), "accepted");
    EXPECT_EQ(stageName(S::Flying), "flying");
    EXPECT_EQ(stageName(S::Arrived), "arrived");
    EXPECT_EQ(stageName(S::Summary), "summary");
}
