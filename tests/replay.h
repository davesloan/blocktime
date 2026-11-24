// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reads the replay fixtures (tests/data/replay-*.csv): Dave's probe flights' telemetry, cut from the event log between
// block_out and block_in on 2026-11-23. Header ts,parking_brake,on_ground,gs_kt,alt_ind_ft,alt_agl_ft,vs_fpm,lat,lon,
// one sample per line, and "# block_out <time>" / "# block_in <time>" lines.

#pragma once

#include <chrono>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "active_clock.h"
#include "event_log.h"
#include "phase_detector.h"

namespace replay {

using namespace std::chrono;
using blocktime::ActiveClock;
using blocktime::formatUtc;
using blocktime::PhaseSample;
using blocktime::TimePoint;

inline blocktime::TimePoint parseTs(const std::string& text) {
    sys_time<milliseconds> t{};
    std::istringstream in(text);
    in >> parse("%Y-%m-%dT%H:%M:%S", t);
    return time_point_cast<TimePoint::duration>(t);
}

inline std::string hms(blocktime::TimePoint t) { return formatUtc(t).substr(11, 8); }

struct Replay {
    TimePoint blockOut{}, blockIn{};
    std::vector<PhaseSample> samples;
};

// Reads a replay fixture; activeS comes from an ActiveClock fed each sample's time, as the recorder's would be.
inline Replay loadReplay(const std::string& name) {
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

}  // namespace replay
