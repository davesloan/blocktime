# Milestone 7 (Phases and the Six Stages) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** During a recorded flight, Live shows where Dave is: the recorder's phases grouped into design.md §13.5's six
stages, each with its sub-events (level-offs, altitude and transition markers, 200 nm out, the runways flown, the
touchdown rate). Every flight also starts collecting what milestone 10's analysis needs: the SayIntentions ATC log,
the origin and destination ATIS, and the altimeter settings (D-123).

**Architecture:**
- **Plain C++ in `blocktime-core`, unit-tested:**
  - `ActiveClock` (D-058) and `PhaseDetector` (the rules in recorder.md, numbers in D-124), tested on two of Dave's
    real flights replayed from fixtures;
  - `runway_match.h` names a runway from a ground roll (D-126) and `TouchdownCapture` reads the touchdown (D-125);
  - `FlightTracker` is the glue the recorder calls: it owns one flight's phase detector, runways, roll samples,
    touchdown arming and altimeter changes, and turns them into log events;
  - SayIntentions parsing and the ATC de-duplication (D-127), and `FlightStages`, which builds the six stages from
    the log events (D-128).
- **The recorder** feeds `FlightTracker` from block out to block in and writes what it returns to the event log, then
  tells the observer. SimConnection gains three requests: the runways (facility data, now in the normal build), a
  fast touchdown definition that runs only near the ground, and the altimeter settings.
- **The app's web results go into the same log** through an `AppEventQueue` the recorder thread drains (D-127): the
  log has one writer.
- **Qt:** `FlightWatcher` polls SayIntentions on a timer driven by the phase. `RecorderBridge` holds a
  `FlightStages` and exposes `stages` and `phase` to QML. Live gets a Flight progress card.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, yaml-cpp,
gtest), Qt 6 Quick + Network + Location, SimConnect (MSFS 2020 SDK).

**Spec:**
- `docs/roadmap.md`, milestone 7, is the scope.
- `docs/recorder.md`: "Phases" (the rules), "ATC log, ATIS and altimeter", "Parking spots and runways", "Sampling
  strategy", "Output: local event log".
- `docs/design.md` §13.5 (stages and sub-events) and §14 ("Flight analysis": never scored, D-122).
- `docs/notes/probe-flight.md`, results item 4 (the runway method) and `docs/notes/sayintentions-api.md`.
- Decisions: D-058 (freezes), D-122 (naming, never scoring), D-123 (what to log, fetch timing, dedupe);
  **D-124..D-128, written with this plan** (all Proposed).

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`, release and debug. All tests pass. `blocktime-app_qmllint` reports nothing.
- **Files:**
  - every new source and QML file starts with the GPL header;
  - QML files `import BlockTime`, use `pragma ComponentBehavior: Bound`, and take every colour, size and font from
    `Theme.qml`;
  - `blocktime-core` and its tests include no Qt, `windows.h` or `SimConnect.h`.
- **Recorder budget:** the main telemetry definition gains only `PLANE HEADING DEGREES TRUE` (D-129, logged and not
  used yet). The new requests are each their own definition, so a SimVar name the sim refuses drops only that request:
  - runways: one facility request per airport, at connect and when the planned flight changes;
  - touchdown: `SIMCONNECT_PERIOD_SIM_FRAME` only while armed (below 300 ft AGL on the way down), `PERIOD_NEVER`
    otherwise;
  - altimeter: `PERIOD_SECOND` + `CHANGED`.
- **Phase numbers (D-124), as constants in `phase_detector.h`:**
  - pushback ends: brake set after block out, then ≥ **3 kt** with the brake off; or ≥ **6 kt** at any time;
  - takeoff roll: confirmed on the ground at ≥ **40 kt**, but timed from where the acceleration began: walk back from
    the first 40 kt sample while each earlier sample is at least **0.5 kt** slower than the next (the sim has no
    takeoff-roll signal, and Dave's rolling takeoffs reached 40 kt 7–8 s after the roll began);
  - climb: airborne at ≥ **1,000 ft AGL**;
  - level: indicated altitude within **±100 ft** of the hold's first sample for **20 s** of active time; a level ends
    when the altitude is more than **200 ft** from the level;
  - cruise: a level within **1,000 ft** of the plan's cruise altitude; without a plan, a level above **10,000 ft**
    held **300 s**;
  - top of descent: VS ≤ **−500 fpm** for **60 s**, and within **2 × 3 nm per 1,000 ft** of the destination
    (great circle);
  - approach: within **30 nm** of the destination and below **10,000 ft**, or below **3,000 ft AGL**;
  - go-around: VS ≥ **+500 fpm** for **10 s** and **200 ft** above the approach's lowest altitude;
  - landing: first sample on the ground after being airborne; taxi in: on the ground below **30 kt**;
  - airborne means not on the ground and ≥ 100 ft AGL (`kAirborneMinAglFt`, D-042).
- **Altitudes are indicated** (`INDICATED ALTITUDE`, the pilot's own reference) for phases, levels and markers.
- **Holds use `ActiveClock` seconds** (D-058): each sim message adds the time since the previous one, capped at **2 s**.
- **Runways (D-126):** the roll is the samples on the ground at ≥ 40 kt; a runway end fits when the roll's track is
  within **20°** of its heading and the roll's mean distance from its centreline is ≤ **75 m**; the nearest fit wins.
  Never scored (D-122): the offsets are logged as information.
- **ATC and ATIS (D-123):** ATC every **60 s** while a flight is open, plus once at block in. ATIS every **10 min**:
  the origin's from block out to liftoff, the destination's from top of descent to touchdown. A failed call is
  dropped and tried again at the next poll; nothing waits on it.
- **Times** in log `data` are `formatUtc` instants; Live shows `HH:MMZ`.
- **Version:** **0.8.0** (`CMakeLists.txt`, `vcpkg.json`).
- **Commits:** straight to `main`, why not what, D-numbers, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Don't push until the milestone closes (D-121).
- **Commands:**
  - build `cmake --build --preset x64-release`;
  - tests `ctest --preset x64-release`;
  - one suite `build\x64-release\blocktime-tests.exe --gtest_filter=<Suite>.*`, and the Qt suites in
    `blocktime-qt-tests.exe`.

## Review Focus

1. **A sim freeze during a level-off or descent** (D-058, Dave's 51 s KSAN freeze). Expected: the freeze adds at most
   2 s to any hold, so no level, cruise or top of descent is confirmed by it. Test: Task 1
   `PhaseDetector.FreezeAddsNothingToAHold`.
2. **SayIntentions isn't running, or a call fails** (no active SI flight, network down, bad key). Expected: nothing is
   logged for that poll, the flight records normally, and the next poll tries again. Test: Task 8
   `FlightWatcher.FailedCallsLogNothingAndRetry`.
3. **A late ATC reply**: an exchange first fetched with no ATC text, then fetched again with it. Expected: it's logged
   again with the reply, and identical repeats never are. Test: Task 7 `CommsDedupe.RelogsAnExchangeThatChanged`.
4. **A flight interrupted mid-air, then flown again.** Expected: the next attempt starts at Pushback with fresh
   stages, markers and touchdown, and fast sampling is off. Tests: Task 5 `FlightTracker.InterruptForgetsTheFlight`,
   Task 10 `RecorderBridge.StagesResetAtBlockOut`.
5. **No runway data for where the roll happened** (a diversion, or facility data that never came). Expected: the
   runway is `""`, the stage line reads "Departed" / "Landed, −142 fpm" without a runway, and nothing else changes.
   Tests: Task 3 `RunwayMatch.NoRunwaysGiveNoName`, Task 9 `FlightStages.LandedWithoutARunway`.

---

### Task 1: ActiveClock and the PhaseDetector's phases and levels

**Files:**
- Create: `src/active_clock.h`, `src/phase_detector.h`, `src/phase_detector.cpp`
- Create: `tests/phase_detector_test.cpp`; fixtures `tests/data/replay-kbfi-kpdx.csv`, `tests/data/replay-kpdx-ksfo.csv`
  (already committed with this plan)
- Modify: `CMakeLists.txt` (core sources, test file)

The fixtures are Dave's KBFI→KPDX and KPDX→KSFO flights of 2026-11-23, cut from the event log between `block_out`
and `block_in`: header `ts,parking_brake,on_ground,gs_kt,alt_ind_ft,alt_agl_ft,vs_fpm,lat,lon`, one telemetry
sample per line, and two comment lines `# block_out <time>` and `# block_in <time>`.

