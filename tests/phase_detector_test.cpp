// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The phase rules (recorder.md "Phases", D-124), replayed on two of Dave's real flights and on small made-up ones.
// The replay fixtures are telemetry cut from the event log between block_out and block_in (2026-11-23).

#include <chrono>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

#include "active_clock.h"
#include "event_log.h"
#include "phase_detector.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

TimePoint parseTs(const std::string& text) {
    sys_time<milliseconds> t{};
    std::istringstream in(text);
    in >> parse("%Y-%m-%dT%H:%M:%S", t);
    return time_point_cast<TimePoint::duration>(t);
}

std::string hms(TimePoint t) { return formatUtc(t).substr(11, 8); }

struct Replay {
    TimePoint blockOut{}, blockIn{};
    std::vector<PhaseSample> samples;
};

// Reads a replay fixture; activeS comes from an ActiveClock fed each sample's time, as the recorder's would be.
Replay loadReplay(const std::string& name) {
    std::ifstream in(std::string(BLOCKTIME_SOURCE_DIR "/tests/data/") + name);
    Replay r;
    ActiveClock clock;
    std::string line;
    std::getline(in, line);  // header
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        if (line.rfind("# block_out ", 0) == 0) {
            r.blockOut = parseTs(line.substr(12));
            continue;
        }
        if (line.rfind("# block_in ", 0) == 0) {
            r.blockIn = parseTs(line.substr(11));
            continue;
        }
        std::istringstream f(line);
        std::string ts, field;
        std::getline(f, ts, ',');
        std::vector<double> v;
        while (std::getline(f, field, ',')) v.push_back(std::stod(field));
        PhaseSample s;
        s.time = parseTs(ts);
        clock.onMessage(steady_clock::time_point(duration_cast<steady_clock::duration>(s.time.time_since_epoch())));
        s.activeS = clock.seconds();
        s.parkingBrake = v[0] != 0;
        s.onGround = v[1] != 0;
        s.groundSpeedKt = v[2];
        s.altFt = v[3];
        s.aglFt = v[4];
        s.vsFpm = v[5];
        s.lat = v[6];
        s.lon = v[7];
        r.samples.push_back(s);
    }
    return r;
}

// One line per event: "<key> <HH:MM:SS>", the key naming what happened ("climb", "level:climb:2000",
// "ended:approach:3000", "step:37000", "stepped:39000").
struct Seen {
    std::string key;
    TimePoint time;
};

Seen describe(const PhaseEvent& e) {
    return std::visit(
        [](const auto& x) -> Seen {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, PhaseChanged>) {
                return {std::string(phaseCode(x.phase)), x.time};
            } else if constexpr (std::is_same_v<T, LevelOff>) {
                return {"level:" + std::string(phaseCode(x.phase)) + ":" + std::to_string(x.altFt), x.time};
            } else if constexpr (std::is_same_v<T, LevelOffEnded>) {
                return {"ended:" + std::string(phaseCode(x.phase)) + ":" + std::to_string(x.altFt), x.time};
            } else if constexpr (std::is_same_v<T, StepClimb>) {
                return {"step:" + std::to_string(x.fromFt), x.time};
            } else {
                return {"stepped:" + std::to_string(x.altFt), x.time};
            }
        },
        e);
}

std::vector<Seen> run(PhaseDetector& d, const std::vector<PhaseSample>& samples) {
    std::vector<Seen> seen;
    for (const auto& s : samples)
        for (const auto& e : d.onSample(s)) seen.push_back(describe(e));
    return seen;
}

