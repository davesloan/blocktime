// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "geo.h"
#include "route_progress.h"
#include "simbrief.h"

using namespace blocktime;

namespace {

constexpr double kNm = 1852.0;

std::string fixture(const char* name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(RouteProgress, FollowsTheRoute) {
    RouteProgress r({{0, 0}, {0, 1}, {1, 1}});
    r.update(0, 0.5);
    EXPECT_NEAR(r.flownNm(), 30, 0.5);
    r.update(0.5, 1);
    EXPECT_NEAR(r.flownNm(), 90, 0.5);
    EXPECT_DOUBLE_EQ(r.toGoNm() + r.flownNm(), r.totalNm());
    EXPECT_NEAR(r.share(), 0.75, 0.01);
}

TEST(RouteProgress, NeverGoesBack) {
    RouteProgress r({{0, 0}, {0, 1}, {1, 1}});
    r.update(0, 0.8);
    const double flown = r.flownNm();
    r.update(0, 0.5);
    EXPECT_DOUBLE_EQ(r.flownNm(), flown);
}

TEST(RouteProgress, DoublingBackNeverGoesBack) {
    // Leaves (0,0) east, loops round and comes back within 1 NM of it on leg 5, then carries on west.
    RouteProgress r({{0, 0}, {0, 1}, {1, 1}, {2, 1}, {2, 0}, {1, 0}, {0.01, 0}, {-1, 0}});
    r.update(0, 0.3);
    const double early = r.flownNm();
    r.update(0.005, 0.0);  // near the shared point while still on the first leg: no jump to leg 5
    EXPECT_LT(r.flownNm(), early + 1);
    for (const RoutePoint p : std::vector<RoutePoint>{{0, 0.9}, {0.5, 1}, {1.5, 1}, {2, 0.5}, {1.5, 0}, {0.5, 0}}) {
        r.update(p.lat, p.lon);
    }
    const double late = r.flownNm();
    EXPECT_GT(late, 290);  // about 300 NM to (0.5, 0) along the loop
    r.update(0.005, 0.0);  // near the shared point late in the route: stays on the late leg
    EXPECT_GT(r.flownNm(), late);
    EXPECT_LT(r.flownNm(), late + 32);
}

TEST(RouteProgress, NoPlanIsTheStraightLine) {
    RouteProgress r({{32.7336, -117.19}, {37.3626, -121.929}});
    EXPECT_NEAR(r.totalNm(), distanceMeters(32.7336, -117.19, 37.3626, -121.929) / kNm, 0.01);
    EXPECT_GT(r.totalNm(), 300);
}

TEST(RouteProgress, FinishIsTheWholeRoute) {
    RouteProgress r({{0, 0}, {0, 1}});
    r.update(0, 0.2);
    r.finish();
    EXPECT_DOUBLE_EQ(r.flownNm(), r.totalNm());
    EXPECT_DOUBLE_EQ(r.share(), 1.0);
    r.reset();
    EXPECT_DOUBLE_EQ(r.flownNm(), 0);
}

TEST(RouteProgress, TooShortARouteIsEmpty) {
    RouteProgress r({{0, 0}});
    r.update(0, 1);
    EXPECT_DOUBLE_EQ(r.totalNm(), 0);
    EXPECT_DOUBLE_EQ(r.share(), 0);
}

TEST(RouteProgress, KsanKsjcFixture) {
    std::vector<RoutePoint> route{{32.7336, -117.19}};
    for (const RoutePoint& p : parseNavlog(fixture("ofp-ksan-ksjc-navlog.json"))) route.push_back(p);
    route.push_back({37.3626, -121.929});
    RouteProgress r(route);

    std::ifstream in(BLOCKTIME_SOURCE_DIR "/tests/data/flight-ksan-ksjc/events-2026-11-25.jsonl");
    std::string line;
    bool airborne = false;
    int samples = 0;
    double last = 0;
    while (std::getline(in, line)) {
        const nlohmann::json j = nlohmann::json::parse(line, nullptr, false);
        if (!j.is_object()) continue;
        const std::string type = j.value("type", "");
        if (type == "liftoff") airborne = true;
        if (type == "touchdown") break;
        if (!airborne || type != "telemetry") continue;
        const nlohmann::json& d = j["data"];
        if (!d.contains("lat") || !d.contains("lon")) continue;
        r.update(d["lat"].get<double>(), d["lon"].get<double>());
        EXPECT_GE(r.flownNm(), last);
        last = r.flownNm();
        ++samples;
    }
    ASSERT_GT(samples, 100);
    EXPECT_GT(r.flownNm(), r.totalNm() - 5);
}

TEST(AltitudeArc, KeepsThePeakPerPiece) {
    AltitudeArc arc;
    arc.record(0.005, 3000);
    arc.record(0.009, 5000);
    arc.record(0.5, 33000);
    const std::vector<double> peaks = arc.peaks();
    ASSERT_EQ(peaks.size(), static_cast<std::size_t>(AltitudeArc::kPieces));
    EXPECT_DOUBLE_EQ(peaks[0], 5000);
    EXPECT_DOUBLE_EQ(peaks[50], 33000);
    EXPECT_DOUBLE_EQ(peaks[10], -1);
    arc.record(1.0, 900);  // the end of the route falls in the last piece
    EXPECT_DOUBLE_EQ(arc.peaks()[99], 900);
    arc.reset();
    EXPECT_DOUBLE_EQ(arc.peaks()[50], -1);
}

TEST(AltitudeArc, ShareScales) {
    EXPECT_NEAR(altitudeShare(11000, 33000), 1.0 / 3, 0.001);
    EXPECT_DOUBLE_EQ(altitudeShare(40000, 33000), 1.0);
    EXPECT_DOUBLE_EQ(altitudeShare(17500, 0), 0.5);
    EXPECT_DOUBLE_EQ(altitudeShare(-50, 33000), 0.0);
}