**Interfaces:**
- Produces:
  ```cpp
  class ActiveClock {  // D-058: silence from the sim adds at most kMaxGap
  public:
      static constexpr std::chrono::milliseconds kMaxGap{2000};
      void onMessage(std::chrono::steady_clock::time_point t);
      double seconds() const;
  };
  enum class Phase { AtGate, Pushback, TaxiOut, Takeoff, Climb, Cruise, Descent, Approach, GoAround, Landing,
                     TaxiIn, Arrived };
  std::string_view phaseCode(Phase);  // "at_gate" … "go_around" … "arrived"
  struct PhasePlan { int cruiseAltitudeFt = 0, transitionAltitudeFt = 0, transitionLevelFt = 0;
                     bool hasDestination = false; double destinationLat = 0, destinationLon = 0; };
  struct PhaseSample { TimePoint time{}; double activeS = 0; bool parkingBrake = false, onGround = false;
                       double altFt = 0 /* indicated */, aglFt = 0, vsFpm = 0, groundSpeedKt = 0, lat = 0, lon = 0; };
  struct PhaseChanged { Phase phase, previous; TimePoint time; int altFt; };
  struct LevelOff { Phase phase; TimePoint time; int altFt; };       // altFt: window mean, rounded to 100
  struct LevelOffEnded { Phase phase; TimePoint time; int altFt; };  // altFt: mean of the whole level, rounded
  struct StepClimb { TimePoint time; int fromFt; };                  // cruise level left (up, or down out of TOD range)
  struct StepClimbEnded { TimePoint time; int altFt; };
  // Task 2 adds Marker to this variant.
  using PhaseEvent = std::variant<PhaseChanged, LevelOff, LevelOffEnded, StepClimb, StepClimbEnded>;
  class PhaseDetector {
  public:
      PhaseDetector(PhasePlan plan, TimePoint blockOut, double altFt);  // starts in Pushback
      PhaseChanged started() const;                                     // AtGate → Pushback at block out
      std::vector<PhaseEvent> onSample(const PhaseSample& s);
      PhaseChanged onBlockIn(TimePoint t, double altFt);                // → Arrived
      Phase phase() const;
      std::optional<int> levelFt() const;                               // set while level in climb/descent/approach
      bool stepClimbing() const;
  };
  ```

