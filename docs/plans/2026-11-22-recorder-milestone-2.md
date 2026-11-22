# Recorder Milestone 2 (Block Out / Block In) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The recorder detects block out and block in from the parking brake (D-031, D-032) and logs `block_out`, `block_in` and `flight_interrupted` events with time, position and fuel.

**Architecture:** A new pure-logic `BlockDetector` (no SimConnect, no windows.h) takes one valid telemetry sample at a time and returns at most one block event. `main.cpp` feeds it every sample that passes the junk filter and writes what it returns to the `EventLog`. The pure code goes into a `blocktime-core` static library, so the exe and a new GoogleTest test exe both link it. State is in memory only. Persisting it and resuming after a restart is milestone 3.

**Tech Stack:** C++20, MSVC /W4, CMake presets + Ninja, vcpkg manifest (nlohmann-json, **gtest**, which Dave chose on 2026-11-22), SimConnect (MSFS 2020 SDK).

**Spec:** `docs/recorder.md` (Flight lifecycle), `docs/decisions.md` D-031, D-032, D-023, D-025, D-039; `docs/notes/msfs-observed-behaviour.md`.

## Context

Milestone 1 is done and merged: the recorder connects, logs sim state and logs telemetry once a second when values change. Milestone 2 makes it start and end flights. Dave has already decided both rules:
- **Block out:** the brake release that is followed by ≥ 10 m of movement with the brake still off. It is timed at the release.
- **Block in:** after a landing, a brake set that stays set until every engine is shut down. It is timed at the set.

All the inputs needed are already in `TelemetrySample` (brake, on ground, position, AGL, GS, fuel, `GENERAL ENG COMBUSTION:n`). No new SimVars are needed.

## Global Constraints

- Builds with no warnings at `/W4 /permissive- /utf-8`, in both release and debug.
- Every new source file starts with the GPL header used in `recorder/src/*.cpp` (`// BlockTime Recorder` / copyright / `SPDX-License-Identifier: GPL-3.0-or-later`).
- The core library and tests must not include `windows.h` or `SimConnect.h`.
- Log line format is unchanged (D-020): `{"ts","type","data"}`. `ts` is the write time. Block event times go in `data`.
- All times are real-world UTC (`system_clock`), formatted as `YYYY-MM-DDTHH:MM:SS.mmmZ`.
- Only the PMDG 737/777 on MSFS 2020 (D-039).
- Recorder version becomes **0.2.0** (`CMakeLists.txt` `project(... VERSION)` and `vcpkg.json`).
- Commits go straight to `main` (D-038). Messages explain why and cite D-numbers. They end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Detection rules (decided here; logged as Proposed in Task 6)

- **Release / set** = a change in `parkingBrake` between two consecutive valid samples. The first sample after the detector starts, or after `onSessionEnded`, only sets the baseline. It is never treated as a release.
- **Block-out candidate:** a release with no flight open, while `onGround`. It stores that sample: time, lat/lon, fuel lb/gal, ATC ID, title.
- **Confirmation:** a later sample with the brake off, on the ground, and `distanceMeters(candidate, sample) >= 10.0` → `BlockOut`, timed at the candidate's sample. A set before that drops the candidate.
- **Airborne:** after block out, a sample with `!onGround && altAglFt >= 100.0` sets `airborne`. The 100 ft is well above the 8–14 ft that PMDG reads on the ground, so a ground flicker during taxi isn't a flight.
- **Landed:** `airborne`, then a sample with `onGround`.
- **Block-in candidate:** after landing, a set while on the ground. It stores that sample. A release before all engines are off drops it. Speed isn't checked: a brake held through engine shutdown on the ground only happens when stopped, and a speed check could leave a flight stuck open.
- **Confirmation:** a sample with the brake still set and `enginesRunning == 0` → `BlockIn`. It carries the set's time/position/fuel and the shutdown time/fuel. If the engines are already off at the set, it fires on that same sample. The flight is then closed.
- **Session end** (sim disconnect, recorder exit) with a flight open → `FlightInterrupted{flightId, reason}`. The flight, the candidate and the brake baseline are cleared. This is a milestone 2 stopgap. Milestone 3 decides about persistence and resume (D-009).
- `flight_id` = `formatUtc(release time)`.
- `enginesRunning` = the number of true `engCombustion[i]` for `i < clamp(numEngines, 0, 4)`, the same as the logged `eng_running`.
- Times are the time the sample arrived, so they can be up to ~1 s late (PERIOD_SECOND). Sim `ZULU TIME` isn't read. The airline runs on real time (design.md), so this drops the remark in D-023.

