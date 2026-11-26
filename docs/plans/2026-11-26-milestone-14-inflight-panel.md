# Milestone 14: The In-Flight Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, D-033) to implement this plan
> task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** From block out to block in, Live shows one instrument panel under the header:
- seven lamps that flash while a switch is wrong by the debrief's rules;
- four steam gauges;
- an ATC card with the gist of each clearance;
- readouts that change with the phase, including the arrival ATIS.

Cards and gauges flip in a left-to-right wave when they change. The Pre-Flight panel loses its departure frequency,
says the ATIS letter as a word and hides at block out. Debrief check 2 ends at the transition altitude for new flights.
The real app and the replay window both get it.

**Architecture:** The logic is plain C++ in `blocktime-core`, written tests first:
- one shared reading of the cockpit lines, so the lamps and the debrief agree by construction;
- a `LampBoard` with the debrief's windows;
- readers that turn one ATC transmission into slot changes;
- an `AtcCard` that keeps the slots;
- the arrival ATIS readers;
- an `inFlightReadouts` function that picks the readouts for the phase.

The recorder and the replay start passing `cockpit` lines to the bridge, which today only the log gets. A new Qt
object, `InFlightModel`, feeds all of it from the bridge's events and samples and turns it into QML properties. A new
`InFlightPanel.qml` draws it with new `Lamp`, gauge and `FlipCard` components on top of milestone 13's
`InstrumentPanel` and `Readout`.

**Tech Stack:** C++20, MSVC, Qt 6 / QML (Qt Quick Shapes, already used by `RouteMap.qml`), nlohmann::json, GoogleTest.