- [ ] **Step 1: Write the failing tests.** A helper `replay(name, plan)` reads a fixture into `PhaseSample`s (the
  `activeS` from an `ActiveClock` fed each `ts`) and returns the events with times as `HH:MM:SS`. Expected events
  (from the prototype run on these fixtures while writing the plan); compare each time to within **±3 s**:

  ```cpp
  TEST(PhaseDetector, ReplaysKbfiKpdx) {
      // plan: cruise 20000, TA 18000, TL 18000, destination KPDX 45.5887, -122.5975
      // phases: pushback 22:51:08, taxi_out 22:54:17, takeoff 22:56:08 (40 kt at 22:56:16), climb 22:56:51, cruise 23:03:10 (20000),
      //         descent 23:04:32, approach 23:12:34, landing 23:27:16, taxi_in 23:27:40
      // levels: climb 2000 @22:57:15 (ended 2000 @22:57:57), climb 11000 @23:00:05 (ended 11000 @23:00:31),
      //         descent 10000 @23:11:40, approach 5000 @23:17:38, approach 2000 @23:23:55
      // no step climbs
  }
  TEST(PhaseDetector, ReplaysKpdxKsfo) {
      // plan: cruise 39000, TA 18000, TL 18000, destination KSFO 37.6190, -122.3750
      // phases: pushback 23:54:32, taxi_out 23:57:43, takeoff 00:03:13 (40 kt at 00:03:21), climb 00:04:06, cruise 00:23:12 (39000),
      //         descent 00:56:25, approach 01:18:19, landing 01:30:29, taxi_in 01:30:43
      // levels: descent 11000 @01:16:28, approach 6000 @01:20:35, approach 4000 @01:23:15,
      //         approach 3100 @01:25:02 ended 3000 @01:26:25 (the window mean vs the whole level's mean)
      // no climb level-offs, no step climbs
  }
  ```
  Plus short synthetic tests, each building a few samples:
  - `ForwardOnly`: after Cruise, a level-off and a climb never bring back Climb;
  - `LevelBelowCruiseStaysClimb`: level 30 s at 9,000 ft with cruise 35,000 gives `LevelOff{Climb, 9000}`, phase
    stays Climb (the KOAK example);
  - `NoPlanCruiseAfterFiveMinutes`: cruise altitude 0, level at 24,000 ft: still Climb after 299 s, Cruise at 300 s;
  - `DescentFromClimb`: never level near cruise, then VS −1,500 for 60 s: Descent;
  - `StepClimbInCruise`: cruise at 37,000, climb to 39,000: `StepClimb{from 37000}`, then `StepClimbEnded{39000}`;
  - `StepDescentFarOut`: cruise at 37,000, 600 nm out, VS −1,000 for 60 s: `StepClimb` (down), still Cruise;
  - `GoAroundAndBack`: in Approach at 600 ft, VS +1,500 for 10 s: GoAround at the hold's start; level 20 s at
    3,000 ft: Approach again;
  - `FreezeAddsNothingToAHold`: level at 11,000 ft for 10 s of active time, then one sample 51 s later whose
    `activeS` is only 2 s on: no `LevelOff`;
  - `PushbackEndsWithoutABrakeSet`: no brake set, ground speed 7 kt: TaxiOut;
  - `RollingTakeoffTimedFromTheAcceleration`: taxi steady at 12 kt, then +5 kt a second: Takeoff is reported on the
    first sample at 40 kt, with the time of the 12 kt sample just before the speed started rising;
  - `StandingTakeoffTimedFromTheStop`: stopped at 0 kt (lined up), then accelerating: the time is the last 0 kt
    sample;
  - `FastTaxiIsNotATakeoff`: 30 kt on the ground for a minute: still TaxiOut.
- [ ] **Step 2: Run `--gtest_filter=PhaseDetector.*`.** Expected: fails to compile (no `phase_detector.h`).
- [ ] **Step 3: Implement `ActiveClock` (header-only) and `PhaseDetector`** with the D-124 numbers as `inline constexpr`
  constants. The level window resets whenever a sample is more than 100 ft from the window's first sample; a level is
  confirmed once `activeS − windowStartActiveS ≥ 20`, and its time is the window's first sample. In TaxiOut, keep
  the last 60 s of samples for the takeoff walk-back. In Cruise, leaving
  the level upward is a `StepClimb` at once; leaving it downward waits for the descent hold, which is top of descent
  inside the range and a `StepClimb` (its `fromFt` above the new level) outside it.
- [ ] **Step 4: Run the suite.** Expected: all pass. If a replay time is off by more than 3 s, find which rule
  differs from the Global Constraints rather than widening the tolerance.
- [ ] **Step 5: Commit** ("The recorder's phase rules, tuned on two real flights (D-124, D-058)").

### Task 2: Markers

**Files:** Modify `src/phase_detector.h/.cpp`, `tests/phase_detector_test.cpp`.

**Interfaces:**
- Produces:
  ```cpp
  enum class MarkerKind { Above10000, TransitionAltitude, Within200Nm, TransitionLevel, Below10000, FinalApproach };
  std::string_view markerCode(MarkerKind);  // "above_10000", "transition_altitude", "within_200nm",
                                            // "transition_level", "below_10000", "final_approach"
  struct Marker { MarkerKind kind; TimePoint time; int altFt; };
  using PhaseEvent = std::variant<PhaseChanged, LevelOff, LevelOffEnded, StepClimb, StepClimbEnded, Marker>;
  ```
- Each marker fires once per flight, on the sample that crosses (the previous sample on the other side):
  above 10,000 ft and the transition altitude going up in Takeoff/Climb/Cruise; the transition level and below
  10,000 ft going down in Cruise/Descent/Approach; final approach crossing 1,000 ft AGL down in Approach; 200 nm from
  the destination crossing inward while airborne (never at all on a flight that starts inside 200 nm). Transition
  markers are skipped when the plan's value is 0.

- [ ] **Step 1: Failing tests.** Extend both replay tests (±3 s):
  - KBFI→KPDX: above_10000 22:59:34, transition_altitude 23:02:20, transition_level 23:06:33, below_10000 23:11:41,
    final_approach 23:25:36; no within_200nm (a 114 nm flight);
  - KPDX→KSFO: above_10000 00:06:48, transition_altitude 00:10:02, within_200nm 00:48:47, transition_level
    01:12:14, below_10000 01:18:19, final_approach 01:28:59.

  And `MarkersFireOnce` (climb through 10,000, dip below, climb again: one marker), `NoTransitionMarkersWithoutAPlan`.
- [ ] **Step 2: Run.** Expected: compile failure on `Marker`.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the suite.** Expected: all pass.
- [ ] **Step 5: Commit** ("Altitude, transition and 200 nm markers (design.md §13.5, D-124)").