## Review Focus

1. **Reconnect mid-flight:** a candidate or brake state from the old session must not create a block out in the new one. Pinned in Task 4.
2. **Ground flicker during taxi out:** a one-sample `on_ground=false` at 8 ft must not count as a landing. Pinned in Task 3.
3. **Hold-short set after landing:** a set that is released with engines running must not end the flight. Pinned in Task 3.
4. **Full GSX sequence** (profile set, release, set for GSX, release + push 40 m, set after push, release, taxi): gives exactly one block out, at the push release. Pinned in Task 2.
5. **Return to gate without flying:** brake set + shutdown with no landing gives no block in, and the flight stays open. Pinned in Task 3. This is accepted behaviour for now and noted in progress.md.

---

### Task 0: Save the plan in the repo

- [ ] Copy this plan to `docs/plans/2026-11-22-recorder-milestone-2.md` (CLAUDE.md asks for plans under `docs/plans/`). Commit: `Docs: plan for recorder milestone 2 (block out / block in)`.

### Task 1: Test scaffolding, `formatUtc`, `distanceMeters`

**Files:**
- Modify: `recorder/vcpkg.json` (add `"gtest"`), `recorder/CMakeLists.txt`, `recorder/CMakePresets.json`, `recorder/src/event_log.h/.cpp`
- Create: `recorder/src/geo.h`, `recorder/src/geo.cpp`, `recorder/tests/geo_test.cpp`, `recorder/tests/event_log_test.cpp`

**Interfaces (produces):**
- `std::string blocktime::formatUtc(std::chrono::system_clock::time_point t);` in `event_log.h`. `utcTimestamp()` becomes `formatUtc(system_clock::now())`.
- `double blocktime::distanceMeters(double lat1Deg, double lon1Deg, double lat2Deg, double lon2Deg);` in `geo.h`. Haversine, Earth radius 6,371,000 m.
- CMake target `blocktime-core` (STATIC: `event_log.cpp`, `telemetry_filter.cpp`, `geo.cpp`, later `block_detector.cpp`). It has PUBLIC nlohmann_json, the compile definitions and the warning options. `blocktime-recorder` links `blocktime-core` + SimConnect. `blocktime-tests` links `blocktime-core` + `GTest::gtest_main`, inside `enable_testing()`, `include(GoogleTest)` and `gtest_discover_tests(blocktime-tests)`. Add `/external:W0` so gtest headers don't break `/W4`.
- `CMakePresets.json`: `testPresets` `x64-debug` and `x64-release` (`"output": {"outputOnFailure": true}`).

- [ ] **Step 1: Write failing tests**
  - `FormatUtc.MillisecondsAndZ`: `sys_days{2026y/11/22} + 14h + 3min + 7s + 123ms` → `"2026-11-22T14:03:07.123Z"`.
  - `FormatUtc.WholeSecondHasZeroMillis`: → ends with `".000Z"`.
  - `Geo.ZeroForSamePoint`: `distanceMeters(32.7336,-117.1897,32.7336,-117.1897) == 0.0`.
  - `Geo.ThousandthDegreeLatitudeIsAbout111m`: `(32.733,-117.19)→(32.734,-117.19)` is `NEAR 111.2, 0.5`.
  - `Geo.LongitudeShrinksWithLatitude`: `(60.0,10.0)→(60.0,10.001)` is `NEAR 55.6, 0.5`.
- [ ] **Step 2:** Configure + build. Expect a compile/link failure (no `formatUtc`, no `geo.h`).
- [ ] **Step 3:** Implement the CMake/preset/vcpkg changes, `formatUtc`, and `distanceMeters`.
- [ ] **Step 4:** Build, then run `ctest --preset x64-release`. Expect all 5 to pass, and the release build to show no warnings.
- [ ] **Step 5:** Commit: `Recorder: add GoogleTest and a core library for testable logic (D-041)`.