void expectEvents(const std::vector<Seen>& seen, const std::vector<std::pair<std::string, std::string>>& expected) {
    std::vector<std::string> keys, wantKeys;
    for (const auto& s : seen) keys.push_back(s.key);
    for (const auto& w : expected) wantKeys.push_back(w.first);
    ASSERT_EQ(keys, wantKeys);
    for (std::size_t i = 0; i < seen.size(); ++i) {
        const TimePoint want = parseTs(formatUtc(seen[i].time).substr(0, 11) + expected[i].second);
        const auto off = duration_cast<milliseconds>(seen[i].time - want).count();
        EXPECT_LE(std::abs(off), 3000) << seen[i].key << " at " << hms(seen[i].time) << ", expected "
                                       << expected[i].second;
    }
}

PhasePlan plan(int cruiseFt, double destLat, double destLon) {
    PhasePlan p;
    p.cruiseAltitudeFt = cruiseFt;
    p.transitionAltitudeFt = 18000;
    p.transitionLevelFt = 18000;
    p.hasDestination = true;
    p.destinationLat = destLat;
    p.destinationLon = destLon;
    return p;
}

}  // namespace

TEST(PhaseDetector, ReplaysKbfiKpdx) {
    const Replay r = loadReplay("replay-kbfi-kpdx.csv");
    PhaseDetector d(plan(20000, 45.5887, -122.5975), r.blockOut, 25);
    EXPECT_EQ(describe(d.started()).key, "pushback");
    EXPECT_EQ(hms(d.started().time), "22:51:08");
    const auto seen = run(d, r.samples);
    expectEvents(seen, {{"taxi_out", "22:54:17"},
                        {"takeoff", "22:56:08"},  // 40 kt at 22:56:16; timed from where the roll began
                        {"climb", "22:56:51"},
                        {"level:climb:2000", "22:57:15"},
                        {"ended:climb:2000", "22:57:57"},
                        {"level:climb:11000", "23:00:05"},
                        {"ended:climb:11000", "23:00:31"},
                        {"cruise", "23:03:10"},
                        {"descent", "23:04:32"},
                        {"level:descent:10000", "23:11:40"},
                        {"ended:descent:10000", "23:12:26"},
                        {"approach", "23:12:34"},
                        {"level:approach:5000", "23:17:38"},
                        {"ended:approach:5000", "23:20:23"},
                        {"level:approach:2000", "23:23:55"},
                        {"ended:approach:2000", "23:24:31"},
                        {"landing", "23:27:16"},
                        {"taxi_in", "23:27:40"}});
    EXPECT_EQ(describe(d.onBlockIn(r.blockIn, 27)).key, "arrived");
}

TEST(PhaseDetector, ReplaysKpdxKsfo) {
    const Replay r = loadReplay("replay-kpdx-ksfo.csv");
    PhaseDetector d(plan(39000, 37.6190, -122.3750), r.blockOut, 28);
    const auto seen = run(d, r.samples);
    expectEvents(seen, {{"taxi_out", "23:57:43"},
                        {"takeoff", "00:03:13"},  // 40 kt at 00:03:21
                        {"climb", "00:04:06"},
                        {"cruise", "00:23:12"},
                        {"descent", "00:56:25"},
                        {"level:descent:11000", "01:16:28"},
                        {"ended:descent:11000", "01:17:55"},
                        {"approach", "01:18:19"},
                        {"level:approach:6000", "01:20:35"},
                        {"ended:approach:6000", "01:22:00"},
                        {"level:approach:4000", "01:23:15"},
                        {"ended:approach:4000", "01:24:25"},
                        // the window's mean was 3,100 while the altitude settled; the whole level's mean is 3,000
                        {"level:approach:3100", "01:25:02"},
                        {"ended:approach:3000", "01:26:25"},
                        {"landing", "01:30:29"},
                        {"taxi_in", "01:30:43"}});
}

// ---- Made-up flights -------------------------------------------------------------------------------------------

namespace {

const TimePoint kT0 = sys_days{2026y / 11 / 23} + 12h;

// Builds samples one second apart (active time too, unless a freeze is added).
class Flight {
public:
    explicit Flight(PhasePlan p = plan(35000, 0, 10)) : d_(p, kT0, 20) {}