### Task 3: Naming the runway

**Files:**
- Create: `src/runway_match.h`, `src/runway_match.cpp`, `tests/runway_match_test.cpp`; fixtures
  `tests/data/runways-KBFI.json`, `runways-KPDX.json`, `runways-KSFO.json` (committed with this plan, in the
  `airport_runways` format below, from the probe's facility data)
- Modify: `src/geo.h/.cpp` if a bearing helper is needed; `CMakeLists.txt`

**Interfaces:**
- Produces:
  ```cpp
  struct Runway { double lat = 0, lon = 0, headingDeg = 0 /* true, the primary end's */, lengthM = 0, widthM = 0;
                  int primaryNumber = 0, primaryDesignator = 0, secondaryNumber = 0, secondaryDesignator = 0; };
  struct AirportRunways { std::string icao; std::vector<Runway> runways; };
  struct RollPoint { double lat = 0, lon = 0; };
  struct RunwayName { std::string icao, runway /* "14R"; "" = none */; double offCentrelineM = 0, offHeadingDeg = 0; };
  // designator 0 none, 1 L, 2 R, 3 C (probe-flight.md); the number zero-padded to 2 digits
  std::string runwayEndName(int number, int designator);
  RunwayName nameRunway(const std::vector<AirportRunways>& airports, const std::vector<RollPoint>& roll);
  nlohmann::json toJson(const AirportRunways&);   // {"icao","runways":[{lat,lon,heading,length_m,width_m,
                                                  //   primary_number,primary_designator,secondary_number,
                                                  //   secondary_designator}]}
  AirportRunways airportRunwaysFromJson(const nlohmann::json&);
  ```
- The roll's track is the bearing from its first to its last point; fewer than 2 points, or less than 300 m between
  them, gives no name. Each runway gives two ends (primary at `headingDeg`, secondary at +180°). The centreline
  offset of a point uses a flat east/north projection around the runway's centre.

- [ ] **Step 1: Failing tests.** Roll points come from the replay fixtures (on the ground, ≥ 40 kt, before the first
  airborne sample for a takeoff, after the last one for a landing):
  - `NamesTheProbeFlightsRunways`: KBFI takeoff → `{"KBFI","14R"}`, KPDX landing → `10R`, KPDX takeoff → `10R`, KSFO
    landing → `28R`, each with `offCentrelineM < 10` and `offHeadingDeg < 2`, given all three airports' runways;
  - `DirectionPicksTheEnd`: the KBFI roll reversed → `32L`;
  - `NoRunwaysGiveNoName`: no airports, or only KSFO's for the KBFI roll → `runway == ""`;
  - `TooFarFromAnyCentreline`: the KBFI roll shifted 150 m sideways → `""`;
  - `RunwayEndNames`: (3,0) → "03", (10,2) → "10R", (28,1) → "28L", (16,3) → "16C";
  - `JsonRoundTrip` for one airport.
- [ ] **Step 2: Run `--gtest_filter=RunwayMatch.*`.** Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: all pass.
- [ ] **Step 5: Commit** ("Name the runway from the ground roll, never score it (D-126, D-122)").

### Task 4: Touchdown capture

**Files:** Create `src/touchdown_capture.h/.cpp`, `tests/touchdown_capture_test.cpp`; modify `CMakeLists.txt`.

**Interfaces:**
- Produces:
  ```cpp
  struct FastSample { TimePoint time{}; bool onGround = false; double vsFpm = 0, aglFt = 0, gForce = 0, iasKt = 0,
                      lat = 0, lon = 0; };
  struct Touchdown { TimePoint time{}; double vsFpm = 0, gForce = 0, iasKt = 0, lat = 0, lon = 0; };
  class TouchdownCapture {
  public:
      std::optional<Touchdown> onSample(const FastSample& s);  // once, 1 s after the first contact
      void reset();
      bool captured() const;
  };
  ```
- The touchdown is the first sample on the ground after at least one airborne sample. `time`, `lat`, `lon` are that
  sample's; `vsFpm` and `iasKt` are the last airborne sample's; `gForce` is the highest from contact to contact + 1 s.
  It's returned with the first sample at or after contact + 1 s. A bounce never gives a second one.

- [ ] **Step 1: Failing tests:** `RateIsTheLastAirborneSample` (−180 then contact → −180), `GIsThePeakOverOneSecond`,
  `ReturnedOneSecondAfterContact`, `ABounceIsNotASecondTouchdown`, `NothingWithoutAnAirborneSample` (armed on the
  ground), `ResetAllowsAnother`.
- [ ] **Step 2: Run.** Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: all pass.
- [ ] **Step 5: Commit** ("Touchdown from a sim-frame sample around the contact (D-125)").

### Task 5: FlightTracker, the recorder's per-flight glue

**Files:** Create `src/flight_tracker.h/.cpp`, `tests/flight_tracker_test.cpp`; modify `src/flight_match.h`
(`FlightTarget`), `CMakeLists.txt`.

**Interfaces:**
- Consumes: Tasks 1–4; `BlockOut`, `BlockIn` (`block_detector.h`); `FlightTarget`.
- Produces:
  ```cpp
  // flight_match.h, FlightTarget gains (0 = unknown):
  double destinationLat = 0, destinationLon = 0;
  int cruiseAltitudeFt = 0, transitionAltitudeFt = 0, transitionLevelFt = 0;

  struct LogEvent { std::string type; nlohmann::json data; };
  struct AltimeterSample { double captMb = 0, foMb = 0; bool captStd = false, foStd = false; };
  inline constexpr double kTouchdownArmAglFt = 300, kTouchdownDisarmAglFt = 500;
  class FlightTracker {
  public:
      void setRunways(AirportRunways airport);  // kept across flights, replaced per ICAO
      std::vector<LogEvent> onBlockOut(const BlockOut& out, const std::optional<FlightTarget>& target,
                                       double altFt, double activeS);
      std::vector<LogEvent> onSample(const PhaseSample& s);
      std::vector<LogEvent> onFastSample(const FastSample& s);
      std::vector<LogEvent> onAltimeter(const AltimeterSample& a);
      std::vector<LogEvent> onBlockIn(const BlockIn& in, double altFt);
      void onFlightEnded();                       // interrupted: forget the flight
      bool wantsFastSampling() const;
      bool flightOpen() const;
  };
  ```
- **The log events** (every one also has `flight_id`; `time` is `formatUtc`):

  | Type | Data | When |
  |---|---|---|
  | `phase_changed` | `phase`, `previous`, `time`, `alt_ft`, `flight_level` | each `PhaseChanged`; block out (Pushback), block in (Arrived) |
  | `level_off` / `level_off_ended` | `phase`, `time`, `alt_ft`, `flight_level` | each level |
  | `step_climb` / `step_climb_ended` | `time`, `from_ft` / `alt_ft`, `flight_level` | in cruise |
  | `flight_marker` | `marker`, `time`, `alt_ft` | each `Marker` |
  | `liftoff` | `time`, `lat`, `lon`, `ias_kt`, `airport`, `runway`, `off_centreline_m`, `off_heading_deg` | first airborne sample after the takeoff roll |
  | `touchdown` | `time`, `lat`, `lon`, `vs_fpm`, `g`, `ias_kt` | from `TouchdownCapture` |
  | `landing_runway` | `airport`, `runway`, `off_centreline_m`, `off_heading_deg` | at Taxi in, from the landing roll |
  | `altimeter` | `capt_mb` (0.1), `fo_mb`, `capt_std`, `fo_std` | at block out, then on any change while the flight is open |

  `flight_level` is true when the altitude is at or above the plan's transition altitude (climb, cruise) or
  transition level (descent, approach), and false without a plan. The takeoff roll's points are the samples on the
  ground at ≥ 40 kt while in Takeoff; the landing roll's are those at ≥ 40 kt in Landing. Fast sampling is wanted in
  Descent, Approach or GoAround below 300 ft AGL, until the touchdown is captured or the aircraft climbs above 500 ft
  AGL.

- [ ] **Step 1: Failing tests** (synthetic samples):
  - `BlockOutStartsPushbackAndLogsTheAltimeter`: `onBlockOut` after an `onAltimeter` returns `phase_changed`
    (`pushback`, `previous` `at_gate`) and `altimeter`;
  - `AltimeterOnlyOnChangeWhileOpen`: no flight → nothing; same values twice → one event; 1013.2 → 1013.3 → a second;
  - `LiftoffNamesTheRunway`: KBFI runways set, the KBFI roll from the fixture → `liftoff` with `runway` "14R";
  - `LandingRunwayAtTaxiIn`;
  - `FastSamplingWindow`: false in Climb at 200 ft AGL; true in Approach at 250 ft; false after the touchdown is
    captured; false again above 500 ft after a go-around;
  - `FlightLevelFlag`: a cruise `phase_changed` at 39,000 with TA 18,000 has `flight_level` true; a climb level-off at
    5,000 has false;
  - `InterruptForgetsTheFlight`: block out, climb, `onFlightEnded()`; then `onSample` returns nothing,
    `wantsFastSampling()` false, and the next `onBlockOut` starts at Pushback with markers able to fire again;
  - `EveryEventCarriesTheFlightId`.
- [ ] **Step 2: Run `--gtest_filter=FlightTracker.*`.** Expected: compile failure.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the whole core suite.** Expected: all pass.
- [ ] **Step 5: Commit** ("FlightTracker turns one flight's phases, runways and touchdown into log events").

### Task 6: Wire the recorder, SimConnection and the planned flight

**Files:** Modify `src/recorder.h/.cpp`, `src/sim_connection.h/.cpp`, `src/planned_flight.cpp` (`flightTarget`),
`src/app_main.cpp`, `src/recorder_bridge.h/.cpp` (`onFlightEvent`), `tests/planned_flight_test.cpp`; create
`src/app_event_queue.h/.cpp`, `tests/app_event_queue_test.cpp`.

**Interfaces:**
- Consumes: Task 5.
- Produces:
  ```cpp
  class AppEventQueue {  // app thread → recorder thread; the recorder is the log's only writer (D-127)
  public:
      void push(std::string type, nlohmann::json data);
      std::vector<LogEvent> drain();
  };
  // Recorder's constructor gains AppEventQueue& after the FlightTargetSlot.
  // RecorderObserver gains:
  virtual void onFlightEvent(const std::string& /*type*/, const nlohmann::json& /*data*/) {}
  // SimConnection gains:
  using FastSampleCallback = std::function<void(const FastSample&)>;
  using AltimeterCallback = std::function<void(const AltimeterSample&)>;
  using RunwaysCallback = std::function<void(AirportRunways)>;
  void setFlightCallbacks(FastSampleCallback, AltimeterCallback, RunwaysCallback);
  void requestRunways(const std::vector<std::string>& icaos);
  void setFastSampling(bool on);   // only calls SimConnect when the state changes
  double activeSeconds() const;    // ActiveClock fed by every received message
  ```
- New SimConnect definitions, each its own id: `DEF_TOUCHDOWN` (`VERTICAL SPEED` fpm, `SIM ON GROUND`, `PLANE ALT ABOVE
  GROUND` ft, `G FORCE` GForce, `AIRSPEED INDICATED` kt, `PLANE LATITUDE`/`LONGITUDE` deg), `DEF_ALTIMETER`
  (`KOHLSMAN SETTING MB:1/2` millibars, `KOHLSMAN SETTING STD:1/2` bool), and a runway-only facility definition
  (`OPEN AIRPORT`, `OPEN RUNWAY` … `CLOSE RUNWAY`, `CLOSE AIRPORT`, the runway fields the probe reads). The probe's
  own requests stay as they are.
- `flightTarget()` fills the new `FlightTarget` fields from the destination airport and the imported plan (0 without
  one).
- **Heading (D-129):** `TelemetrySample` gains `double headingTrueDeg` and `kTelemetryVars` gains
  `{"PLANE HEADING DEGREES TRUE", "degrees", VarType::Float64, 0.5f}`, both after `indicatedAirspeedKt` /
  `AIRSPEED INDICATED` (the `static_assert` keeps them in step). The `telemetry` event logs it as `heading_true_deg`.
  Nothing reads it in this milestone.

- [ ] **Step 1: Failing tests:**
  - `AppEventQueue.DrainsInOrderAndEmpties`, `AppEventQueue.PushFromAnotherThread`;
  - `PlannedFlight.TargetCarriesThePlan`: after `storeSimbriefPlan` with the test OFP, `flightTarget()` has the
    destination's position, cruise altitude and transition values; before the import they're 0.
- [ ] **Step 2: Run.** Expected: compile failures.
- [ ] **Step 3: Implement.**
  - `Recorder::run`: one `FlightTracker`. After a `BlockOut` → `tracker.onBlockOut`; each valid sample → a
    `PhaseSample` (with `sim.activeSeconds()`) to `tracker.onSample` while the flight is open; `BlockIn` →
    `tracker.onBlockIn`; `FlightInterrupted` → `tracker.onFlightEnded()`. After each call,
    `sim.setFastSampling(tracker.wantsFastSampling())`.
  - Each returned `LogEvent` is written with `log.write` and passed to `observer.onFlightEvent`, plus a feed line for
    phases, liftoff and touchdown (`"23:27:16  LANDING"`, `"TOUCHDOWN  -142 fpm  1.21 g"`).
  - Runways are requested at the handshake, and whenever the target's origin or destination differs from the last
    request; each arrival goes to `tracker.setRunways` and is logged as `airport_runways`.
  - Each loop iteration drains the `AppEventQueue` into the log.
  - `app_main.cpp` owns the queue and passes it to the recorder (and to Task 8's watcher). `BridgeObserver` posts
    `onFlightEvent` to `RecorderBridge::applyFlightEvent(QString type, QVariantMap data)`, which emits
    `flightEvent(type, data)` (Task 10 uses it).
- [ ] **Step 4: Build release and debug; run all tests.** Expected: no warnings, all pass.
- [ ] **Step 5: Run the app without the sim** (`build\x64-release\BlockTime.exe`): starts, "Waiting for sim", nothing in
  `errors.log`.
- [ ] **Step 6: Commit** ("The recorder logs phases, runways, touchdown and the altimeter (D-124..D-127)").

### Task 7: SayIntentions ATIS and ATC parsing, and the de-duplication

**Files:** Modify `src/sayintentions.h/.cpp`, `tests/atis_test.cpp`; create `src/atc_log.h/.cpp`,
`tests/atc_log_test.cpp`, fixture `tests/data/comms_history.json` (hand-written from the documented shape:
`flight_id`, `comm_history[]` with `id`, `stamp_zulu`, `frequency`, `station_name`, `incoming_message`,
`outgoing_message`, the `_english` fields, `atc_url`, `pilot_url`).

**Interfaces:**
- Produces:
  ```cpp
  std::string getWxUrl(std::string_view apiKey, std::string_view icao);  // one airport
  std::string commsHistoryUrl(std::string_view apiKey);                  // getCommsHistory, no since_id (D-123)
  struct Atis { std::string icao, letter /* "current" */, text /* "atis" */, metar, altimeter, departing, arriving; };
  std::optional<Atis> parseAtis(std::string_view json, std::string_view icao);  // nullopt: error, or not listed
  struct CommsHistory { std::string flightId; std::vector<nlohmann::json> entries; };  // entries verbatim
  std::optional<CommsHistory> parseCommsHistory(std::string_view json);  // nullopt: error or not that shape
  class CommsDedupe {
  public:
      // The entries to log: new ids, and ids whose content changed since they were logged. A different flight_id
      // starts over.
      std::vector<nlohmann::json> toLog(const CommsHistory& history);
      void reset();
  };
  class AtisDedupe {
  public:
      bool isNew(const Atis& atis);  // per ICAO: letter or text differs from the last logged
      void reset();
  };
  ```
- Fields are read loosely, like `parseAtisRunways`: a number or a string, and a missing field is `""`. `id` may come
  as a number or a string; an entry without one is keyed by its `stamp_zulu` + `frequency` + `incoming_message`.

- [ ] **Step 1: Failing tests:**
  - `Atis.ParsesTheLiveFields` from `getwx_live.json`: KBFI, letter "K", altimeter "30.02", departing "14R,14L",
    arriving "14R", text starting "Boeing Field information Kilo";
  - `Atis.ErrorOrUnknownAirportIsNothing`;
  - `CommsDedupe.LogsEachExchangeOnce`: the fixture twice → all entries, then none;
  - `CommsDedupe.RelogsAnExchangeThatChanged`: same `id`, `outgoing_message` "" then filled → logged both times;
  - `CommsDedupe.NewFlightStartsOver`;
  - `AtisDedupe.OnlyChanges`: same letter and text → false; a new letter → true; another ICAO tracked apart.
- [ ] **Step 2: Run.** Expected: compile failures.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: all pass.
- [ ] **Step 5: Commit** ("Parse SayIntentions' ATIS and ATC log; log each exchange once, again if it changes (D-127)").

### Task 8: FlightWatcher polls SayIntentions during the flight

**Files:** Create `src/flight_watcher.h/.cpp`; modify `src/planner.h`, `src/qt_planner.h/.cpp`, `src/app_main.cpp`,
`tests/qt_models_test.cpp` (the fake planner and the new tests), `CMakeLists.txt`.

**Interfaces:**
- Consumes: Task 6 (`AppEventQueue`, `RecorderBridge::flightEvent`, `RecorderBridge::blockEvent`), Task 7.
- Produces:
  ```cpp
  // Planner gains:
  virtual void fetchWx(QString icao, std::function<void(QByteArray body, QString error)> done) = 0;
  virtual void fetchCommsHistory(std::function<void(QByteArray body, QString error)> done) = 0;

  class FlightWatcher : public QObject {
      Q_OBJECT
  public:
      FlightWatcher(Planner& planner, RecorderBridge& bridge, const blocktime::FlightTargetSlot& target,
                    blocktime::AppEventQueue& queue, QObject* parent = nullptr);
      void setIntervals(std::chrono::milliseconds atc, std::chrono::milliseconds atis);  // tests; 60 s and 10 min
  };
  ```
- At `block_out` it keeps the flight id and the target's origin and destination, fetches the ATC log and the origin's
  ATIS at once, then on its timers. `liftoff` stops the origin's ATIS. `phase_changed` to `descent` starts the
  destination's (at once, then every 10 min). `touchdown` stops it. `block_in` fetches the ATC log one last time and
  stops everything; `flight_interrupted` stops everything without a last fetch.
- Pushed events: `atis` {`flight_id`, `icao`, `role` ("origin"/"destination"), `letter`, `text`, `metar`,
  `altimeter`, `departing`, `arriving`} when `AtisDedupe` says it's new; `atc_exchange` {`flight_id`,
  `si_flight_id`, `entry`} for each entry `CommsDedupe` returns. Both dedupes reset at block out.
- A call that fails, or whose answer doesn't parse, pushes nothing. Keys are never logged.

- [ ] **Step 1: Failing tests** (fake planner records calls and answers from fixtures; drive the bridge with
  `applyBlockEvent` / `applyFlightEvent`; intervals of a few ms with `QTRY_*`):
  - `FlightWatcher.PollsTheOriginUntilLiftoff`: block out → `fetchWx("KBFI")` and one `atis` with role origin; the
    same answer again pushes nothing; after `liftoff` no more KBFI calls;
  - `FlightWatcher.DestinationFromTopOfDescentToTouchdown`;
  - `FlightWatcher.AtcEveryPollAndOnceAtBlockIn`: two polls give the fixture's entries once; `block_in` triggers one
    more call, then none;
  - `FlightWatcher.FailedCallsLogNothingAndRetry`: an error, then a good answer → the queue has only the good
    answer's events;
  - `FlightWatcher.InterruptStopsWithoutALastFetch`.
- [ ] **Step 2: Run the Qt suite.** Expected: compile failures.
- [ ] **Step 3: Implement**, including `QtPlanner::fetchWx` / `fetchCommsHistory` (developer key in the header, as
  `fetchRunways` does). `app_main.cpp` creates the watcher with the real planner, the bridge, the slot and the queue.
- [ ] **Step 4: Build; run all tests.** Expected: no warnings, all pass.
- [ ] **Step 5: Commit** ("Log the ATC exchanges and the ATIS while the flight is open (D-123, D-127)").

### Task 9: FlightStages builds the six stages

**Files:** Create `src/flight_stages.h/.cpp`, `tests/flight_stages_test.cpp`; modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: Task 5's event types and fields.
- Produces:
  ```cpp
  enum class Stage { PreDeparture, Departure, Climb, Cruise, Descent, Arrival };
  enum class StageState { Upcoming, Current, Done };
  struct StageLine { std::string text, start /* "HH:MMZ" */, end /* "" = an instant, or still running */;
                     bool running = false; };
  struct StageView { Stage stage; std::string name; StageState state; std::vector<StageLine> lines; };
  class FlightStages {
  public:
      void onEvent(std::string_view type, const nlohmann::json& data);
      void reset();
      std::vector<StageView> view() const;  // always six; before any event, Pre-Departure is Current
      std::string phaseLabel() const;       // "Climb — level at 11,000 ft", "Cruise — step climb", "" before block out
  };
  ```
- Stage names: "Pre-Departure", "Departure", "Climb", "Cruise", "Descent", "Arrival". Phases map to stages as in
  design.md §13.5 (Approach, GoAround, Landing and TaxiIn are in Descent). The stage of the current phase is
  Current; earlier ones Done.
- Lines (altitudes `11,000 ft`, or `FL350` when `flight_level`):

  | Event | Stage | Text | Times |
  |---|---|---|---|
  | `phase_changed` pushback / taxi_out / takeoff | Departure | "Pushback" / "Taxiing to runway" / "Takeoff roll" | start → the next phase |
  | `liftoff` | Departure | "Takeoff" | instant |
  | `liftoff` | Climb | "Departed runway 14R" ("Departed" without a runway) | instant |
  | `level_off` … `_ended` | the phase's stage | "Level at 11,000 ft" (the ended event's altitude once it ends) | start → end |
  | `flight_marker` above_10000 / transition_altitude | Climb | "Above 10,000 ft" / "Passed transition altitude" | instant |
  | `phase_changed` cruise | Cruise | "Cruising at FL390" | start → step or descent |
  | `step_climb` … `_ended` | Cruise | "Step climb to FL410" / "Descended to FL370" ("Step climb" while running) | start → end |
  | `flight_marker` within_200nm | Cruise | "200 nm from destination" | instant |
  | `phase_changed` descent | Descent | "Top of descent" | instant |
  | `flight_marker` transition_level / below_10000 / final_approach | Descent | "Passed transition level" / "Below 10,000 ft" / "Final approach" | instant |
  | `phase_changed` approach / go_around | Descent | "Approach" / "Go-around" | start → the next phase |
  | `touchdown` (+ `landing_runway`) | Descent | "Landed, −142 fpm", then "Landed on 28R, −142 fpm" | instant |
  | `phase_changed` taxi_in | Descent | "Taxiing to gate" | start → block in |
  | `phase_changed` arrived | Arrival | "Parked" | instant |

  Pre-Departure has no lines until milestone 8's GSX services.