**Spec:** `docs/dashboard.md`, "The in-flight panel (brainstorm of 2026-11-26, D-191 to D-198)" and "The instrument
panel look (D-179)"; D-191 to D-200. The milestone's scope is in `docs/roadmap.md`, "Milestone 14". Claude's calls
made while planning are D-201 (Proposed), written with this plan.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`. `blocktime-app_qmllint` reports nothing new (the one Info in
  `ReplayBar.qml` is from milestone 11).
- Every new file starts with the GPL header. Every QML file has `import BlockTime`, `pragma ComponentBehavior: Bound`
  and `required` properties in delegates.
- Every colour, size, font and duration goes in `qml/Theme.qml` (D-054): the lamp lens colours, the gauge faces, the
  flip's 500 ms and the wave's 60 ms step.
- **No continuous animations** except a lamp flashing (1 s lit, 1 s dark) while it's wrong (D-195). The flash timer
  runs only while a lamp is wrong. Needles glide to each new value and are still between samples. Flips run only on a
  change.
- Nothing on Live reads the wall clock (D-166). Use only `RecorderBridge::now()`.
- **No radio frequency anywhere on the dashboard** (D-191). The squawk stays.
- **A guessed clearance is an anti-goal.** A phrase BlockTime can't read changes no slot.
- **Lamps never nag about anything the debrief doesn't score** (D-195). Outside its windows a lamp never flashes. A
  lamp whose data isn't in the log stays dark and never flashes.
- Milestone 15 fills top of climb, top of descent, landing time, fuel against plan, fuel at landing, the plan's
  cruise-speed bug and the header's arrival line. None of them is in this milestone, not even as empty readouts.
- Core keeps ISO UTC strings and `TimePoint`s. The Qt layer formats local times (`11:01 AM`).
- Version 0.15.0.
- Test commands: `cmake --build --preset x64-debug`, then `ctest --preset x64-debug`, and the same with `x64-release`
  (`README.md`). 792 tests pass at the start.

## Review Focus

1. **Numbers in calls that aren't clearances.** "Traffic, 9 o'clock, 5 miles, Airbus, 4,000." and "Contact Los
   Angeles Center on 134.575." must change nothing. "Altimeter 30.19" inside the descend-via call must not become an
   altitude. Tests: `AtcInstructions.TrafficCallChangesNothing`, `AtcInstructions.FrequencyChangesNothing` and
   `AtcInstructions.DescendViaIgnoresTheAltimeter` (Task 6).
2. **SayIntentions logs the same exchange again.** It re-logs when the content changes, and BlockTime re-logs the
   whole history at block out. A repeat must not move `LATEST` or reset a slot to an older value. Exchanges are kept by
   id and applied in ATC's time order. Test: `AtcCard.RepeatedExchangeChangesNothing` (Task 7).
3. **A flight that never reaches the transition altitude, or a 737 without PMDG data.** The seat belt lamp's first
   window ends at the descent or the touchdown, as the debrief's does. Without PMDG data the seat belts and landing
   lights stay dark and never flash. Tests: `LampBoard.LowFlightClosesBeltsWindowAtDescent` and
   `LampBoard.NoPmdgDataNeverFlashes` (Task 5).
4. **A go-around.** The APPROACH slot clears, the panel goes back to the approach readouts, and the landing lights and
   gear windows follow the approach that lands, as the debrief does. Tests: `AtcCard.GoAroundClearsApproach` (Task 7),
   `InFlightReadouts.GoAroundLeavesShortFinal` (Task 8), `LampBoard.GoAroundRestartsLandingLights` (Task 5).
5. **A replay seek or the app opening mid-flight.** Nothing flips and no wave runs on the first fill. Only a change
   after the panel is on show flips. Test: `InFlightModel.FirstFillIsNotAChange` (Task 9). Dave checks the flips by
   eye, since offscreen grabs don't show animation.

---

### Task 1: The v2 mockup, for Dave's approval

The brainstorm's v1 (`docs/images/dashboard-inflight-v1.html`) was approved in direction. v2 shows what milestone 14
actually builds, with what changed after v1. No code is built until Dave approves.

**Files:**
- Create: `docs/images/dashboard-inflight-v2.html`, starting from a copy of v1 (same moments, same Next button, same
  real ATC words from `tests/data/flight-ksan-ksjc`)

- [ ] **Step 1: Apply the changes after v1:**
  - the altitude gauge with D-202's three hands on a 0–9 dial and no drum: short and wide for the ten thousands, long
    for the thousands, long and skinny for the hundreds; the setting window stays;
  - no frequency anywhere;
  - the ATIS letter as its word (`JULIETT`);
  - none of milestone 15's readouts: top of climb, top of descent, landing, fuel at landing, the fuel bug and its
    `+200` window, the cruise-speed bug, the header's On time / Delayed line;
  - Climb shows CRUISE (`FL400`);
  - Cruise shows CRUISE, then DEST ATIS (`JULIETT`, aside `30L 30R`) from the `within_200nm` marker;
  - Descent and Approach show the seven arrival ATIS readouts: ATIS `JULIETT`, RUNWAYS `30L 30R`, APPROACHES
    `ILS 30L · RNAV-Z 30R`, WIND `Variable / 4 kt`, OAT `17 °C`, ALTIMETER `30.19 inHg` with aside `SET AT TL`,
    CONDITION `DRY`;
  - Descent and Approach also show STAR `SILCN6 TROXX` and LANDING RWY `30L`, each with the amber `PLAN …` flag when
    ATC's differ;
  - Short final: HEIGHT, WIND (`360° / 4 kt`, aside `TOWER`), RUNWAY, VREF (`138 KT`, aside `FLAPS 30`);
  - Arrival: LANDING with the stars and `−100 FPM`, and GATE `4`. The TOUCHDOWN distance is dropped (not in the
    spec).
- [ ] **Step 2: Lay out the readout block** for nine readouts (Descent and Approach) beside the ATC card. With the
  header collapsed, the panel must fit a 760 px window, as Pre-Flight does. The mockup decides the grid.
- [ ] **Step 3: Animate what Dave can only check by eye:**
  - a wrong lamp flashing 1 s / 1 s;
  - the flip, a half-turn about the horizontal axis in about 500 ms;
  - the wave, 60 ms per card by x position, on Next.
  
  Moment 8 (three slots in one call) and the phase changes show the wave.
- [ ] **Step 4: Show Dave** (open the file in the browser pane). Rework it until he approves, then record what he
  changed in D-201 or the spec.
- [ ] **Step 5: Commit** "In-flight panel mockup v2 approved (D-201)", with stills grabbed from the browser pane:
  `docs/images/dashboard-inflight-v2.png` (cruise) and `-approach.png`.

### Task 2: D-200: ATC every 30 s, the destination ATIS from 200 nm, check 2 to the transition altitude

**Files:**
- Modify: `src/flight_watcher.cpp` (`setIntervals(60s, 5min)` → `30s`; the destination trigger at lines 79–86),
  `src/flight_watcher.h` (the comment "60 s and 10 min")
- Modify: `src/debrief_measure.cpp` (`beltsDeparture`), `src/debrief_measure.h` (the comment on
  `belts_off_departure_s`), `src/debrief_score.cpp:82` (the label)
- Modify: `docs/debrief.md` (check 2's row at line 107 and its note at line 270)
- Test: `tests/qt_models_test.cpp` (FlightWatcher), `tests/debrief_measure_test.cpp`, `tests/debrief_store_test.cpp`
  (or wherever `writeMeasurements` is tested)

**Interfaces:**
- Produces: none new. `measureFlight`'s `belts_off_departure_s` now runs from the start of taxi out to the first
  `transition_altitude` marker, else the descent, else the touchdown.

- [ ] **Step 1: Write the failing tests:**
  - `FlightWatcher.DestinationAtisFrom200Nm`: a `flight_marker` with `marker: within_200nm` fetches the destination
    ATIS and starts the timer. A `phase_changed` to `descent` after it doesn't fetch again. A flight with no
    `within_200nm` marker still starts at `descent` or `approach`, as today.
  - `DebriefMeasure.BeltsWindowEndsAtTransitionAltitude`: belts off between `above_10000` and `transition_altitude`
    count. With belts off for 40 s between the two markers and on everywhere else, `belts_off_departure_s` is 40.
  - `DebriefMeasure.LowFlightSkipsTenThousandChecks`: extend it so that, with no `transition_altitude` marker, the
    window ends at the descent.
  - `DebriefStore.StoredMeasurementIsNotRemeasured`: a flight written with `belts_off_departure_s` 0 loads back as 0
    (D-160: a changed measurement applies to flights settled after it).
- [ ] **Step 2: Run them to make sure the new ones fail.** Run: `ctest --preset x64-debug -R
  "DestinationAtisFrom200Nm|BeltsWindowEndsAtTransitionAltitude"`. Expected: FAIL.
- [ ] **Step 3: Implement the changes:**
  - In `FlightWatcher`, `within_200nm` joins the destination trigger, with the same guards (`!destinationTimer_
    .isActive() && !destinationDone_`).
  - One ATC interval of 30 s, which also applies to the gate's polling (D-201).
  - In `beltsDeparture`, `transition_altitude` replaces `above_10000`.
  - The label becomes "Seat belt sign, taxi to transition altitude". The label shows on old flights too; only the
    measurement is per flight.
- [ ] **Step 4: Update the KLAS → KSAN fixture's expected value** in `DebriefMeasure.KlasKsanFixture` if it moves.
  Say in the commit why: the window is longer.
- [ ] **Step 5: Run all tests.** Run: `ctest --preset x64-debug`. Expected: all pass.
- [ ] **Step 6: Commit** "D-200: ATC every 30 s, destination ATIS from 200 nm, belts check to the transition
  altitude".

### Task 3: Pre-Flight: until block out, the ATIS word, no departure frequency

**Files:**
- Modify: `src/departure_conditions.h/.cpp` (add `phoneticWord`), `src/preflight_log.cpp` (`departureView`'s
  `atisLetter`), `src/preflight_model.h/.cpp`, `qml/PreFlightPanel.qml`
- Test: `tests/departure_conditions_test.cpp`, `tests/qt_models_test.cpp`

**Interfaces:**
- Produces (`departure_conditions.h`):
  ```cpp
  // "J" → "JULIETT", "a" → "ALFA": the ICAO spelling (D-201, Dave). "" for anything else.
  std::string phoneticWord(std::string_view letter);
  ```
- Produces: `DepartureView::atisLetter` now holds the word (`LIMA`). `PreFlightModel::visible()` is
  `hasFlight_ && !log_.blockedOut()`. `PreFlightModel` loses `departureFrequency`, `taxiRoute` and `taxiRunway`, and
  the panel loses their readouts: the taxi route moves to the ATC card (D-194).
  `PreFlightLog::taxi()` stays, since `atcRunway()` uses it.

- [ ] **Step 1: Write the failing tests:**
  - `PhoneticWord.Letters`: `J` → `JULIETT`, `L` → `LIMA`, `a` → `ALFA`, `X` → `XRAY`, `Z` → `ZULU`, `""` → `""`, `JJ` → `""`.
  - `PreFlightModel.HidesAtBlockOut`: the existing visibility test's liftoff case moves to block out.
  - `PreFlightModel.AtisLetterIsAWord`.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement.** Remove the departure frequency and taxi readouts from `PreFlightPanel.qml`.
  `Clearance::departureFrequency` stays in core (parsing it costs nothing), but nothing shows it.
- [ ] **Step 4: Run all tests and qmllint.** Run: `cmake --build --preset x64-debug --target blocktime-app_qmllint`,
  then `ctest --preset x64-debug`. Expected: all pass, nothing new from qmllint.
- [ ] **Step 5: Commit** "Pre-Flight: hides at block out, ATIS as its word, no departure frequency (D-191, D-193,
  D-194)".

### Task 4: The cockpit reaches the app, through one shared reading

The lamps must use the debrief's reading of the `cockpit`, `altimeter` and telemetry lines, not a copy of it
(`dashboard.md`, "Lamps"). Today `cockpit` lines go only to the log: the recorder writes them without telling the
observer (`src/recorder.cpp:207` and `:220`), and the replay skips them (`kLogOnly` in `src/flight_replay.cpp:61`).

**Files:**
- Create: `src/cockpit_reading.h/.cpp`; add it to `blocktime-core` in `CMakeLists.txt`
- Modify: `src/debrief_measure.h/.cpp`, `src/debrief_score.cpp` (use the shared reading and constants),
  `src/recorder.cpp`, `src/flight_replay.cpp`
- Test: `tests/cockpit_reading_test.cpp` (add it to `blocktime-tests`), `tests/flight_replay_test.cpp`

**Interfaces:**
- Produces (`cockpit_reading.h`). `Family` moves here from `debrief_measure.h`, which includes this header.
  ```cpp
  enum class Family { B737, B777 };
  Family familyOf(std::string_view aircraftFolderOrFamily);  // "777" anywhere → B777, else B737

  // The debrief's reading of the logged lines (D-159); the lamps use the same (D-195).
  bool beltsOn(const nlohmann::json& cockpit);           // seat_belts 1 (auto) or 2 (on)
  bool landingLightsOn(const nlohmann::json& cockpit);   // landing_lights
  bool gearDown(const nlohmann::json& cockpit);          // gear_down
  std::optional<bool> apuSwitchOn(const nlohmann::json& cockpit);  // nullopt without apu_switch (no PMDG data, D-172)
  bool altimetersStd(const nlohmann::json& altimeter);   // capt_mb and fo_mb both within 0.05 of 1013.2
  // The seat belts and the 737's landing lights need PMDG's data (D-159): readable on a 777, or once a cockpit line
  // with pmdg: true was seen.
  bool beltsReadable(Family family, bool pmdgSeen);

  // The windows' times, shared by debrief_measure.cpp, debrief_score.cpp and the lamps.
  inline constexpr std::chrono::seconds kBeaconTolerance{10};       // the score's "within 10 s"
  inline constexpr std::chrono::seconds kTakeoffLightsGrace{10};
  inline constexpr std::chrono::seconds kStdTolerance{60};          // std_late_s passes within 60 s
  inline constexpr std::chrono::seconds kDestinationBaroDelay{60};  // after transition_level
  inline constexpr std::chrono::seconds kLandingLightsDelay{60};    // after below_10000
  inline constexpr std::chrono::seconds kStrobesTolerance{180};     // after taxi in starts
  ```
- Produces: `RecorderObserver::onFlightEvent("cockpit", data)` is called right after each `log.write("cockpit", …)`,
  including the block-out snapshot. `replayRoute("cockpit") == ReplayRoute::Flight`. So `RecorderBridge::flightEvent`
  carries `cockpit` on Live and in replays.

- [ ] **Step 1: Write the failing tests:**
  - `CockpitReading.*`: one test per function. Seat belts 0/1/2; altimeters 1013.2/1013.2 is STD and 1013.2/1019.0
    isn't; no `apu_switch` → nullopt; `beltsReadable(B737, false)` is false.
  - `FlightReplay.CockpitLinesReachTheBridge`: `replayRoute("cockpit")` is `Flight`.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement** `cockpit_reading.cpp`. Make `debrief_measure.cpp` and `debrief_score.cpp` call it instead
  of their own lambdas and constants (the lambdas in `beltsDeparture`, `beltsDescent`, `stdLate`, `landingLights`,
  `stable`, `apuTransition`, `apuGate`; `kTakeoffLightsGrace` and `kStdHpa`; the tolerances in
  `debrief_score.cpp`). Forward `cockpit` to the observer in `recorder.cpp`, and drop it from `kLogOnly`.
- [ ] **Step 4: Run all tests.** Every `DebriefMeasure` and `DebriefScore` test must pass unchanged: the reading
  moved, it didn't change.
- [ ] **Step 5: Commit** "One reading of the cockpit for the debrief and the lamps; cockpit lines reach the bridge
  (D-195)".

### Task 5: The lamps: `LampBoard`

**Files:**
- Create: `src/lamps.h/.cpp`; add to `blocktime-core`
- Test: `tests/lamps_test.cpp`; add to `blocktime-tests`

**Interfaces:**
- Consumes: `cockpit_reading.h` (Task 4); `TelemetrySample` (`src/sim_vars.h`: `lightBeacon`, `lightStrobe`,
  `engCombustion`, `numEngines`).
- Produces (`lamps.h`):
  ```cpp
  enum class Lamp { Beacon, Strobes, LandingLights, SeatBelts, Apu, AltStd, GearDown };  // the strip's order
  struct LampState {
      Lamp lamp;
      bool known = false;  // its data is in the log; unknown lamps are dark and never wrong
      bool lit = false;    // the switch is on (ALT STD: both on STD; GEAR DN: down)
      bool wrong = false;  // inside one of its windows and in the wrong state: the lamp flashes
  };
  class LampBoard {
  public:
      void reset(Family family);  // a new flight
      // phase_changed, flight_marker, cockpit, altimeter, liftoff, touchdown; anything else is ignored. `at` is
      // data.time when the line has one (the takeoff is back-dated to the start of the roll), else when it arrived.
      void onEvent(std::string_view type, const nlohmann::json& data, TimePoint at);
      void onBlockOut(TimePoint at);
      void onBlockIn(TimePoint at);
      void onSample(const TelemetrySample& s, TimePoint at);  // the beacon, the strobes and engine starts
      std::array<LampState, 7> lamps(TimePoint now) const;
  };
  ```

The windows, from `dashboard.md`'s lamp table. T is `now`. "Last" means the latest such event, as the debrief takes
it.

| Lamp | Wrong while |
|---|---|
| Beacon | off, from `min(block out, the engine start after the engines were last all off) + kBeaconTolerance` until block in |
| Seat belts | off, from taxi out until the first of `transition_altitude`, descent, touchdown; and off from the last `transition_level` until block in |
| Strobes | off, from takeoff + `kTakeoffLightsGrace` until liftoff; on, from the last taxi in + `kStrobesTolerance` until block in |
| Landing lights | off, from takeoff + `kTakeoffLightsGrace` until liftoff; off, from `max(last below_10000 + kLandingLightsDelay, last go_around)` until touchdown |
| APU | the selector on, from `transition_altitude` until block in |
| ALT STD | not STD, from `transition_altitude + kStdTolerance` until `transition_level`; still STD, from the last `transition_level + kDestinationBaroDelay` until touchdown |
| Gear down | up, from the last `final_approach` until touchdown |

The spec's windows after block in (the beacon until the engines are off, the APU off until the last engine stops)
aren't built. The panel hides at block in (D-154), so they could never show. This is a question for Dave in D-201.

- [ ] **Step 1: Write the failing tests**, with timelines built in code like `debrief_measure_test.cpp`'s:
  - `LampBoard.BeaconFlashesTenSecondsAfterEngineStart`: engine start 30 s before block out, beacon off. Wrong at
    start + 11 s, not at start + 9 s, and not once the beacon is on.
  - `LampBoard.BeltsOnUntilTransitionAltitude`: belts off at `above_10000` is wrong; at `transition_altitude` + 1 s it
    isn't; in cruise it never is.
  - `LampBoard.BeltsOffAfterTransitionLevel`.
  - `LampBoard.LowFlightClosesBeltsWindowAtDescent`.
  - `LampBoard.NoPmdgDataNeverFlashes`: a 737 with only `pmdg: false` cockpit lines. Seat belts and landing lights
    are `known == false`, `wrong == false`, all flight.
  - `LampBoard.TakeoffLightsGrace`: strobes off at takeoff + 9 s isn't wrong; at + 11 s it is; after liftoff it isn't.
  - `LampBoard.StrobesOnAfterTaxiIn`: wrong from taxi in + 180 s.
  - `LampBoard.LandingLightsBelow10000`, and `LampBoard.GoAroundRestartsLandingLights`.
  - `LampBoard.ApuOnAfterTransitionAltitude`, and `LampBoard.NoApuSwitchIsUnknown`.
  - `LampBoard.AltStdBothWays`: not STD at `transition_altitude` + 61 s is wrong; STD at `transition_level` + 61 s is
    wrong; STD between them isn't.
  - `LampBoard.GearUpOnFinal`.
  - `LampBoard.NothingIsWrongBeforeBlockOut`.
  - `LampBoard.KsanKsjcFixture`: the whole `tests/data/flight-ksan-ksjc` log. At seven moments (block out + 1 min,
    taxi, the takeoff roll, cruise, descent, final, taxi in) a lamp is wrong only where `measureFlight` on the same
    log scores that check as missed.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement `LampBoard`.** Keep the last cockpit and altimeter state and the window edges as
  `std::optional<TimePoint>`. `lamps(now)` is pure.
- [ ] **Step 4: Run all tests.** Expected: all pass.
- [ ] **Step 5: Commit** "LampBoard: lamps that flash by the debrief's windows (D-195)".

### Task 6: ATC's words to slot changes

**Files:**
- Create: `src/atc_instructions.h/.cpp`; add to `blocktime-core`
- Modify: `src/clearance.h/.cpp` (the expected level; the plan's SID form moved here from `preflight_log.cpp`),
  `src/preflight_log.cpp` (use it)
- Test: `tests/atc_instructions_test.cpp`; add to `blocktime-tests`

**Interfaces:**
- Produces (`clearance.h`):
  ```cpp
  struct Clearance {  // add:
      std::string expectLevel;  // "Expect FL400 one-zero minutes after departure" → "FL400"; ""
      int expectMinutes = 0;    // 10; 0 when not said
  };
  // The plan's full form "PADRZ2 IKAYE" when ATC's SID has the plan's name (or ATC named none); else ATC's own, and
  // *changed is set. Moved out of clearanceView, which now calls it.
  std::string planSidForm(std::string_view atcSid, std::string_view planSid, std::string_view planTransition,
                          bool* changed);
  ```
- Produces (`atc_instructions.h`):
  ```cpp
  enum class Slot { Taxi, HoldShort, Altitude, Route, Speed, Approach };
  struct SlotChange {
      Slot slot;
      std::string title;  // "ROUTE" or "HEADING" for Slot::Route; the slot's name otherwise ("HOLD SHORT")
      std::string value;  // "" clears the slot ("resume normal speed")
      std::string aside;  // a qualifier, or "" (the card shows the time instead)
  };
  struct RoutePlan { std::string sid, sidTransition; };  // the plan's, for the clearance's ROUTE
  // Everything one transmission changed, in the order said. Empty when nothing is read.
  std::vector<SlotChange> readInstructions(std::string_view atcWords, const RoutePlan& plan);
  ```

The phrases. The first eleven rows are from the KSAN → KSJC log, word for word; the rest are made up from the spec.

| Test | ATC says | Changes |
|---|---|---|
| `Clearance` | "Cleared to Norman Y. Mineta San Jose via the PADRZ2 departure. Then as filed. Climb via the SID. Expect FL400 one-zero minutes after departure. Departure on 119.6. Squawk 3166. Expect runway 27" (plan `PADRZ2`/`IKAYE`) | ALTITUDE `CLIMB VIA SID` aside `EXP FL400 +10 MIN`; ROUTE `PADRZ2 IKAYE` |
| `TaxiOut` | "Runway 27 taxi via A, A7, B, B1." | TAXI `A, A7, B, B1` aside `RWY 27` |
| `ClimbAndMaintain` | "Climb and maintain FL220" | ALTITUDE `FL220` |
| `GreetingThenClimb` | "Los Angeles Center, Good morning, climb and maintain FL300" | ALTITUDE `FL300` |
| `DescendViaIgnoresTheAltimeter` | "Descend via SILCN6, TROXX transition into San Jose. San Jose is currently operating west. Altimeter 30.19" | ALTITUDE `DESCEND VIA SILCN6`; ROUTE `SILCN6 TROXX` |
| `ExpectApproach` | "NorCal Approach, Juliet is current, thanks for that. Good morning. Expect the ILS runway 30L approach into San Jose via KLIDE transition" | APPROACH `EXP ILS 30L` aside `KLIDE` |
| `ThreeInOneCall` | "12 miles from HIVAK. Turn left heading 320. Descend and maintain 2,700 until established on the localizer. Cleared for the ILS runway 30L approach." | HEADING `320` aside `LEFT`; ALTITUDE `2,700` aside `UNTIL ESTABLISHED`; APPROACH `CLEARED ILS 30L` |
| `TrafficCallChangesNothing` | "Traffic, 9 o'clock, 5 miles, Airbus, 4,000." | none |
| `ClearedToLand` | "San Jose Tower, Runway 30L, Winds 360 at 4. Cleared to land." | APPROACH `CLEARED TO LAND 30L` |
| `TaxiIn` | "San Jose Ground, Taxi to Terminal Alpha Gate 4 via Kilo, hold short of Runway 30R" | TAXI `K` aside `TERMINAL A GATE 4`; HOLD SHORT `30R` |
| `CrossRunway` | "cross-runway 30R" | HOLD SHORT `CROSS 30R` |
| `FrequencyChangesNothing` | "Contact Los Angeles Center on 134.575." and "Monitor Tower on 118.3. Good day." | none |
| `TakeoffClearanceChangesNothing` | "Lindbergh Tower, Runway 27, Winds calm. RNAV WNFLD, cleared for takeoff." | none |
| `FlyHeading` | "Fly heading 340" | HEADING `340`, aside "" |
| `Speed` | "Maintain 210 knots" / "Resume normal speed" | SPEED `210 KT` / SPEED "" |
| `ClearedWithTransition` | "Cleared RNAV Y runway 27 approach, LYNDI transition" | APPROACH `CLEARED RNAV-Y 27 LYNDI` |
| `Direct` | "Proceed direct KLIDE" / "Resume own navigation" | ROUTE `DIRECT KLIDE` / ROUTE `OWN NAV` |
| `FeetAndFlightLevel` | "Descend and maintain 11,000" / "Climb and maintain flight level 240" / "Climb and maintain 5000" | `11,000` / `FL240` / `5,000` |
| `ClimbViaSidLater` | "Climb via the SID" | ALTITUDE `CLIMB VIA SID` |
| `InitialAltitudeClearance` | the clearance with "Climb and maintain 5,000. Expect FL350 five minutes after departure" | ALTITUDE `5,000` aside `EXP FL350 +5 MIN` |

- [ ] **Step 1: Write the failing tests**, one `TEST(AtcInstructions, <name>)` per row, asserting the whole vector.
  Also `Clearance.ExpectLevel` (`FL400`, 10) in `clearance_test.cpp`, and move the existing SID-form cases to
  `Clearance.PlanSidForm`.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement `readInstructions`.** Split the words into sentences and read each with its own pattern,
  case-insensitive:
  - a clearance (`parseClearance`) first;
  - a taxi-out (`parseTaxi`);
  - a taxi-in ("taxi to <place> via <route>", phonetic words to letters: Alpha → A, Kilo → K, "Alpha 7" → A7);
  - hold short;
  - cross;
  - climb/descend and maintain (FL or feet, "until established" as the aside);
  - climb/descend via;
  - heading;
  - speed;
  - expect/cleared approach (approach types ILS, LOC, RNAV with a letter, VOR, VISUAL);
  - cleared to land;
  - direct and own navigation.
  
  A sentence that matches none changes nothing. Digits only count after a verb pattern (that is what keeps "Airbus,
  4,000" and "Altimeter 30.19" out).
- [ ] **Step 4: Run all tests.** Expected: all pass, including `PreFlightLog` and `ClearanceView` unchanged.
- [ ] **Step 5: Commit** "ATC's words to slot changes for the ATC card (D-197)".

### Task 7: The ATC card: `AtcCard`

**Files:**
- Create: `src/atc_card.h/.cpp`; add to `blocktime-core`
- Test: `tests/atc_card_test.cpp`; add to `blocktime-tests`

**Interfaces:**
- Consumes: `AtcMessage`, `atcMessage()` (`atc_message.h`), `readInstructions`, `RoutePlan` (Task 6).
- Produces (`atc_card.h`):
  ```cpp
  enum class CardStage { Departure, Airborne, Arrival };  // block out to the takeoff roll; to touchdown; after it
  struct AtcSlot {
      std::string title;  // "TAXI", "ALTITUDE", "ROUTE" / "HEADING", "SPEED", "APPROACH", "HOLD SHORT"
      std::string value;  // "" = nothing given yet (the readout shows a dash)
      std::string aside;  // the qualifier; "" = show `time` as a local time
      std::string time;   // ATC's stamp_zulu of the call that set it; "" when empty
      bool latest = false;
  };
  class AtcCard {
  public:
      // Messages before plannedAt don't count (the previous leg's). A different plannedAt starts over.
      void setFlight(std::string plannedAt, RoutePlan plan);
      void onMessage(const AtcMessage& m);  // any airport or center; a repeat of an id replaces it
      void onGoAround(std::string time);    // clears APPROACH (D-201)
      // Departure: ALTITUDE, ROUTE, TAXI (v2: ALTITUDE and ROUTE where they'll be airborne; the QML leaves SPEED's
      // place empty). Airborne: ALTITUDE, ROUTE (or HEADING), SPEED, APPROACH. Arrival: TAXI, HOLD SHORT.
      std::vector<AtcSlot> slotsFor(CardStage stage) const;  // not slots(): a Qt macro
  };
  ```
- The card is rebuilt from scratch on every message and go-around: apply every kept message in `time` order, then the
  go-arounds at their times. A whole flight is about 50 exchanges. `LATEST` marks the slots changed by the last
  transmission that changed any slot, until the next one that does (v1's rule, D-201). A slot's time is the call that
  last changed its value. Messages with no `time` are dropped, as in `PreFlightLog`.

- [ ] **Step 1: Write the failing tests:**
  - `AtcCard.KsanKsjcFixture`: feed every `atc_exchange` of `tests/data/flight-ksan-ksjc`.
    - At 19:00:30Z, Airborne: ALTITUDE `FL380`, ROUTE `PADRZ2 IKAYE`, SPEED and APPROACH empty.
    - At 19:50:30Z: HEADING `320` aside `LEFT`, ALTITUDE `2,700` aside `UNTIL ESTABLISHED` and APPROACH
      `CLEARED ILS 30L`, all three `latest`.
    - At the end, Arrival: TAXI `K` aside `TERMINAL A GATE 4` and HOLD SHORT `CROSS 30R`.
  - `AtcCard.BeforeTheFirstAltitudeClearance`: after the clearance only, ALTITUDE is `CLIMB VIA SID` with aside
    `EXP FL400 +10 MIN`; after "Climb and maintain FL220" it is `FL220` with aside "" and `time` set.
  - `AtcCard.RepeatedExchangeChangesNothing`: re-sending an earlier id with the same words after a later call leaves
    the slots and `latest` as they were.
  - `AtcCard.UnreadCallKeepsLatest`: a traffic call after the three-in-one call leaves all three `latest`.
  - `AtcCard.PreviousLegIgnored`: messages before `plannedAt` change nothing.
  - `AtcCard.GoAroundClearsApproach`.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement `AtcCard`.**
- [ ] **Step 4: Run all tests.**
- [ ] **Step 5: Commit** "AtcCard: the slots of what ATC cleared (D-197)".

### Task 8: The phase readouts and the arrival ATIS

**Files:**
- Create: `src/inflight_readouts.h/.cpp`; add to `blocktime-core`
- Create: `src/altitude_face.h/.cpp` (v2's radio altitude switch, D-203); add to `blocktime-core`
- Modify: `src/departure_conditions.h/.cpp` (arrival ATIS readers; `atisWind` also reads a tower's "Winds 360 at 4"),
  `src/plan_brief.h/.cpp` (STAR, landing runway, reserve, the endurance figures), `src/debrief_measure.h/.cpp` and
  `src/debrief_score.h/.cpp` (the landing's facts and stars, extracted), `src/pricing.h/.cpp` (the crew count,
  extracted)
- Test: `tests/inflight_readouts_test.cpp` and `tests/altitude_face_test.cpp` (add to `blocktime-tests`),
  `tests/departure_conditions_test.cpp`, `tests/plan_brief_test.cpp`, `tests/debrief_score_test.cpp`,
  `tests/pricing_test.cpp`

**Interfaces:**
- Produces (`departure_conditions.h`):
  ```cpp
  // "ILS RWY 30L AND RNAV-Z RWY 30R APCHS IN USE" → "ILS 30L · RNAV-Z 30R"; "" without one. VISUAL is left out.
  std::string atisApproaches(std::string_view atisText);
  // "30L,30R" → "30L 30R"
  std::string runwayList(std::string_view atisRunways);
  ```
- Produces (`plan_brief.h`): `PlanBrief` gains `std::string star, starTransition` (`general.star_ident`,
  `general.star_trans`), `std::string landingRunway` (`destination.plan_rwy`, normalized), `int reserveLb`
  (`fuel.reserve`), and for FUEL REMAINING (v2) `int rampLb` (`fuel.plan_ramp`), `int taxiLb` (`fuel.taxi`) and
  `int enduranceS` (`times.endurance`: seconds, or `"HH:MM:SS"` as Dave's stored OFPs have it). 0 without them.
  ```cpp
  // Fuel on board at the plan's own burn: fuelLb × enduranceS / (rampLb − taxiLb), in whole minutes. nullopt when the
  // plan lacks any of the three (D-203).
  std::optional<int> fuelRemainingMinutes(int fuelLb, const PlanBrief& plan);
  ```
- Produces (`pricing.h`), extracted from `price()` so the panel's crew is the one BlockTime pays:
  ```cpp
  // design.md section 10: pilotsFor(blockHours), plus flightAttendantsFor(seatsInstalled) × vipTimes on an
  // executive airplane (× 1 on an airliner); a freighter carries pilots only.
  int crewFor(std::string_view role, int seatsInstalled, double blockHours, int vipAttendantsTimes);
  ```
- Produces (`altitude_face.h`, D-203):
  ```cpp
  class AltitudeFace {
  public:
      void reset();                  // block out: the radio face (on the ground), armed
      void onSample(double aglFt);   // armed and below 2,000 → radio; above 3,000 → altimeter, and armed again
      void onGoAround();             // the altimeter at once, disarmed
      void onFinalApproach();        // the radio face, armed
      bool radio() const;
  };
  ```
- Produces (`debrief_measure.h`, `debrief_score.h`), moved out of `Measurer::landing()` and `Scorer` so that the
  panel's stars are the debrief's:
  ```cpp
  LandingFacts landingFacts(const nlohmann::json& touchdown, int bounces, std::optional<int> vrefKt,
                            const nlohmann::json* landingRunway);
  int landingStars(const LandingFacts& f, Family family);  // 0-5, D-158
  ```
- Produces (`inflight_readouts.h`):
  ```cpp
  enum class PanelPhase { Departure, Climb, Cruise, Arrival, ShortFinal, Landed };
  // Departure: pushback, taxi_out, takeoff (to liftoff). Climb, Cruise. Arrival: descent, approach, go_around.
  // ShortFinal: from the last final_approach marker (after the last go-around) to touchdown. Landed: from touchdown.
  PanelPhase panelPhase(std::string_view phase, bool onFinal, bool touchedDown);

  struct ReadoutView {
      std::string title, value, unit, tag;  // tag: the aside or source ("ATIS", "PLAN 30R", "SET AT TL")
      bool flagged = false;                 // the amber PLAN flag (D-189)
      bool words = false;                   // Readout's smaller font
      bool wide = false;                    // two columns (APPROACHES, v2)
      bool ticks = false;                   // a value that ticks: never flips (FUEL REMAINING)
      int stars = -1;                       // LANDING only: 0-5
  };
  struct InFlightFacts {  // what the readouts need; the model fills it from the log, the plan and the last sample
      DepartureView departure; ClearanceView clearance;  // Pre-Flight's views (Task 3)
      int cruiseLevelFt = 0;                             // the plan's; 0 = none
      const nlohmann::json* destinationAtis = nullptr;  // the latest `atis` with role destination
      std::string atcStar, atcRunway;                    // from the card: the descend-via STAR, the approach's runway
      std::string towerWind;                             // windText of the landing clearance's wind; ""
      PlanBrief plan;
      int passengers = 0, crew = 0;                      // SOULS (v2)
      int fuelLb = 0;                                    // the last sample's, for FUEL REMAINING (v2)
      int vrefKt = 0, landingFlaps = 0;                  // the cockpit's; 0 = not set
      std::optional<int> stars; int touchdownFpm = 0;   // after touchdown and bounces
      std::string gate;                                  // arrival_gate's `gate` ("Gate 4"); ""
  };
  std::vector<ReadoutView> inFlightReadouts(PanelPhase phase, const InFlightFacts& f);
  ```

The readouts, in order. A readout with no value shows a dash and is still listed, so the grid doesn't jump. **Every
phase starts with SOULS** (`49`, tag `44 PAX + 5 CREW`) **and FUEL REMAINING** (`2:47`, unit `H:MM`, `ticks`; a dash
without the plan's endurance figures) (v2, D-203):

| Phase | Readouts |
|---|---|
| Departure | RUNWAY (`27`, tag `275° · ATC`, flagged as Pre-Flight), SQUAWK, WIND, ALTIMETER |
| Climb | CRUISE (`FL400`) |
| Cruise | CRUISE; DEST ATIS (`JULIETT`, tag `30L 30R`) once a destination ATIS is logged |
| Arrival | ATIS `JULIETT`, RUNWAYS `30L 30R`, APPROACHES `ILS 30L · RNAV-Z 30R`, WIND `Variable / 4 kt`, OAT `17 °C`, ALTIMETER `30.19` unit `inHg` tag `SET AT TL`, CONDITION `DRY` (`WET` with precipitation in the METAR); STAR (the plan's `SILCN6 TROXX`, else ATC's, with tag `PLAN SILCN6` flagged when they differ); LANDING RWY (ATC's approach runway, else the ATIS's first arriving, else the plan's; tag `PLAN 30R` flagged when ATC's differs) |
| ShortFinal | WIND (the tower's, tag `TOWER`; else the ATIS's, tag `ATIS`), RUNWAY (as LANDING RWY), VREF (`129`, unit `KT`, tag `FLAPS 40`). No HEIGHT (v2: the radio altitude dial) |
| Landed | LANDING (stars, tag `−100 FPM`), GATE (`4`, from `Gate 4`) |

- [ ] **Step 1: Write the failing tests:**
  - `AtisReaders.ApproachesAndRunways` on the KSJC ATIS text above.
  - `AtisWind.TowerWords`: "Winds 360 at 4", "Winds calm".
  - `PlanBrief.StarAndLandingRunway` on a brief fixture with `general.star_ident` `SILCN6`, `star_trans` `TROXX`,
    `destination.plan_rwy` `30L` and `fuel.reserve` `2460`. Add these fields to `tests/data/ofp-ksan-ksjc-brief.json`,
    taking the values from Dave's stored OFP for the flight (in `%LOCALAPPDATA%\BlockTime\blocktime.db`,
    `simbrief_plan.ofp_json`).
  - `DebriefScore.LandingStarsMatchScoreFlight`: for the KLAS → KSAN fixture, `landingStars(landingFacts(…))` equals
    `scoreFlight(measureFlight(…)).stars`.
  - `InFlightReadouts.<Phase>`: one per phase row above, with the KSAN → KSJC values.
  - `InFlightReadouts.ArrivalFlagsAtcRunway`: ATC `30R`, plan `30L` → LANDING RWY `30R`, tag `PLAN 30L`, flagged.
  - `InFlightReadouts.CruiseBeforeDestAtis`: DEST ATIS shows a dash.
  - `InFlightReadouts.GoAroundLeavesShortFinal`: `panelPhase("go_around", false, false)` is `Arrival`.
  - `InFlightReadouts.SoulsAndFuelFirstInEveryPhase`: for each phase, the first two are SOULS `49` (tag
    `44 PAX + 5 CREW`) and FUEL REMAINING; without the plan's endurance FUEL REMAINING is blank.
  - `PlanBrief.FuelRemaining`: 13,420 lb with ramp 13,439, taxi 500, endurance `"02:41:00"` → 167 minutes (2:47);
    missing endurance → nullopt.
  - `Pricing.CrewFor`: the BBJ2 (executive, 48 seats, 1.1 h, VIP × 3) → 5; an airliner with 162 seats → 2 + 4 = 6; a
    freighter → 2; 9 hours → 3 pilots. `price()` uses `crewFor` and its tests pass unchanged.
  - `AltitudeFace.*`:
    - `StartsOnRadio`;
    - `ClimbsOutAbove3000`: radio until 3,001;
    - `TerrainBetween2000And3000KeepsTheFace`: the altimeter face stays at 2,400 descending from 4,000, the radio face
      stays at 2,600 after switching at 1,990;
    - `GoAroundShowsAltimeterUntilFinalApproach`: after `onGoAround` at 800 ft, samples of 900 / 1,500 / 1,200 keep
      the altimeter; `onFinalApproach` brings the radio face;
    - `GoAroundThenAbove3000Rearms`: after a go-around and a climb to 3,200, descending through 1,990 brings the radio
      face.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement.** `AtcCard` exposes what Arrival needs: add
  `std::string AtcCard::star() const` (the last descend-via STAR), `std::string AtcCard::approachRunway() const` (the
  last approach or landing runway) and `std::string AtcCard::landingWind() const` (the landing clearance's wind, via
  `atisWind` and `windText`).
- [ ] **Step 4: Run all tests.**
- [ ] **Step 5: Commit** "The in-flight readouts by phase, and the arrival ATIS (D-193, D-194)".

### Task 9: `InFlightModel`, on Live and in the replay window

**Files:**
- Create: `src/inflight_model.h/.cpp` (in the app's Qt sources, next to `preflight_model.*`)
- Modify: `src/app_main.cpp` (create it next to `PreFlightModel` at line 467 and pass it to Live as an initial
  property), `src/replay_session.h/.cpp` (a `Q_PROPERTY(InFlightModel* inflight …)` next to `preflight`)
- Test: `tests/qt_models_test.cpp` (or a new `tests/inflight_model_test.cpp` in `blocktime-qt-tests`)

**Interfaces:**
- Consumes: `RecorderBridge` (`flightEvent`, `blockEvent`, `sampleChanged`, `lastSample()`, `now()`, `flightOpen`),
  `PlannedFlightDisplay` (`view()`, `changed`), `LampBoard`, `AtcCard`, `PreFlightLog`, `inFlightReadouts`.
- Produces (QML properties, all `NOTIFY changed` unless noted):
  ```
  bool visible                 // the bridge's flight is open (block out to block in)
  bool animate                 // false until the first fill is done: QML flips only while true
  QVariantList lamps           // [{name: "BEACON", lens: "red"|"white"|"amber"|"green", lit, wrong}] in Lamp order
  bool flashOn                 // NOTIFY flashChanged: toggles every second while a lamp is wrong; true otherwise
  // Speed: "gs" on the ground below 40 kt, "ias" otherwise (switches at 40 kt both ways)
  QString speedFace; double speedKt; QString machText; double vrefBugKt   // -1 = no bug (Approach to touchdown only)
  double altitudeFt; QString altimeterSetting   // "STD" when both are on STD, else the captain's in the latest ATIS's unit ("30.09")
  QString altitudeFace; double aglFt           // v2: "altimeter" or "radio" (AltitudeFace, fed every sample, go_around and final_approach)
  double vsFpm
  double fuelLb; double fuelScaleLb; double lowFuelLb  // scale: the block-out fuel rounded up to 6,000 lb; arc: the plan's reserve, 0 = none
  QString fuelText             // v2: the fuel gauge's window, "7,410"
  QString atcTitle             // "ATC"
  QVariantList atcSlots        // [{title, value, aside, latest, spacer}], aside already the local time when the slot's is "";
                               // before the takeoff roll a spacer stands in SPEED's place (v2)
  QString phaseTitle           // "DEPARTURE", "CLIMB", "CRUISE", "ARRIVAL", "SHORT FINAL", "LANDED"
  QVariantList readouts        // [{title, value, unit, tag, flagged, words, wide, ticks, stars}]
  ```
- The samples come about once a second, so lamps are re-evaluated on each one: a window that opens with time (beacon
  start + 10 s) needs no timer of its own. The one `QTimer` is the 1 s flash, running only while a lamp is wrong.
- The stars are computed at `bounces` (the touchdown's facts with the bounces), with `landingStars`.
- The crew comes from `crewFor` with the planned flight's role and seats, the plan's block time and the VIP multiple
  from `rates.yaml`: find where New Flight's money preview gets them and pass the same values in (v2).

- [ ] **Step 1: Write the failing tests**, driving a `RecorderBridge` with `applyFlightEvent` / `applySample` /
  `applyBlockEvent` and a fixed clock, as the `PreFlightModel` tests do:
  - `InFlightModel.VisibleFromBlockOutToBlockIn`.
  - `InFlightModel.SpeedFaceSwitchesAt40Kt`: on the ground at 39 kt → `gs`; at 41 kt → `ias`; airborne → `ias`;
    landing rollout at 39 kt → `gs`.
  - `InFlightModel.VrefBugOnlyOnApproach`.
  - `InFlightModel.FlashRunsOnlyWhileWrong`: the timer is inactive with nothing wrong.
  - `InFlightModel.KsanKsjcReplay`: feed the whole fixture through a `ReplayTimeline`. At 19:50:30Z: three `latest`
    slots, `phaseTitle` `ARRIVAL`, and ATIS `JULIETT` in `readouts`.
  - `InFlightModel.FirstFillIsNotAChange`: `animate` is false while a replay catches up on a seek, and true after.
  - `InFlightModel.AltitudeFaceFollowsTheSamples`: radio at block out, altimeter after a sample at 3,100 ft AGL,
    radio after 1,900; a `phase_changed` to `go_around` gives the altimeter, a `final_approach` marker the radio.
  - `InFlightModel.FuelText`: a sample of 7,412.6 lb → `7,410`.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement `InFlightModel`.**
  - Feed `flightEvent`s through the same JSON conversion `PreFlightModel` uses.
  - The flight's family comes from `PlannedFlightView::family` via `familyOf`.
  - Reset the board and the card at block out of a new flight, and on `setFlight` with a new `plannedAt`.
  - `animate` turns true on the first `sampleChanged` after the model saw its first event. Check how
    `ReplaySession` feeds a seek and pick the signal that marks the end of the catch-up (D-184's status lines solved
    the same problem: follow them).
- [ ] **Step 4: Run all tests.**
- [ ] **Step 5: Commit** "InFlightModel: the in-flight panel's data on Live and in replays".

### Task 10: The panel in QML

**Files:**
- Create: `qml/Lamp.qml`, `qml/SteamGauge.qml` (the bezel, black face, ticks, glass and a needle that glides),
  `qml/SpeedGauge.qml`, `qml/AltitudeGauge.qml`, `qml/VsiGauge.qml`, `qml/FuelGauge.qml`, `qml/FlipCard.qml`,
  `qml/InFlightPanel.qml`; add them to the QML module in `CMakeLists.txt`
- Modify: `qml/LivePage.qml` (the panel under `PreFlightPanel`, visible while `inflight.visible`),
  `qml/ReplayWindow.qml`, `qml/Theme.qml`, `qml/Readout.qml` (v2: the tag moves to the title row; a `latest` placard),
  `qml/PreFlightPanel.qml` (only if the tag move needs it)

**Interfaces:**
- Consumes: `InFlightModel`'s properties (Task 9); `InstrumentPanel` and `Readout` (milestone 13).
- Produces:
  - `FlipCard`: `property string key` (the content's signature), `property bool animate`, and
    `signal flipRequested(FlipCard card)` when `key` changes while `animate`.
  - `function flip(delayMs: int)`: a half-turn about the x axis, using `Rotation`'s built-in perspective. The new
    content shows from the midpoint. `Theme.flipDuration` (500 ms).
  - The wave lives in `InFlightPanel`: requests collected in one event-loop turn (`Qt.callLater`) are sorted by x
    (`mapToItem(panel, 0, 0).x`) and flipped at `index * Theme.flipWaveStep` (60 ms).

What flips:
- an ATC slot whose title or value changes;
- a readout whose title or value changes, unless it `ticks`;
- the speed gauge when `speedFace` changes, and the altitude gauge when `altitudeFace` changes (v2).

Not the needles, times or distances.

- [ ] **Step 1: Build the components to the approved v2 mockup.**
  - Faces are drawn once (Qt Quick Shapes, as `RouteMap.qml` does). Only the needle `rotation` changes, with a short
    `Behavior` (`Theme.needleGlide`).
  - The VSI is 0 at nine o'clock, ±6,000 fpm.
  - The speed gauge's ground speed face runs 0–40 kt with a red arc 30–40; the airspeed face runs 0–400 kt with a
    Mach window; the amber bug goes at `vrefBugKt`.
  - The altitude gauge has D-202's three hands and the setting window. Each hand's angle comes from the whole
    altitude (`altitudeFt / 1000 * 360` for the hundreds hand), not a remainder, so a glide across 9 → 0 turns
    forward and doesn't spin back.
  - The fuel gauge's red arc runs from 0 to `lowFuelLb`, and a window under the hub shows `fuelText` (v2).
  - **Radio altitude face (v2, D-203):** 0–3,000 ft with the bottom 500 ft over half of the 300° sweep (`a(h) = h ≤ 500 ?
    h / 500 × 150 : 150 + (h − 500) / 2500 × 150`, from −150°), numbered 0 1 2 3 4 5 10 20 30, `FT × 100` along the
    bottom of the circle, the setting window kept; the needle stops at 3,000.
  - **`Readout` (v2):** the tag (aside, source or flag) sits on the title row, right-aligned; `property bool latest`
    lights the title placard amber. The Pre-Flight panel gets the new look for free.
  - **Layout (v2):** the ATC card across the panel with column weights 1.3 / 1 / 0.6 / 1.3 (a `spacer` entry leaves
    its column empty); the readouts under it, four a row, a `wide` readout spanning two. Gauge and spacing sizes from
    the mockup (gauges about 136 px) go in `Theme.qml`.
- [ ] **Step 2: Wire the panel** on Live and in the replay window, the same way as `PreFlightPanel`.
- [ ] **Step 3: Run qmllint and all tests.** Expected: nothing new from qmllint, all tests pass.
- [ ] **Step 4: Check it on replays** of Dave's KSAN → KSJC leg (`--replay 2026-11-25T18:43:04.053Z --at … --grab`),
  with the header collapsed, at these points (offsets after block out):
  - `block_out`: lamps, the GS face, Departure readouts, ALTITUDE `CLIMB VIA SID`;
  - `taxi_out`: TAXI `A, A7, B, B1`;
  - `0:15`: ALTITUDE `FL220`;
  - `cruise`;
  - `0:42`: descend via, ARRIVAL readouts with `JULIETT`;
  - `0:58`: EXP ILS 30L;
  - `1:10`: the three `LATEST` slots (ATC lines reach the log at the next fetch, a minute late in this 0.11.0 log);
  - `1:15`: short final, the radio altitude face;
  - `taxi_in`: TAXI `K`, HOLD SHORT.
  
  Compare each grab with the mockup, and check the 760 px fit.

  Expected oddity: this log was recorded by 0.11.0, which read the gear as up all flight
  (`docs/notes/msfs-observed-behaviour.md`). So GEAR DN flashes on short final in this replay, as the debrief
  scored it. That is not a lamp bug.
- [ ] **Step 5: Commit** "The in-flight panel: lamps, gauges, the ATC card and the phase readouts (D-194 to D-198)",
  with the grabs that differ from the mockup fixed first.

### Task 11: Close the milestone's build

- [ ] **Step 1: Docs.**
  - `docs/dashboard.md`: an "As built (milestone 14, 0.15.0)" paragraph under "The in-flight panel", with the code
    map and the stills.
  - `docs/debrief.md` (Task 2 already) and `docs/recorder.md` (cockpit lines now reach the observer).
  - `docs/roadmap.md`: "Built in 0.15.0".
  - `docs/decisions.md`: D-201 with every call made while building.
  - `README.md` if any step changed.
  - Version 0.15.0 in `CMakeLists.txt`.
- [ ] **Step 2: Verification** (`superpowers:verification-before-completion`): debug and release build with no
  warnings, all tests pass, qmllint clean.
- [ ] **Step 3: Code review** (`superpowers:requesting-code-review`, then `superpowers:receiving-code-review`). Fix
  what's Critical and Important, with tests.
- [ ] **Step 4: `docs/progress.md`.** What Dave should see on a replay (numbered, expected results), what he should
  see on his next flight, what's unverified (the flips and the flashing by eye; SayIntentions' wording beyond the
  fixtures), and the D-201 calls for him.
- [ ] **Step 5: Commit** "Milestone 14 built: the in-flight panel (0.15.0)". Dave closes it after trying it, then
  `main` is pushed (D-121).