    // `n` samples, one a second, moving altitude by vs each second.
    Flight& air(int n, double altFt, double vsFpm, double aglOffset = 0) {
        for (int i = 0; i < n; ++i) {
            PhaseSample s = base();
            s.altFt = altFt + vsFpm / 60.0 * i;
            s.aglFt = s.altFt + aglOffset;
            s.vsFpm = vsFpm;
            s.groundSpeedKt = 250;
            push(s);
        }
        return *this;
    }
    Flight& ground(int n, double gsKt, bool brake = false, double gsStepKt = 0) {
        for (int i = 0; i < n; ++i) {
            PhaseSample s = base();
            s.onGround = true;
            s.parkingBrake = brake;
            s.altFt = 20;
            s.aglFt = 8;
            s.groundSpeedKt = gsKt + gsStepKt * i;
            push(s);
        }
        return *this;
    }
    // The sim sends nothing for `wallS` seconds; the active clock moves only 2 s.
    Flight& freeze(int wallS) {
        wall_ += wallS - 1;
        active_ += 1;
        return *this;
    }
    // Takes off and climbs to 1,500 ft AGL so later samples start in Climb.
    Flight& toClimb() {
        ground(5, 0, true).ground(1, 7).ground(1, 20).ground(10, 30, false, 3);
        return air(1, 50, 2000).air(1, 1500, 2000);
    }
    PhaseSample base() const {
        PhaseSample s;
        s.time = kT0 + seconds(wall_);
        s.activeS = active_;
        s.lat = lat_;
        s.lon = 0;
        return s;
    }
    void push(const PhaseSample& s) {
        for (const auto& e : d_.onSample(s)) seen_.push_back(describe(e));
        ++wall_;
        ++active_;
    }
    Flight& at(double lat) {
        lat_ = lat;
        return *this;
    }
    std::vector<std::string> keys() const {
        std::vector<std::string> k;
        for (const auto& s : seen_) k.push_back(s.key);
        return k;
    }
    bool saw(const std::string& key) const {
        for (const auto& s : seen_)
            if (s.key == key) return true;
        return false;
    }
    const std::vector<Seen>& seen() const { return seen_; }
    PhaseDetector& detector() { return d_; }

private:
    PhaseDetector d_;
    std::vector<Seen> seen_;
    long long wall_ = 0;
    double active_ = 0;
    double lat_ = 0;  // the destination is at 0, 10: about 600 nm east
};

}  // namespace

TEST(PhaseDetector, ForwardOnly) {
    Flight f;
    f.toClimb().air(60, 2000, 3000).air(30, 35000, 0).air(30, 35000, 1500).air(30, 35750, 0);
    ASSERT_EQ(f.detector().phase(), Phase::Cruise);
    f.air(30, 35750, 2000).air(40, 36750, 0);
    EXPECT_EQ(f.detector().phase(), Phase::Cruise);
    int climbs = 0;
    for (const auto& k : f.keys()) climbs += k == "climb";
    EXPECT_EQ(climbs, 1);
}

TEST(PhaseDetector, LevelBelowCruiseStaysClimb) {
    Flight f;
    f.toClimb().air(30, 9000, 0);
    EXPECT_TRUE(f.saw("level:climb:9000"));
    EXPECT_EQ(f.detector().phase(), Phase::Climb);
    EXPECT_EQ(f.detector().levelFt(), 9000);
}

TEST(PhaseDetector, NoPlanCruiseAfterFiveMinutes) {
    PhasePlan p;  // no cruise altitude, no destination
    Flight f(p);
    f.toClimb().air(299, 24000, 0);
    EXPECT_EQ(f.detector().phase(), Phase::Climb);
    f.air(3, 24000, 0);
    EXPECT_EQ(f.detector().phase(), Phase::Cruise);
    EXPECT_TRUE(f.saw("ended:climb:24000"));
}