### Task 2: `BlockDetector`: block out

**Files:**
- Create: `recorder/src/block_detector.h`, `recorder/src/block_detector.cpp`, `recorder/tests/block_detector_test.cpp`
- Modify: `recorder/CMakeLists.txt` (add `block_detector.cpp` to core, and the test file to tests)

**Interfaces (produces), in namespace `blocktime`:**
```cpp
using TimePoint = std::chrono::system_clock::time_point;
struct BlockSample { TimePoint time; bool parkingBrake; bool onGround; double latDeg, lonDeg, altAglFt,
                     groundSpeedKt, fuelLb, fuelGal; int enginesRunning; std::string atcId, title; };
BlockSample toBlockSample(const TelemetrySample& s, TimePoint time);
struct BlockOut { std::string flightId; TimePoint time; double latDeg, lonDeg, fuelLb, fuelGal, movedM;
                  std::string atcId, title; };
struct BlockIn { std::string flightId; TimePoint time; double latDeg, lonDeg, fuelLb, fuelGal;
                 TimePoint shutdownTime; double shutdownFuelLb; long long blockTimeS; };
struct FlightInterrupted { std::string flightId; std::string reason; };
using BlockEvent = std::variant<BlockOut, BlockIn, FlightInterrupted>;
inline constexpr double kBlockOutMinMoveM = 10.0;    // D-031
inline constexpr double kAirborneMinAglFt = 100.0;
class BlockDetector {
public:
    std::optional<BlockEvent> onSample(const BlockSample& s);
    std::optional<FlightInterrupted> onSessionEnded(std::string_view reason);  // body in Task 4
    bool flightOpen() const;
};
```
Test helpers live in the test file. `gate(t)` returns a sample at KSAN (32.7336, −117.1897): brake set, on ground, AGL 8, GS 0, fuel 9000 lb / 1343 gal, 2 engines running, `"N738DV"`. `north(s, m)` returns a copy with `latDeg += m / 111195.0`. Use `t0 = sys_days{2026y/11/22} + 14h`.

- [ ] **Step 1: Write failing tests**
  - `BlockOut.ReleaseWithoutMovementDoesNothing`: set → release → `north(9 m)` → no events; `!flightOpen()`.
  - `BlockOut.TimedAtReleaseWithReleaseFuelAndPosition`: set@t0, release@t0+5s (fuel 9000), `north(10 m)`@t0+40s (fuel 8990) → `BlockOut` with `time == t0+5s`, `fuelLb == 9000`, lat/lon == release sample, `movedM >= 10`, `flightId == "2026-11-22T14:00:05.000Z"`, `atcId == "N738DV"`; `flightOpen()`.
  - `BlockOut.GsxSequenceGivesOneBlockOutAtPushRelease` (Review Focus 4): set@t0, release@+10s, set@+20s, release@+60s, `north(40 m)`@+120s, set@+180s, release@+240s, `north(500 m)`@+300s → exactly one event, a `BlockOut` with `time == t0+60s`.
  - `BlockOut.SetBeforeTenMetersDropsCandidate`: release@+5s, `north(5 m)`, set, `north(15 m)` (brake set) → no event.
  - `BlockOut.FirstSampleWithBrakeOffIsNotARelease`: first sample brake off, then `north(100 m)` → no event.
  - `BlockOut.ReleaseWhileAirborneIsIgnored`: set (on ground false, AGL 3000), release, move 1 km → no event.
- [ ] **Step 2:** Build and run tests. Expect a compile failure, then FAIL.
- [ ] **Step 3:** Implement `toBlockSample`, `onSample` (block-out half of the rules above) and `flightOpen`. `onSessionEnded` returns `std::nullopt` for now.
- [ ] **Step 4:** Run `ctest --preset x64-release`. Expect all to pass.
- [ ] **Step 5:** Commit: `Recorder: detect block out from brake release plus 10 m (D-031)`.

### Task 3: `BlockDetector`: landing and block in

**Files:** Modify `recorder/src/block_detector.cpp` and `recorder/tests/block_detector_test.cpp`.