- [ ] **Step 1: Failing tests:** `FlightStages.SixStagesBeforeAnything`; `ADepartureInOrder` (pushback, taxi out,
  takeoff, liftoff with runway → Departure's three timed lines and "Takeoff", Climb's "Departed runway 14R", Climb
  Current); `LevelOffTextUsesTheEndedAltitude` (3,100 then ended 3,000 → "Level at 3,000 ft"); `FlightLevels`
  ("Cruising at FL390"); `LandedWithoutARunway` ("Landed, −142 fpm"); `LandedThenRunwayUpdatesTheLine`;
  `PhaseLabels` (Climb level → "Climb — level at 11,000 ft"; step climb → "Cruise — step climb"; plain → "Descent");
  `RunningLinesHaveNoEnd`.
- [ ] **Step 2: Run.** Expected: compile failure.
- [ ] **Step 3: Implement.** The minus sign in "−142 fpm" is U+2212, like the rest of the UI.
- [ ] **Step 4: Run.** Expected: all pass.
- [ ] **Step 5: Commit** ("Group the phases into design.md §13.5's six stages (D-128)").

### Task 10: The stages on Live

**Files:** Modify `src/recorder_bridge.h/.cpp`, `qml/LivePage.qml`, `qml/FlightPanel.qml`, `tests/qt_models_test.cpp`;
create `qml/FlightStages.qml`; add it to the QML module in `CMakeLists.txt`.

