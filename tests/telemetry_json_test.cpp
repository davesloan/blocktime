// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Telemetry and block events read back from their logged form, for the replay (milestone 11, docs/replay.md).

#include <chrono>
#include <cstring>

#include <gtest/gtest.h>

#include "event_log.h"
#include "telemetry_filter.h"
#include "telemetry_json.h"

using namespace std::chrono;
using namespace blocktime;

namespace {

TimePoint at(const char* iso) { return *parseUtc(iso); }

}  // namespace

TEST(TelemetryJson, RoundTrip) {
    TelemetrySample s{};
    s.parkingBrake = 1;
    s.onGround = 1;
    s.fuelTotalLb = 15316.5;
    s.fuelTotalGal = 2286.0;
    s.latitudeDeg = 32.7336;
    s.longitudeDeg = -117.1897;
    s.altitudeFt = 17.2;
    s.altitudeAglFt = 8.1;
    s.indicatedAltitudeFt = 20.4;
    s.verticalSpeedFpm = -1.5;
    s.groundSpeedKt = 0.4;
    s.indicatedAirspeedKt = 0.2;
    s.headingTrueDeg = 271.3;
    s.numEngines = 2;
    s.engCombustion[0] = 1;
    s.engCombustion[1] = 0;
    s.lightBeacon = 1;
    s.lightStrobe = 0;
    s.mach = 0.78;
    s.flapsTrailingPct = 40.0;
    s.flapsHandleIndex = 3;
    std::memcpy(s.title, "PMDG 737-800 BBJ2", 18);
    std::memcpy(s.atcId, "N814SX", 7);

    const TelemetrySample r = telemetryFromJson(toJson(s));
    EXPECT_EQ(r.parkingBrake, 1);
    EXPECT_EQ(r.onGround, 1);
    EXPECT_DOUBLE_EQ(r.fuelTotalLb, 15316.5);
    EXPECT_DOUBLE_EQ(r.fuelTotalGal, 2286.0);
    EXPECT_DOUBLE_EQ(r.latitudeDeg, 32.7336);
    EXPECT_DOUBLE_EQ(r.longitudeDeg, -117.1897);
    EXPECT_DOUBLE_EQ(r.altitudeFt, 17.2);
    EXPECT_DOUBLE_EQ(r.altitudeAglFt, 8.1);
    EXPECT_DOUBLE_EQ(r.indicatedAltitudeFt, 20.4);
    EXPECT_DOUBLE_EQ(r.verticalSpeedFpm, -1.5);
    EXPECT_DOUBLE_EQ(r.groundSpeedKt, 0.4);
    EXPECT_DOUBLE_EQ(r.indicatedAirspeedKt, 0.2);
    EXPECT_DOUBLE_EQ(r.headingTrueDeg, 271.3);
    EXPECT_EQ(r.numEngines, 2);
    EXPECT_EQ(r.engCombustion[0], 1);
    EXPECT_EQ(r.engCombustion[1], 0);
    EXPECT_EQ(r.lightBeacon, 1);
    EXPECT_EQ(r.lightStrobe, 0);
    EXPECT_DOUBLE_EQ(r.mach, 0.78);
    EXPECT_DOUBLE_EQ(r.flapsTrailingPct, 40.0);
    EXPECT_EQ(r.flapsHandleIndex, 3);
    EXPECT_EQ(simString(r.title, sizeof(r.title)), "PMDG 737-800 BBJ2");
    EXPECT_EQ(simString(r.atcId, sizeof(r.atcId)), "N814SX");
}

TEST(TelemetryJson, MissingFieldsAreZero) {
    const TelemetrySample r = telemetryFromJson({{"alt_ind_ft", 3454.5}});
    EXPECT_DOUBLE_EQ(r.indicatedAltitudeFt, 3454.5);
    EXPECT_DOUBLE_EQ(r.mach, 0);
    EXPECT_DOUBLE_EQ(r.fuelTotalLb, 0);
    EXPECT_EQ(r.numEngines, 0);
    EXPECT_EQ(simString(r.title, sizeof(r.title)), "");
}

TEST(TelemetryJson, BlockEventsRoundTrip) {
    BlockOut out;
    out.flightId = "2026-11-25T18:43:04.053Z";
    out.time = at("2026-11-25T18:43:04.053Z");
    out.latDeg = 32.73;
    out.lonDeg = -117.19;
    out.fuelLb = 15316;
    out.fuelGal = 2286;
    out.movedM = 11.5;
    out.atcId = "N814SX";
    out.title = "PMDG 737-800 BBJ2";
    out.plannedFlightId = 7;
    out.aircraftFolder = "pmdg 737-800bbj2";
    out.parking = "Gate 23";
    const auto o = blockEventFromJson("block_out", toJson(BlockEvent{out}));
    ASSERT_TRUE(o && std::holds_alternative<BlockOut>(*o));
    const BlockOut& ro = std::get<BlockOut>(*o);
    EXPECT_EQ(ro.flightId, out.flightId);
    EXPECT_EQ(ro.time, out.time);
    EXPECT_DOUBLE_EQ(ro.latDeg, out.latDeg);
    EXPECT_DOUBLE_EQ(ro.lonDeg, out.lonDeg);
    EXPECT_DOUBLE_EQ(ro.fuelLb, out.fuelLb);
    EXPECT_DOUBLE_EQ(ro.fuelGal, out.fuelGal);
    EXPECT_DOUBLE_EQ(ro.movedM, out.movedM);
    EXPECT_EQ(ro.atcId, out.atcId);
    EXPECT_EQ(ro.title, out.title);
    EXPECT_EQ(ro.plannedFlightId, out.plannedFlightId);
    EXPECT_EQ(ro.aircraftFolder, out.aircraftFolder);
    EXPECT_EQ(ro.parking, out.parking);

    BlockIn in;
    in.flightId = out.flightId;
    in.time = at("2026-11-25T20:01:45.912Z");
    in.latDeg = 37.36;
    in.lonDeg = -121.93;
    in.fuelLb = 9876;
    in.fuelGal = 1474;
    in.shutdownTime = at("2026-11-25T20:02:30.000Z");
    in.shutdownFuelLb = 9850;
    in.blockTimeS = 4718;
    in.parking = "Gate 15";
    const auto i = blockEventFromJson("block_in", toJson(BlockEvent{in}));
    ASSERT_TRUE(i && std::holds_alternative<BlockIn>(*i));
    const BlockIn& ri = std::get<BlockIn>(*i);
    EXPECT_EQ(ri.flightId, in.flightId);
    EXPECT_EQ(ri.time, in.time);
    EXPECT_DOUBLE_EQ(ri.latDeg, in.latDeg);
    EXPECT_DOUBLE_EQ(ri.lonDeg, in.lonDeg);
    EXPECT_DOUBLE_EQ(ri.fuelLb, in.fuelLb);
    EXPECT_DOUBLE_EQ(ri.fuelGal, in.fuelGal);
    EXPECT_EQ(ri.shutdownTime, in.shutdownTime);
    EXPECT_DOUBLE_EQ(ri.shutdownFuelLb, in.shutdownFuelLb);
    EXPECT_EQ(ri.blockTimeS, in.blockTimeS);
    EXPECT_EQ(ri.parking, in.parking);

    EXPECT_FALSE(blockEventFromJson("block_out_refused", {{"time", "2026-11-25T18:43:04.053Z"}}));
    EXPECT_FALSE(blockEventFromJson("block_out", {{"flight_id", "x"}, {"time", "not a time"}}));
}