Add a test helper `flyTo(det, t)`: block out, then an airborne sample (on ground false, AGL 3000), then a landed sample (on ground, brake off, engines 2), with the times advancing.

- [ ] **Step 1: Write failing tests**
  - `BlockIn.TimedAtSetWithShutdownFuel`: after landing, set@T (fuel 5000), engines 1@T+60s, engines 0@T+90s (fuel 4990) → `BlockIn` with `time == T`, `fuelLb == 5000`, `shutdownTime == T+90s`, `shutdownFuelLb == 4990`, `blockTimeS == (T − releaseTime)` in seconds, same `flightId` as the block out; `!flightOpen()`.
  - `BlockIn.SetReleasedWithEngineRunningIsIgnored` (RF 3): set@T1, release@T1+60s, taxi `north(300 m)`, set@T2, engines 0 → one `BlockIn` with `time == T2`.
  - `BlockIn.EnginesAlreadyOffAtSet`: after landing, engines 0 with the brake off, then set@T → `BlockIn` on that sample with `shutdownTime == T`.
  - `BlockIn.GroundFlickerIsNotALanding` (RF 2): block out, one sample `onGround=false, AGL 8`, back on ground, set, engines 0 → no `BlockIn`; `flightOpen()`.
  - `BlockIn.ReturnToGateWithoutFlyingStaysOpen` (RF 5): block out, set, engines 0 → no event; `flightOpen()`.
  - `BlockIn.NewFlightAfterBlockIn`: after a block in, a release + 10 m → a second `BlockOut` with a new `flightId`.
- [ ] **Step 2:** Run tests. Expect FAIL.
- [ ] **Step 3:** Implement airborne/landed tracking and the block-in candidate and confirmation, per the rules above.
- [ ] **Step 4:** Run tests. Expect all to pass.
- [ ] **Step 5:** Commit: `Recorder: detect block in from brake set held through engine shutdown (D-032)`.

### Task 4: `BlockDetector`: session end

**Files:** Modify `recorder/src/block_detector.cpp` and `recorder/tests/block_detector_test.cpp`.

- [ ] **Step 1: Write failing tests**
  - `Session.EndWithOpenFlightReportsInterrupted`: block out, then `onSessionEnded("sim_quit")` → `FlightInterrupted{flightId, "sim_quit"}`; `!flightOpen()`.
  - `Session.EndWithoutFlightReportsNothing` → `std::nullopt`.
  - `Session.CandidateAndBrakeStateDoNotSurvive` (RF 1): set, release (candidate), `onSessionEnded("sim_process_gone")`, then a first sample with the brake off at `north(50 m)`, then `north(100 m)` → no event.
- [ ] **Step 2:** Run tests. Expect FAIL.
- [ ] **Step 3:** Implement `onSessionEnded`: return an interrupted event if a flight is open, then reset all state.
- [ ] **Step 4:** Run tests. Expect all to pass.
- [ ] **Step 5:** Commit: `Recorder: report flights cut short by a lost sim session (D-044)`.

### Task 5: Event JSON and wiring into `main.cpp`

**Files:** Modify `recorder/src/block_detector.h/.cpp`, `recorder/src/main.cpp` and `recorder/tests/block_detector_test.cpp`.

**Interfaces (produces):**
- `std::string_view eventType(const BlockEvent& e);` returns `"block_out"`, `"block_in"` or `"flight_interrupted"`.
- `nlohmann::json toJson(const BlockEvent& e);` with these keys:
  - `block_out`: `flight_id, time, lat, lon, fuel_lb, fuel_gal, moved_m, atc_id, title`
  - `block_in`: `flight_id, time, lat, lon, fuel_lb, fuel_gal, shutdown_time, shutdown_fuel_lb, block_time_s`
  - `flight_interrupted`: `flight_id, reason`
  - Times use `formatUtc`.

- [ ] **Step 1: Write failing tests**
  - `Json.BlockOutFields`: has exactly the keys above; `time == "2026-11-22T14:00:05.000Z"`; `eventType == "block_out"`.
  - `Json.BlockInFields`: has exactly the keys above, with `block_time_s` an integer.
  - `Json.InterruptedFields`: `{"flight_id","reason"}`.
