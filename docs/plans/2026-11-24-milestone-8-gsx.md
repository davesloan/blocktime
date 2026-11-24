# Milestone 8 (GSX) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every GSX service Dave uses shows on Live with its start and end, from catering before departure to
deboarding after arrival. "Ready for shutdown or turnaround" appears with its checklist. Each settled flight stores
its phase times and GSX service times for the leg templates.

**Architecture:**
- **Recorder (plain C++, tested):** `GsxWatcher` turns the GSX L:vars into `gsx_service` log events. SimConnection
  reads them always, in their own definition. Telemetry gains `LIGHT BEACON` and `LIGHT STROBE`.
- **Shared interpretation (plain C++, tested):** `GsxTimeline` turns the log's `gsx_service` and block events into
  service spans and assigns them to legs (D-138). Live uses it, and so does settling.
- **Live:** `FlightStages` owns a `GsxTimeline`, shows the next leg before block out and the last flight until the
  summary is dismissed (D-134), and adds the shutdown checklist (D-135).
- **Database:** migration 5 adds `flight_phase` and `flight_service` (D-137). Settling writes them.
  `recordDeboardings` adds deboarding rows after the fact.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, gtest),
Qt 6 Quick, SimConnect (MSFS 2020 SDK).

**Spec:**
- `docs/roadmap.md`, milestone 8.
- `docs/design.md` §13.5 (stages, GSX services, Ready for Shutdown).
- `docs/recorder.md`, "GSX Pro (D-073)".
- `docs/database.md` (rules, milestone 6's tables).
- `docs/notes/probe-flight.md` (GSX results).
- Decisions: D-072, D-073, D-075, D-099, **D-134..D-138** (written with this plan).

## Global Constraints

- **Build and test:** no warnings at `/W4 /permissive- /utf-8`, release and debug. `cmake --build --preset x64-release`,
  then `ctest --preset x64-release` all pass. `cmake --build --preset x64-debug --target blocktime-app_qmllint`
  reports nothing.
- **Files:** every new file starts with the GPL header. QML files `import BlockTime`, use
  `pragma ComponentBehavior: Bound`, and take every colour, size and font from `Theme.qml`. `blocktime-core` and its
  tests include no Qt, `windows.h` or `SimConnect.h`.
- **Recorder budget:**
  - the GSX request is its own definition, `PERIOD_SECOND` + `CHANGED`;
  - the main telemetry gains only `LIGHT BEACON` and `LIGHT STROBE` (both `bool`, `Int32`; accepted on both families
    in the probe flights).
- **GSX L:vars, in this order (the struct's order):**
  - `FSDT_GSX_CATERING_STATE`, `FSDT_GSX_REFUELING_STATE`, `FSDT_GSX_BOARDING_STATE`, `FSDT_GSX_DEBOARDING_STATE`;
  - `FSDT_GSX_DEPARTURE_STATE`, `FSDT_GSX_NUMPASSENGERS_BOARDING_TOTAL`, `FSDT_GSX_NUMPASSENGERS_DEBOARDING_TOTAL`;
  - all with units `"number"`.
- **States:** 1 available, 2 not_available, 3 bypassed, 4 requested, 5 performing, 6 completed. Service names in the
  log and database: `catering`, `refuelling`, `boarding`, `deboarding`, `pushback`.
- **Copy:**
  - Pre-Departure lines: "Catering", "Refuelling", "Boarding";
  - Departure: "Pushback (GSX)" when GSX pushed;
  - Arrival: "Deboarding", "Ready for shutdown or turnaround", with checklist items "Engines off", "Parking brake
    set", "Deboarding complete" (only if GSX deboarding started), "Beacon off" and "Strobes off".
- **Version:** 0.9.0.
- **Happy path for config (D-099);** care goes to the sim and the money.

## Review Focus

1. **GSX not installed or not running** (every L:var 0): nothing is logged, Live behaves as in 0.8.0, Pre-Departure
   ends with no lines. Tested in Task 1 (`IgnoresZeros`) and Task 3 (`NoServicesPreDepartureEmptyAndDone`).
2. **BlockTime restarted mid-service:** the first value read is 5. It must be logged so the span starts there. Task 1
   (`FirstPerformingIsLogged`).
3. **Pushback requested while catering is still performing:** GSX sets catering to 3. It must vanish from Live and
   never be stored. Task 2 (`AbandonedSpanIsRemoved`), Task 5 (`AbandonedServiceNotStored`).
4. **Dave plans the next flight while deboarding:** the card moves on, but the deboarding row must still reach the
   settled flight. Task 6 (`DeboardingAfterNewPlanStillRecorded`).
5. **Two refuels in one turnaround:** both stored, not one overwriting the other. Task 5 (`TwoRefuelsBothStored`).

---

### Task 1: `GsxWatcher` and the recorder's GSX request

**Files:**
- Create: `src/log_event.h` (move `LogEvent` out of `flight_tracker.h`, which includes it).
- Create: `src/gsx_watcher.h`, `src/gsx_watcher.cpp` (in `blocktime-core`).
- Modify: `src/sim_connection.h/.cpp`: definition and request, callback.
- Modify: `src/recorder.cpp`: own a `GsxWatcher`, write its events. Tell the observer about `sim_running` and
  `sim_stopped` (also on disconnect).
- Modify: `CMakeLists.txt`.
- Test: `tests/gsx_watcher_test.cpp`.

**Interfaces:**
- Produces:

  ```cpp
  #pragma pack(push, 1)
  struct GsxValues {
      double catering, refuelling, boarding, deboarding, pushback;  // states
      double boardedPax, deboardedPax;
  };
  #pragma pack(pop)

  class GsxWatcher {
  public:
      std::vector<LogEvent> onValues(const GsxValues& v, TimePoint now);
      void reset();  // a new sim session: forget the last states
  };
  ```

- Event `gsx_service`: `{"service", "state", "time"}`, plus `"passengers"` (int) for boarding and deboarding.
- Observer gets `onFlightEvent("sim_running" | "sim_stopped", {"time": iso})`. The recorder doesn't write them again:
  SimConnection already logs `sim_running`/`sim_stopped`/`sim_disconnected`.

- [ ] **Step 1: Write the failing tests** in `tests/gsx_watcher_test.cpp`:
  - `IgnoresZeros`: all zeros → `onValues` returns empty, twice.
  - `FirstAvailableIsNotLogged`: all states 1 → empty.
  - `LogsEachStateChange`: refuelling 1 → 4 → 5 → 6 → 1 → four events, states `requested`, `performing`,
    `completed`, `available`, `service == "refuelling"`, with no `passengers` key.
  - `BoardingCarriesPassengers`: boarding 1 → 5 with `boardedPax` 12 → one event, `passengers == 12`.
  - `PassengerChangeAloneIsNotLogged`: boarding stays 5 while `boardedPax` goes 12 → 13 → empty.
  - `ZeroFlickerKeepsState`: boarding 5, then all 0, then boarding 5 → no event after the first.
  - `FirstPerformingIsLogged`: the first values have boarding 5 → one `performing` event.
  - `ResetForgetsStates`: boarding 5, `reset()`, boarding 5 → logged again.
- [ ] **Step 2: Run** `ctest --preset x64-release -R GsxWatcher`. Expected: build fails (no `gsx_watcher.h`).
- [ ] **Step 3: Implement** `GsxWatcher`. Values are rounded to int. A value outside 1..6 changes nothing. Then wire
  SimConnection: `DEF_GSX_SERVICE` and `REQ_GSX_SERVICE` ids that don't clash with the probe's (101+), registered on
  every session, and a `std::function<void(const GsxValues&)>` callback. Add it to `setFlightCallbacks` as a fourth
  argument. The recorder calls `writeFlightEvents(gsx.onValues(v, now))`, and `gsx.reset()` when the sim stops or the
  session ends. Feed line (`flightLine`): `HH:MM:SS  GSX <service> <state>` for requested, performing and completed
  only.
- [ ] **Step 4: Run** the build and `ctest --preset x64-release`. Expected: all pass, no warnings.
- [ ] **Step 5: Commit** "Read GSX's service states always and log gsx_service (D-073, D-138)".

### Task 2: `GsxTimeline`, the services and their legs

**Files:**
- Create: `src/gsx_timeline.h`, `src/gsx_timeline.cpp` (in `blocktime-core`).
- Test: `tests/gsx_timeline_test.cpp`.

**Interfaces:**
- Produces:

  ```cpp
  struct ServiceSpan {
      std::string service;           // catering, refuelling, boarding, deboarding
      std::string startIso, endIso;  // endIso "" while running
      bool running = true;
  };

  class GsxTimeline {
  public:
      // gsx_service, block_out, block_in, flight_interrupted, sim_stopped, sim_disconnected; others ignored.
      // Times come from data "time" (gsx_service, block events) or the caller's `ts` for sim_* (pass it in data as
      // "time").
      void onEvent(std::string_view type, const nlohmann::json& data);
      const std::vector<ServiceSpan>& nextLeg() const;      // catering/refuelling/boarding not yet taken by a block out
      const std::vector<ServiceSpan>& departed() const;     // the completed ones taken at the last block out
      const std::vector<ServiceSpan>& arrival() const;      // deboarding since the last block in
      bool pushbackByGsx() const;                           // pushback requested or performing at the last block out
      const std::string& lastBlockInFlightId() const;       // "" before any block in, and after a block out
  };
  ```

- Rules (D-138):
  - performing starts a span;
  - 6 or 1 completes it;
  - 2 or 3 removes it;
  - `block_out` moves the completed `nextLeg` spans to `departed`, drops the running ones, and clears `nextLeg` and
    `arrival`;
  - `block_in` clears `nextLeg` and `arrival`;
  - deboarding counts only between a block in and the next block out;
  - `flight_interrupted` clears all three lists;
  - `sim_stopped`/`sim_disconnected` clear `nextLeg` and drop running `arrival` spans.

- [ ] **Step 1: Write the failing tests:**
  - `RefuelCompletesAtCompleted`: 4 at 10:00:00, 5 at 10:01:00, 6 at 10:05:00, 1 at 10:05:10 → one `nextLeg` span,
    start 10:01:00, end 10:05:00, not running.
  - `BoardingCompletesOnAvailable`: 5 → 1 → completed, end at the 1's time.
  - `RequestedAloneIsNoSpan`: 4 → 1 → no spans.
  - `BypassedIsNoSpan`: 1 → 3 → 1 → no spans.
  - `AbandonedSpanIsRemoved`: catering 5, then 3 → no spans.
  - `BlockOutTakesCompletedServices`: refuel completed, boarding running, `block_out` → `departed` has refuelling
    only; `nextLeg` empty.
  - `ServicesAfterBlockInAreTheNextLegs`: `block_out`, `block_in`, refuel completed → `nextLeg` has it; `departed`
    unchanged.
  - `DeboardingBelongsToArrival`: `block_in` (flight_id "F1"), deboarding 4/5/6 → `arrival` one completed span,
    `lastBlockInFlightId() == "F1"`; `nextLeg` empty.
  - `DeboardingBeforeAnyBlockInIgnored`: deboarding 5/6 with no `block_in` → `arrival` empty.
  - `SimStoppedClearsNextLeg`: refuel completed, `sim_stopped` → `nextLeg` empty.
  - `InterruptedDropsServices`: refuel completed, `block_out`, `flight_interrupted` → all empty.
  - `PushbackByGsx`: pushback 4 then 5, then `block_out` → true. Without pushback events → false.
  - `KslcTurnaround`: the 2026-11-24 N916SX sequence (from `events-2026-11-24.jsonl`):
    - refuelling 4 at 05:32:59, 5 at 05:34:35, 6 at 05:39:42, 1 at 05:39:53;
    - catering 4 at 05:33:24, 5 at 05:35:33, 6 at 05:39:23, 1 at 05:39:33;
    - boarding 4 at 05:38:32, 5 at 05:38:57, 6 at 05:47:24, 1 at 05:47:35;
    - at 05:48:41 catering, refuelling, boarding and deboarding 3, pushback 5 (no 4 was seen);
    - `block_out` at 05:49:17, `block_in` at 07:48:20;
    - deboarding 4 at 07:49:32, 5 at 07:49:56, 6 at 07:52:47, 1 at 07:53:00.
    - Expect `departed` = refuelling 05:34:35–05:39:42, catering 05:35:33–05:39:23 and boarding 05:38:57–05:47:24;
      `arrival` = deboarding 07:49:56–07:52:47; `pushbackByGsx()`.
- [ ] **Step 2: Run** `ctest --preset x64-release -R GsxTimeline`. Expected: build fails.
- [ ] **Step 3: Implement** `GsxTimeline`.
- [ ] **Step 4: Run** `ctest --preset x64-release`. Expected: all pass.
- [ ] **Step 5: Commit** "Turn gsx_service events into services and give each one its leg (D-138)".

### Task 3: `FlightStages`: GSX lines, the next leg and the shutdown checklist

**Files:**
- Modify: `src/flight_stages.h/.cpp`.
- Test: `tests/flight_stages_test.cpp`.

**Interfaces:**
- Consumes: `GsxTimeline` (Task 2).
- Produces:

  ```cpp
  struct ChecklistItem { std::string text; bool done; };
  struct StageLine { ...; std::vector<ChecklistItem> checks; };  // only the Ready line has checks
  struct ShutdownChecks { bool enginesOff, parkingBrakeSet, beaconOff, strobeOff; };

  class FlightStages {
  public:
      void onEvent(std::string_view type, const nlohmann::json& data);  // now also block_out, block_in,
                                                                        // flight_interrupted, sim_*, gsx_service
      bool onShutdownChecks(const ShutdownChecks& c, const std::string& iso);  // true if the view changed
      void showNextLeg();          // the summary was dismissed: drop the last flight's lines
      bool showingFlight() const;  // from block out until showNextLeg() or an interruption
      bool hasNextLegServices() const;
      // view() and phaseLabel() as before; reset() is removed (block_out now starts a fresh flight itself)
  };
  ```

- **Behaviour:**
  - `view()` while not `showingFlight()`: Pre-Departure is Current with the `nextLeg()` spans; the other stages are
    Upcoming with no lines.
  - While `showingFlight()`: Pre-Departure lists `departed()`, and Arrival adds `arrival()`'s deboarding line and the
    Ready line.
  - Ready appears once after block in, at the first time either GSX deboarding completes, or all four checks pass
    while no deboarding is running.
  - Its `checks` always show the latest `ShutdownChecks`. "Deboarding complete" is listed only once GSX deboarding has
    started.
  - `onShutdownChecks` before block in, or while not `showingFlight()`, changes nothing and returns false.

- [ ] **Step 1: Write the failing tests** (update the existing ones that used `reset()`):
  - `NextLegServicesShowBeforeBlockOut`: refuel performing → Pre-Departure Current, one line "Refuelling", running.
  - `DepartedServicesKeptAfterBlockOut`: refuel completed, `block_out`, `phase_changed` pushback → Pre-Departure Done
    with "Refuelling" start–end; Departure has "Pushback".
  - `NoServicesPreDepartureEmptyAndDone`: `block_out` + pushback → Pre-Departure Done, no lines.
  - `PushbackByGsxLabel`: pushback 5, `block_out`, `phase_changed` pushback → the Departure line reads
    "Pushback (GSX)".
  - `DeboardingAndReadyOnCompletion`:
    - `block_in`, `phase_changed` arrived, deboarding 5 then 6;
    - Arrival lines are "Parked", "Deboarding" and "Ready for shutdown or turnaround", the Ready line at the 6's time;
    - its checks have the five items in the copy order, "Deboarding complete" done.
  - `ReadyOnChecksWithoutGsx`: arrived, `onShutdownChecks({true, true, true, true}, t)` → returns true, Ready at t, four
    items (no deboarding item).
  - `ReadyWaitsForRunningDeboarding`: arrived, deboarding 5, all checks pass → no Ready line; then deboarding 6 →
    Ready.
  - `ChecksUpdateLive`: Ready shown with beacon on, then beacon off → "Beacon off" done, returns true. The same checks
    again → returns false.
  - `ShowNextLegDropsTheFlight`: a flight to arrived, catering completed after block in, `showNextLeg()` → Departure to
    Arrival have no lines; Pre-Departure Current with "Catering".
  - `AbandonedLineDisappears`: catering 5 then 3 → Pre-Departure has no lines.
- [ ] **Step 2: Run** `ctest --preset x64-release -R FlightStages`. Expected: failures / build errors.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `ctest --preset x64-release`. Expected: all pass.
- [ ] **Step 5: Commit** "Live's stages show GSX services and the shutdown checklist (D-134, D-135, D-136)".

### Task 4: Beacon and strobe telemetry, the bridge and Live

**Files:**
- Modify: `src/sim_vars.h` (two `std::int32_t` fields `lightBeacon`, `lightStrobe` and their specs, epsilon 0).
- Modify: `src/recorder.cpp` (`toJson`: `"beacon"`, `"strobe"` bools).
- Modify: `src/recorder_bridge.h/.cpp`:
  - feed block events and `sim_*` into `stages_`, instead of `reset()`;
  - `applySample` sends `ShutdownChecks{enginesRunning == 0, parkingBrake, !beacon, !strobe}`;
  - new `Q_PROPERTY(bool showingFlight …)` and `Q_PROPERTY(bool hasNextLegServices …)` on `stagesChanged`;
  - `Q_INVOKABLE void showNextLeg()`;
  - each line's map gains `checks: [{text, done}]`.
- Modify: `src/planned_flight_model.cpp`: call `bridge_.showNextLeg()` wherever the summary is cleared (`doneForNow`,
  `turnaround`, `reload` with a flight).
- Modify: `qml/LivePage.qml`: FlightProgress `visible: recorder.showingFlight ? (recorder.flightOpen ||
  page.showSummary) : (plannedFlight.hasFlight || recorder.hasNextLegServices)`.
- Modify: `qml/FlightProgress.qml`: under a line with checks, one row per item, a tick or an open circle, and the text
  (done in `Theme.textSecondary`, not done in `Theme.textPrimary`). Any new size goes in `Theme.qml`.
- Modify: `src/app_main.cpp` (`--demo`): the made-up departure gets refuelling and boarding done before pushback.
- Test: `tests/qt_models_test.cpp`.

**Interfaces:**
- Consumes: `FlightStages` (Task 3).

- [ ] **Step 1: Write the failing tests:**
  - `RecorderBridge.GsxServicesBeforeBlockOut`: a `gsx_service` refuelling performing flight event →
    `hasNextLegServices()`, `!showingFlight()`, stage 0 has one line.
  - `RecorderBridge.ArrivalStaysUntilShowNextLeg`:
    - block out, block in, arrived, then a sample with engines off, brake set, beacon and strobe off;
    - stage 5 has the Ready line with four checks;
    - `showNextLeg()` → `!showingFlight()`.
  - `PlannedFlightModel.DoneForNowShowsNextLeg`: after a settled flight's summary, `doneForNow()` →
    `bridge.showingFlight()` false.
- [ ] **Step 2: Run** `ctest --preset x64-release`. Expected: the new tests fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the release build, `ctest`, and qmllint. Expected: all clean. Check by eye:
  - `build\x64-debug\BlockTime.exe --demo` shows Pre-Departure's two lines and "Pushback (GSX)";
  - `--demo --grab` for the record.
- [ ] **Step 5: Commit** "Live's progress card spans the gate; telemetry logs beacon and strobe (D-134, D-135)".

### Task 5: Migration 5 and storing the times at settling

**Files:**
- Modify: `src/airline_db.cpp` (`kMigration5`, appended to `kMigrations`).
- Modify: `src/flight_settlement.h/.cpp` (`FlownFlight` gains the times; `findFlownFlight` collects them; `settleFlight`
  inserts them).
- Modify: `docs/database.md` (milestone 8's tables, the entity diagram, the processes).
- Test: `tests/airline_db_test.cpp`, `tests/flight_settlement_test.cpp`.

**Interfaces:**
- Consumes: `GsxTimeline` (Task 2).
- Produces:

  ```cpp
  struct PhaseStart { std::string phase, at; };                     // at: whole seconds
  struct ServiceTime { std::string service, startedAt, endedAt; };  // whole seconds
  struct FlownFlight { ...; std::vector<PhaseStart> phases; std::vector<ServiceTime> services; };
  ```

- Schema (STRICT, the same `BT_INSTANT` GLOB check as `flight.block_out_at` on every time):

  ```sql
  CREATE TABLE flight_phase (
    flight_id INTEGER NOT NULL REFERENCES flight ON DELETE CASCADE,
    seq INTEGER NOT NULL CHECK (seq >= 0),
    phase TEXT NOT NULL CHECK (phase IN ('pushback','taxi_out','takeoff','climb','cruise','descent','approach',
                                         'go_around','landing','taxi_in','arrived')),
    started_at TEXT NOT NULL,
    PRIMARY KEY (flight_id, seq)
  ) STRICT;
  CREATE TABLE flight_service (
    flight_service_id INTEGER PRIMARY KEY,
    flight_id INTEGER NOT NULL REFERENCES flight ON DELETE CASCADE,
    service TEXT NOT NULL CHECK (service IN ('catering','refuelling','boarding','deboarding')),
    started_at TEXT NOT NULL,
    ended_at TEXT NOT NULL,
    CHECK (ended_at >= started_at),
    UNIQUE (flight_id, service, started_at)
  ) STRICT;
  ```

  The UNIQUE and the primary key both lead with `flight_id`, so the foreign key is indexed. CASCADE as on
  `flight_cabin`: only tests delete flights.
- `findFlownFlight` widens its line filter to `phase_changed`, `gsx_service`, `sim_stopped` and `sim_disconnected`. It
  feeds every block, `gsx_service` and `sim_*` event to one `GsxTimeline` in log order (`sim_*` lines carry no data
  time, so it passes the line's `ts` as `"time"`). At the matching `block_out` it copies `departed()`. It collects
  `phase_changed` with the attempt's `flight_id` in log order.

- [ ] **Step 1: Write the failing tests:**
  - `AirlineDb.Migration5Tables`: an empty database migrated to 5 has both tables, and `user_version == 5`. A
    `flight_phase` row with phase `'boarding'` fails its CHECK, and a `flight_service` row ending before it starts
    fails. Extend the existing FK-index / `EXPLAIN QUERY PLAN` checks if they list tables.
  - `FlightSettlement.SettlingStoresPhases`:
    - a log with `block_out`, `phase_changed` pushback, taxi_out, takeoff, climb, cruise, descent, approach,
      go_around, approach, landing, taxi_in, arrived (12 phases), `block_in`;
    - `flight_phase` has 12 rows, seq 0..11, in that order, times in whole seconds.
  - `FlightSettlement.SettlingStoresPreDepartureServices`: refuel and boarding completed before `block_out` → two
    `flight_service` rows with their whole-second times.
  - `FlightSettlement.AbandonedServiceNotStored`: catering 5 → 3 before block out → no row.
  - `FlightSettlement.EarlierLegServicesNotStored`: a refuel before an earlier flight's `block_in`, then this flight
    → no refuel row on this flight.
  - `FlightSettlement.ServicesBeforeSimStoppedNotStored`: refuel completed, `sim_stopped`, reload, `block_out` → no
    rows.
  - `FlightSettlement.TwoRefuelsBothStored`: two completed refuels before block out → two rows.
- [ ] **Step 2: Run** `ctest --preset x64-release`. Expected: the new tests fail.
- [ ] **Step 3: Implement** the migration, the scan and the inserts in `settleFlight`'s transaction.
- [ ] **Step 4: Run** `ctest --preset x64-release`. Expected: all pass. Then copy Dave's `blocktime.db` to a scratch
  folder and open it with the release app's data folder pointed there (or a small test harness). Check that migration
  5 runs on real data with no error in `errors.log`.
- [ ] **Step 5: Commit** "Store each flight's phase and GSX service times (D-137)".

### Task 6: Deboarding after the flight is settled

**Files:**
- Modify: `src/flight_settlement.h/.cpp`.
- Modify: `src/planned_flight_model.cpp` (call it at startup after `checkFlown`, and on a `gsx_service` flight event
  with service `deboarding` and state `completed` or `available`).
- Test: `tests/flight_settlement_test.cpp`, `tests/qt_models_test.cpp`.

**Interfaces:**
- Consumes: `GsxTimeline::arrival()`, `lastBlockInFlightId()` (Task 2).
- Produces:

  ```cpp
  // Adds each completed GSX deboarding in the log to the settled flight whose block_in it followed. Rows that exist
  // are skipped (INSERT OR IGNORE on the UNIQUE). Returns the rows added.
  int recordDeboardings(db::Database& db, const std::filesystem::path& logDir,
                        std::size_t maxFiles = kOpenFlightLookbackFiles);
  ```

- [ ] **Step 1: Write the failing tests:**
  - `DeboardingAfterBlockInRecorded`: a settled flight, then deboarding 5/6 in the log after its `block_in` → returns
    1, one `deboarding` row.
  - `DeboardingRecordedOnce`: run twice → the second returns 0, still one row.
  - `DeboardingForUnsettledFlightSkipped`: a `block_in` whose flight_id isn't in `flight` → returns 0.
  - `DeboardingAfterNewPlanStillRecorded`: a settled flight, a new `planned_flight` row inserted, then deboarding
    completes → the row goes on the settled flight.
- [ ] **Step 2: Run** `ctest --preset x64-release`. Expected: the new tests fail.
- [ ] **Step 3: Implement.** Each completed `arrival()` span is inserted for `lastBlockInFlightId()` when the span
  completes, so a deboarding is tied to the block in before it.
- [ ] **Step 4: Run** `ctest --preset x64-release`. Expected: all pass.
- [ ] **Step 5: Commit** "Add the deboarding to the settled flight when it ends (D-137)".

### Task 7: Docs, version and the milestone review

**Files:**
- `CMakeLists.txt` (0.9.0).
- `docs/recorder.md` ("GSX Pro" as built: the event, always read, the rules; beacon and strobe in telemetry).
- `docs/design.md` (§13.5: the card spans the gate, no Turnaround and Boarding line, the checklist's items; §13.8:
  services of an interrupted flight).
- `docs/database.md` (done in Task 5).
- `docs/roadmap.md` (milestone 8 built).
- `docs/notes/probe-flight.md` (the 777 boarding goes 5 → 6, unlike the 737's 5 → 1; plain `LIGHT BEACON`, `LIGHT
  STROBE` and `CABIN SEATBELTS ALERT SWITCH` follow the 777; `APU SWITCH` stayed 0).
- `README.md` (if anything changed in the steps).
- `docs/progress.md`.

- [ ] **Step 1: Update the docs** listed above, and mark D-073's "Turnaround and Boarding" part superseded by D-136.
- [ ] **Step 2: Clean release and debug builds;** `ctest` and qmllint. Expected: no warnings, all pass.
- [ ] **Step 3: Whole-milestone review** (`requesting-code-review`, then `receiving-code-review`). Fix each finding
  that holds, with a test that fails first.
- [ ] **Step 4: Progress entry** with Dave's sim checks: one flight, ordered to keep MSFS restarts to a minimum (a GSX
  turnaround with catering, refuelling, boarding, a GSX push, then deboarding on arrival).
- [ ] **Step 5: Commit** "Milestone 8: GSX (0.9.0)". Don't push until Dave's sim checks are done (D-121).