**Interfaces:**
- Consumes: Task 6 (`applyFlightEvent`), Task 9.
- Produces on `RecorderBridge`:
  ```cpp
  Q_PROPERTY(QVariantList stages READ stages NOTIFY stagesChanged)  // [{name, state: "upcoming"|"current"|"done",
                                                                    //   lines: [{text, start, end, running}]}]
  Q_PROPERTY(QString phase READ phase NOTIFY stagesChanged)         // FlightStages::phaseLabel()
  ```
  `applyFlightEvent` feeds the bridge's `FlightStages`. A `block_out` resets it before its `phase_changed` arrives;
  `flight_interrupted` resets it.
- **Live:** a "Flight progress" card (`FlightStages.qml`, using `Card`) between the planned-flight panel and the
  flight panel, shown while `recorder.flightOpen`. One row per stage: the name, accented when Current, dimmed when
  Upcoming; under it each line as time (`22:51Z` or `22:51Z – 22:54Z`) and text. **FlightPanel's Flight card** shows
  `recorder.phase` under the block clock.

- [ ] **Step 1: Failing tests:** `RecorderBridge.StagesFollowFlightEvents` (apply a pushback `phase_changed` → stage 1
  Current, one running line, `phase` "Pushback"); `RecorderBridge.StagesResetAtBlockOut` (lines from an old flight
  are gone after a new `block_out`); `RecorderBridge.StagesResetOnInterrupt`.