- [ ] **Step 2:** Run tests. Expect FAIL.
- [ ] **Step 3:** Implement `eventType` and `toJson`.
- [ ] **Step 4:** Wire it into `main.cpp`:
  - `TelemetryLogger::onSample` returns `bool` (whether the sample was valid).
  - The `SimConnection` lambda: if the sample is valid, call `detector.onSample(toBlockSample(s, system_clock::now()))`. Write any event with `log.write(eventType(e), toJson(e))` and print a one-line console summary (e.g. `14:00:41  BLOCK OUT  N738DV  at 14:00:05  fuel 9000 lb`).
  - After `sim.close(...)` in the `!alive` branch, and before `recorder_stopped` at exit (reason `"recorder_exit"`): call `detector.onSessionEnded(reason)` and log the result.
  - Update the header comment to milestone 2.
- [ ] **Step 5:** Build release + debug with no warnings. All tests pass. Run the exe with MSFS closed: it starts, Ctrl+C exits with code 0, and the log has no block events.
- [ ] **Step 6:** Commit: `Recorder: log block_out, block_in and flight_interrupted events`.

### Task 6: Docs, version, review, final commit

**Files:** `recorder/CMakeLists.txt`, `recorder/vcpkg.json` (0.2.0), `recorder/README.md`, `docs/recorder.md`, `docs/decisions.md`, `docs/progress.md`.

- [ ] Bump the version to 0.2.0.
- [ ] `README.md`:
  - Status → milestone 2.
  - Add rows for `block_out`, `block_in` and `flight_interrupted`, with their fields.
  - Add a **Test** section: `cmake --build --preset x64-release`, then `ctest --preset x64-release`.
  - Note that the first configure now also builds gtest.
- [ ] `docs/recorder.md`:
  - Mark milestone 2 done.
  - Document the detection rules (airborne ≥ 100 ft AGL, transition-only releases, interrupted on session end) and the event fields.
- [ ] `docs/decisions.md`:
  - **D-041** GoogleTest + core library (Dave chose GoogleTest; Accepted).
  - **D-042** Detection details: transitions only, airborne ≥ 100 ft AGL, no separate speed check, real UTC at sample arrival, ZULU not read (Proposed).
  - **D-043** Block event fields: `flight_id` = release time, fuel at set and at shutdown (Proposed).
  - **D-044** Session end → `flight_interrupted` and the flight is dropped, until milestone 3 (Proposed).
- [ ] Run `superpowers:requesting-code-review` on the milestone diff, then `superpowers:receiving-code-review`. Fix the findings that are accepted, re-run the tests, and record any that are deferred.
- [ ] `superpowers:verification-before-completion`: a clean configure + build of release and debug with no warnings, and `ctest` all green.
- [ ] `docs/progress.md` entry: what was done, the unverified items, and the sim checklist below. Commit: `Recorder 0.2.0: milestone 2, block out and block in (D-041..D-044)`.

## Verification

**Without the sim (Claude):** from a VS dev shell in `recorder/`:
1. `cmake --preset x64-release`
2. `cmake --build --preset x64-release`
3. `ctest --preset x64-release`

Expect no warnings and all tests passing. Do the same with `x64-debug`. Then a smoke run with MSFS closed.

**In the sim (Dave), to go in progress.md.** Fly a pattern at KSAN in the PMDG 737, and repeat the shutdown on the 777:
1. Profile set/release, GSX set, release for the push, set after the push, release to taxi. Expect exactly **one** `block_out`, timed at the push release, with `fuel_lb` matching the fuel at that moment.
2. Set and release the brake at hold short before takeoff. Expect no new events.
3. After landing, set and release at a taxiway stop. Expect no `block_in`.
4. Set at the gate, then shut down both engines. Expect one `block_in`, timed at the set, with `shutdown_time` when the last engine stopped and `block_time_s` = in − out.
5. On the 777: engine shutdown makes `eng_running` all false and produces `block_in` (not tested before).
6. Optional: quit MSFS after block out. Expect `sim_disconnected` and then `flight_interrupted` with reason `sim_quit`.
