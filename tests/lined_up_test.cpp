// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// LinedUpDetector: "Final approach" when lined up with a destination runway (D-143, D-145, D-148).

#include <chrono>
#include <cmath>
#include <numbers>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "lined_up.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

constexpr double kLat0 = 33.0, kLon0 = -117.0, kMPerDeg = 111320.0, kNm = 1852.0;
const TimePoint kT0 = sys_days{2026y / 11 / 24} + 18h;

// Runway 09/27, 3,000 m long, centred on (kLat0, kLon0); and its parallel 09L/27R, 230 m north.
std::vector<Runway> runways() {
    Runway main{kLat0, kLon0, 90.0, 3000.0, 45.0, 9, 0, 27, 0};
    Runway north{kLat0 + 230.0 / kMPerDeg, kLon0, 90.0, 3000.0, 45.0, 9, 1, 27, 2};
    return {main, north};
}

double latAt(double northM) { return kLat0 + northM / kMPerDeg; }
double lonAt(double eastM) { return kLon0 + eastM / (kMPerDeg * std::cos(kLat0 * std::numbers::pi / 180.0)); }

// Feeds `n` samples one second apart, starting `eastM`/`northM` from the runway centre and moving 72 m/s (140 kt)
// along `trackDeg`. Returns the first detection, if any.
class Approach {
public:
    Approach() : d_(runways()) {}
    std::optional<LinedUp> fly(double eastM, double northM, double trackDeg, int n, Phase phase = Phase::Approach) {
        const double rad = trackDeg * std::numbers::pi / 180.0;
        std::optional<LinedUp> first;
        for (int i = 0; i < n; ++i) {
            PhaseSample s;
            s.time = kT0 + seconds(t_);
            s.activeS = t_;
            s.lat = latAt(northM + 72.0 * i * std::cos(rad));
            s.lon = lonAt(eastM + 72.0 * i * std::sin(rad));
            s.altFt = 2500;
            s.aglFt = 2500;
            auto r = d_.onSample(s, phase);
            if (r && !first) first = r;
            ++t_;
        }
        return first;
    }
    LinedUpDetector& detector() { return d_; }

private:
    LinedUpDetector d_;
    int t_ = 0;
};

// The threshold of 27 is 1,500 m east of the centre.
constexpr double kThreshold27 = 1500.0;

}  // namespace

TEST(LinedUp, FiresAfterTenSeconds) {
    Approach a;
    const auto r = a.fly(kThreshold27 + 8 * kNm, 0, 270, 16);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->runway, "27");
    // The hold starts once a track is known (150 m of movement, the third sample).
    EXPECT_LE(duration_cast<seconds>(r->time - kT0).count(), 3);
    EXPECT_EQ(r->altFt, 2500);
    EXPECT_NEAR(r->distanceNm, 7.8, 0.15);
}

TEST(LinedUp, NotBeforeTenSeconds) {
    Approach a;
    EXPECT_FALSE(a.fly(kThreshold27 + 8 * kNm, 0, 270, 12).has_value());  // a track from the fourth sample: 8 s held
}

TEST(LinedUp, DownwindIsNot) {
    Approach a;
    EXPECT_FALSE(a.fly(kThreshold27, -1.5 * kNm, 90, 60).has_value());
}

TEST(LinedUp, OutsideTenNmNotYet) {
    Approach a;
    EXPECT_FALSE(a.fly(kThreshold27 + 12 * kNm, 0, 270, 20).has_value());
    Approach b;
    EXPECT_TRUE(b.fly(kThreshold27 + 9.9 * kNm, 0, 270, 20).has_value());
}

TEST(LinedUp, OffsetMoreThanHalfNmIsNot) {
    Approach a;
    EXPECT_FALSE(a.fly(kThreshold27 + 6 * kNm, -0.6 * kNm, 270, 20).has_value());
}

TEST(LinedUp, PicksNearerParallel) {
    Approach a;
    const auto r = a.fly(kThreshold27 + 6 * kNm, 230, 270, 20);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->runway, "27R");
}

TEST(LinedUp, OtherDirection) {
    Approach a;  // landing 09: from the west, heading east
    const auto r = a.fly(-kThreshold27 - 5 * kNm, 0, 90, 20);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->runway, "09");
}

TEST(LinedUp, PastThresholdIsNot) {
    Approach a;  // climbing out westward beyond 27's threshold, as on a missed approach
    EXPECT_FALSE(a.fly(kThreshold27 - 500, 0, 270, 30).has_value());
}

TEST(LinedUp, OnlyInDescentOrApproach) {
    Approach a;
    EXPECT_FALSE(a.fly(kThreshold27 + 8 * kNm, 0, 270, 20, Phase::Cruise).has_value());
    Approach b;
    EXPECT_TRUE(b.fly(kThreshold27 + 8 * kNm, 0, 270, 20, Phase::Descent).has_value());
}

TEST(LinedUp, OnceUntilGoAround) {
    Approach a;
    ASSERT_TRUE(a.fly(kThreshold27 + 8 * kNm, 0, 270, 16).has_value());
    EXPECT_FALSE(a.fly(kThreshold27 + 6 * kNm, 0, 270, 16).has_value());
}

TEST(LinedUp, FiresAgainAfterGoAround) {
    Approach a;
    ASSERT_TRUE(a.fly(kThreshold27 + 8 * kNm, 0, 270, 16).has_value());
    a.fly(kThreshold27 - 500, 0, 270, 2, Phase::GoAround);
    const auto r = a.fly(kThreshold27 + 6 * kNm, 230, 270, 16);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->runway, "27R");
}