- [ ] **Step 2: Run the Qt suite.** Expected: compile failures.
- [ ] **Step 3: Implement the bridge properties, then the QML.**
- [ ] **Step 4: Build; run all tests; run qmllint.** Expected: no warnings, all pass, qmllint silent.
- [ ] **Step 5: Look at it:** `--demo` (extend the demo to replay the KBFI→KPDX fixture's events through
  `applyFlightEvent`, if it doesn't already feed flight events) and `--grab` the Live page mid-flight and after
  landing; check both at the smallest window size.
- [ ] **Step 6: Commit** ("Live shows the flight's progress in six stages (design.md §13.5)").

### Task 11: Docs, version, review, sim checks

**Files:** `CMakeLists.txt`, `vcpkg.json` (0.8.0), `docs/recorder.md`, `docs/design.md`, `docs/roadmap.md`,
`docs/decisions.md`, `docs/notes/probe-flight.md`, `README.md`, `docs/progress.md`.

- [ ] **Step 1: Version 0.8.0.**
- [ ] **Step 2: Docs.**
  - recorder.md: the phase rules' numbers (D-124), the events table's new rows and `heading_true_deg` in `telemetry`
    (D-129); "Parking spots and runways" says
    the recorder names the runway (D-126); "ATC log, ATIS and altimeter" says how the app's results reach the log
    (D-127); milestones 4–5 marked built.
  - design.md §13.5: what milestone 7 shows (and that GSX and the gate come in milestones 8–9).
  - roadmap: milestone 7 built.
- [ ] **Step 3: Whole-milestone review** (`requesting-code-review`, then `receiving-code-review`): fix Critical and
  Important findings, each with a test that fails first; list the deferred minors.
- [ ] **Step 4: Verification** (`verification-before-completion`): clean release and debug builds, all tests, qmllint.
- [ ] **Step 5: Progress entry with Dave's sim checks**, ordered to need one sim session, combined with the next probe
  flight (the end of `probe-flight.md`) and the deferred fuel check. At least:
  1. the phases and stages on Live through a full flight, compared with what Dave did;
  2. `altimeter` lines follow both baro knobs and STD (and agree with `probe_altimeter`);
  3. a `touchdown` with a believable rate, compared with what Dave felt or another tool showed;
  4. `liftoff` / `landing_runway` name the runways flown;
  5. `atis` lines for the origin before takeoff and the destination after top of descent, and `atc_exchange` lines
     matching SayIntentions' own history;
  6. a sim freeze, if one happens, doesn't fake a level-off.
- [ ] **Step 6: Commit.** Push only when Dave's checks pass and the milestone closes (D-121).