TEST(PhaseDetector, DescentFromClimb) {
    Flight f;
    f.toClimb().air(60, 2000, 3000).air(70, 5000, -1500);
    EXPECT_EQ(f.detector().phase(), Phase::Descent);
}

TEST(PhaseDetector, StepClimbInCruise) {
    Flight g(plan(37000, 0, 10));
    g.toClimb().air(30, 37000, 0).air(60, 37000, 2000).air(40, 39000, 0);
    EXPECT_TRUE(g.saw("step:37000"));
    EXPECT_TRUE(g.saw("stepped:39000"));
    EXPECT_EQ(g.detector().phase(), Phase::Cruise);
    EXPECT_TRUE(g.detector().levelFt() == std::nullopt);
}

TEST(PhaseDetector, StepDescentFarOut) {
    Flight f(plan(37000, 0, 10));
    f.toClimb().air(30, 37000, 0).air(70, 37000, -1000);
    EXPECT_TRUE(f.saw("step:37000"));
    EXPECT_EQ(f.detector().phase(), Phase::Cruise);
}

TEST(PhaseDetector, GoAroundAndBack) {
    Flight f(plan(9000, 0, 0));  // the destination is right here
    f.toClimb().air(30, 9000, 0).air(150, 9000, -1500).air(20, 5250, -800, -4000);
    ASSERT_EQ(f.detector().phase(), Phase::Approach);
    f.air(30, 1000, -700, -400).air(25, 662, 1500, -400);
    EXPECT_EQ(f.detector().phase(), Phase::GoAround);
    f.air(70, 1287, 1500, -400).air(25, 3000, 0, -400);
    EXPECT_EQ(f.detector().phase(), Phase::Approach);
    EXPECT_TRUE(f.saw("level:approach:3000"));
}

TEST(PhaseDetector, FreezeAddsNothingToAHold) {
    Flight f;
    f.toClimb().air(10, 11000, 0).freeze(51).air(1, 11000, 0);
    EXPECT_FALSE(f.saw("level:climb:11000"));
    f.air(10, 11000, 0);
    EXPECT_TRUE(f.saw("level:climb:11000"));
}

TEST(PhaseDetector, PushbackEndsWithoutABrakeSet) {
    Flight f;
    f.ground(5, 2).ground(1, 7);
    EXPECT_EQ(f.detector().phase(), Phase::TaxiOut);
}

TEST(PhaseDetector, PushbackEndsAfterTheBrakeSetAndAMove) {
    Flight f;
    f.ground(10, 3).ground(30, 0, true).ground(5, 0);
    EXPECT_EQ(f.detector().phase(), Phase::Pushback);
    f.ground(1, 3);
    EXPECT_EQ(f.detector().phase(), Phase::TaxiOut);
}

TEST(PhaseDetector, RollingTakeoffTimedFromTheAcceleration) {
    Flight f;
    f.ground(1, 7).ground(20, 12).ground(8, 17, false, 5);  // 12 kt steady, then 17, 22, ... 52
    ASSERT_EQ(f.detector().phase(), Phase::Takeoff);
    const auto& takeoff = f.seen().back();
    ASSERT_EQ(takeoff.key, "takeoff");
    EXPECT_EQ(takeoff.time, kT0 + 20s);  // the last 12 kt sample, just before the speed rose
}

TEST(PhaseDetector, StandingTakeoffTimedFromTheStop) {
    Flight f;
    f.ground(1, 7).ground(10, 10).ground(30, 0).ground(10, 5, false, 5);
    ASSERT_EQ(f.detector().phase(), Phase::Takeoff);
    EXPECT_EQ(f.seen().back().time, kT0 + 40s);  // the last 0 kt sample
}

TEST(PhaseDetector, FastTaxiIsNotATakeoff) {
    Flight f;
    f.ground(1, 7).ground(60, 30);
    EXPECT_EQ(f.detector().phase(), Phase::TaxiOut);
}
