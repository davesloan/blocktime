# Milestone 11: Flight Replay Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, D-033) to implement this plan
> task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `BlockTime.exe --replay <flight>` opens a read-only window that plays a settled flight on Live at 1×, 2×,
3× or 10×, launched from a Replay button on every Flight Summary.

**Architecture:** Plain C++ in `blocktime-core` reads the flight's log lines, turns them back into what the recorder
sent, and runs a replay clock (timeline with paused stretches removed, speed, pause, seek). A Qt `ReplaySession`
feeds those lines to a fresh `RecorderBridge` on a 50 ms timer. The replay process builds no recorder, no
`AirlineSession` and no writer, and opens the database read-only (D-175).

**Tech Stack:** C++20, MSVC, Qt 6 / QML, nlohmann::json, sqlite3, GoogleTest.

**Spec:** `docs/replay.md` (D-164, D-166, D-167, D-175).

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`; `blocktime-app_qmllint` reports nothing.
- Every new file starts with the GPL header; every QML file with `import BlockTime`, `pragma ComponentBehavior: Bound`
  where it has delegates.
- Every colour, size and font in `qml/Theme.qml`. No continuous animations; the replay timer runs only while playing.
- A replay writes nothing: no event log line, no database write, no SayIntentions, SimBrief or Claude call.
- Nothing on Live reads the wall clock directly (D-166): only the bridge's clock.
- Version 0.12.0.
- Test commands: `cmake --build --preset debug` then `ctest --preset debug` (see `README.md`).

## Review Focus

1. **A lead-in that crosses UTC midnight** (block out at 00:10Z): the lead-in lines are in the day before's file and
   must replay. Test `FlightReplay.LogFilesIncludeTheDayBefore` (Task 3).
2. **Pause flags that change without an unpause** (`paused` 8, then `paused` 1, then `unpaused`,
   `notes/msfs-observed-behaviour.md`): one skipped stretch, measured from the first `paused`. Test
   `FlightReplay.NestedPausesAreOneStretch` (Task 3).
3. **An older or stripped log** (no `mach`, `title`, `lat`, `fuel_lb`, as in `tests/data/flight-klas-ksan/`): zeros,
   not a crash. Tests `TelemetryJson.MissingFieldsAreZero` (Task 1) and `FlightReplay.StrippedFixtureBuildsATimeline`
   (Task 3).
4. **A flight whose log is gone** when the replay window opens (`--replay` with an old id): the window says why
   instead of showing an empty Live. Test `ReplaySession.MissingLogGivesAnError` (Task 7).
5. **A speed change or pause mid-flight**: the clock keeps the offset it reached; only later ticks change. Test
   `ReplayClock.SpeedChangeKeepsOffset` (Task 3).

---

### Task 1: Telemetry and block events read back from JSON

**Files:**
- Create: `src/telemetry_json.h`, `src/telemetry_json.cpp` (in `blocktime-core`)
- Modify: `src/recorder.cpp` (move `toJson(const TelemetrySample&)` and its helpers `engRunning`, `totalFuelFlow` out),
  `src/block_detector.h/.cpp`, `CMakeLists.txt`
- Test: `tests/telemetry_json_test.cpp` (in `blocktime-tests`)

**Interfaces:**
- Produces: `nlohmann::json blocktime::toJson(const TelemetrySample&)` (moved, unchanged output);
  `TelemetrySample blocktime::telemetryFromJson(const nlohmann::json&)`;
  `std::optional<BlockEvent> blocktime::blockEventFromJson(std::string_view type, const nlohmann::json&)` for
  `block_out` and `block_in` only (nullopt otherwise or when `flight_id`/`time` don't parse).

- [ ] **Step 1: Write the failing tests**
  - `TelemetryJson.RoundTrip`: a sample with every logged field set (brake, ground, fuel lb/gal, lat, lon, alt, agl,
    ind alt, vs, gs, ias, heading, 2 engines running [true,false], beacon, strobe, mach 0.78, flaps 40 %, handle 3,
    title, atc id) → `telemetryFromJson(toJson(s))` has the same values; `engCombustion` = {1,0}; `numEngines` 2.
  - `TelemetryJson.MissingFieldsAreZero`: `telemetryFromJson({{"alt_ind_ft", 3454.5}})` → altitude 3454.5, mach 0,
    fuel 0, title "", numEngines 0.
  - `TelemetryJson.BlockEventsRoundTrip`: a `BlockOut` and a `BlockIn` (flight id, times, lat/lon, fuel, shutdown
    time and fuel, block time, parking) → `blockEventFromJson(eventType(e), toJson(e))` equals the original field by
    field; `blockEventFromJson("block_out_refused", …)` is nullopt.
- [ ] **Step 2: Run** `ctest --preset debug -R TelemetryJson` → FAIL (not built).
- [ ] **Step 3: Implement.** Move `toJson(TelemetrySample)` verbatim; `telemetryFromJson` reads each key with
  `value(key, 0)`, `eng_running` into `engCombustion` (up to 4), strings with `copySimString`-style truncation.
  `blockEventFromJson` uses `parseUtc` for times.
- [ ] **Step 4: Run** all tests → PASS (the recorder's own tests still pass: its output is unchanged).
- [ ] **Step 5: Commit** "Telemetry and block events read back from the log (milestone 11)".

### Task 2: Feed lines from core

**Files:**
- Create: `src/feed_text.h`, `src/feed_text.cpp` (core)
- Modify: `src/recorder.cpp` (`flightLine`, the block lines in `logBlockEvent`, the `ARRIVAL GATE` line use them)
- Test: `tests/feed_text_test.cpp`

**Interfaces:**
- Produces: `std::string blocktime::feedText(std::string_view type, const nlohmann::json& data)`: the line text
  without the time ("CLIMB", "LIFTOFF  14R", "LINED UP  runway 14R  0.3 nm", "GSX boarding performing",
  "TOUCHDOWN  120 fpm  1.21 g", "ARRIVAL GATE  D14  (assigned)"); "" for the rest.
  `std::string blocktime::blockFeedText(const BlockEvent&)`: "BLOCK OUT  N814SX  released 18:43:04  fuel 15316 lb",
  "BLOCK IN  brake set 20:01:45  fuel 9876 lb  block 1:18", "NOT RECORDING  …", "FLIGHT INTERRUPTED  (…)".
  The recorder prepends `HH:MM:SS  ` as before.

- [ ] **Step 1: Write the failing tests** `FeedText.FlightEvents` (each example above, plus `gsx_service` `available`
  → "") and `FeedText.BlockEvents` (the four block lines).
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** by moving the existing formatting; recorder output byte-identical.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "Feed lines formatted in core, for the replay (milestone 11)".

### Task 3: The replayer: log files, timeline and clock

**Files:**
- Create: `src/flight_replay.h`, `src/flight_replay.cpp` (core; not `replay.h`, which `tests/` already has)
- Modify: `src/flight_events.h/.cpp` (an overload over a file list)
- Test: `tests/flight_replay_test.cpp`

**Interfaces:**
- Consumes: `telemetryFromJson` (Task 1), `LogLine`, `loadFlightEvents`.
- Produces:
  - `std::vector<LogLine> loadFlightEvents(const std::vector<std::filesystem::path>& files, std::string_view id)`
    (the existing function calls it).
  - `std::vector<std::filesystem::path> replayLogFiles(const std::filesystem::path& dir, std::string_view flightId)`:
    the day files from the day before block out on.
  - `bool replayLogAvailable(const std::filesystem::path& dir, std::string_view flightId)`: block out's day file
    exists.
  - `enum class ReplayRoute { Sample, Block, Flight, Skip }; ReplayRoute replayRoute(std::string_view type)`; the
    skip set is exactly `docs/replay.md`'s list.
  - `struct ReplayItem { std::int64_t offsetMs; TimePoint ts; ReplayRoute route; std::string type;
    nlohmann::json data; TelemetrySample sample; }` (`data` is null for samples, to save memory).
  - `class ReplayTimeline { explicit ReplayTimeline(std::vector<LogLine>); const std::vector<ReplayItem>& items()
    const; bool empty() const; std::int64_t durationMs() const; TimePoint timeAt(std::int64_t offsetMs) const;
    std::optional<std::int64_t> offsetOf(std::string_view at) const; }`. `Skip` lines are dropped except
    `paused`/`unpaused`, which shape the offsets and are then dropped too. `timeAt` is the flight's own time at an
    offset (the last item's `ts` at or before it, plus the difference). `offsetOf` takes `block_out`, a phase name
    (`climb`, `cruise`, … matched against `phase_changed`'s `phase`), or `H:MM` after block out.
  - `class ReplayClock { void setSpeed(double); double speed() const; void setPaused(bool); bool paused() const;
    std::int64_t advance(std::chrono::milliseconds wall); void seek(std::int64_t offsetMs); std::int64_t offset()
    const; }`.

- [ ] **Step 1: Write the failing tests**
  - `FlightReplay.LogFilesIncludeTheDayBefore`: files for 11-23, 11-24, 11-25, 11-26; id `2026-11-25T00:10:00.000Z`
    → 11-24, 11-25, 11-26.
  - `FlightReplay.LogAvailable`: true with the block-out day's file, false without.
  - `FlightReplay.Routes`: `telemetry` Sample; `block_out`/`block_in` Block; `phase_changed`, `gsx_service`,
    `atc_exchange`, `sim_running`, `sim_stopped` Flight; `cockpit`, `probe_gsx`, `paused`, `sim_disconnected`,
    `airport_runways`, `block_out_refused` Skip.
  - `FlightReplay.OffsetsFromTheFirstLine`: lines at t0, t0+1 s, t0+61 s → offsets 0, 1000, 61000; duration 61000.
  - `FlightReplay.PausedStretchIsSkipped`: telemetry t0, `paused` t0+10 s, telemetry t0+12 s, `unpaused` t0+20 min,
    telemetry t0+20 min+5 s → offsets 0, 10000, 15000; `timeAt(15000)` = t0+20 min+5 s.
  - `FlightReplay.NestedPausesAreOneStretch`: `paused` at +10 s, `paused` at +12 s, `unpaused` at +60 s, a line at
    +61 s → offset 11000.
  - `FlightReplay.OffsetOfPhaseAndTime`: `offsetOf("climb")` is the climb `phase_changed`'s offset;
    `offsetOf("block_out")`; `offsetOf("0:30")` = block out + 1 800 000; `offsetOf("nonsense")` nullopt.
  - `FlightReplay.StrippedFixtureBuildsATimeline`: `tests/data/flight-klas-ksan/` for `2026-11-24T23:53:16.837Z` →
    not empty, first item before block out, last item `block_in`.
  - `ReplayClock.AdvancesBySpeed`: speed 10, `advance(100ms)` → 1000.
  - `ReplayClock.PausedDoesNotAdvance`.
  - `ReplayClock.SpeedChangeKeepsOffset`: speed 1 `advance(1s)` → 1000; speed 10 `advance(1s)` → 11000.
- [ ] **Step 2: Run** `ctest --preset debug -R "FlightReplay|ReplayClock"` → FAIL.
- [ ] **Step 3: Implement.** Offsets: walk the lines keeping `pausedSince` (set on the first `paused` while not
  paused, cleared on `unpaused`) and the total paused time; a line's offset is `ts − first ts − paused total`, and
  a line inside a paused stretch gets the stretch's start offset.
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** "The replayer: a flight's log as a timeline and a clock (milestone 11)".

### Task 4: The settled flight as planned, and a read-only database

**Files:**
- Modify: `src/sqlite_db.h/.cpp`, `src/planned_flight.h/.cpp`
- Test: `tests/planned_flight_test.cpp`, `tests/airline_db_test.cpp`

**Interfaces:**
- Produces: `static std::optional<Database> Database::openReadOnly(const std::filesystem::path&, std::string*
  error)` (`SQLITE_OPEN_READONLY`, `PRAGMA foreign_keys = ON`);
  `std::optional<PlannedFlightView> blocktime::flownFlightView(db::Database&, std::string_view logFlightId)` (the
  `flight` row, `flight_cabin`, `flight_plan`, as `currentPlannedFlight` reads the planned one; `loadInParking` from
  `origin_parking`); `std::optional<std::string> blocktime::latestFlightId(db::Database&)` (by `block_in_at`).

- [ ] **Step 1: Write the failing tests**
  - `AirlineDb.ReadOnlyRefusesWrites`: create a file database through the migrations, reopen read-only, a SELECT
    works and an INSERT into `airport` fails.
  - `PlannedFlight.FlownFlightViewMatchesThePlan`: plan a flight with cabins and a stored plan on the test airline,
    take `currentPlannedFlight`, settle it (the settlement test's helper), then `flownFlightView` gives the same
    registration, type, route, cabins, cargo, fuel price, runways and plan fields.
  - `PlannedFlight.LatestFlightId`: two settled flights → the later block in's id; none → nullopt.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement**, sharing the cabin and plan reading with `currentPlannedFlight` (one function taking the
  table and key column).
- [ ] **Step 4: Run** → PASS.
- [ ] **Step 5: Commit** "A settled flight read back as planned; read-only database (milestone 11)".

### Task 5: Live reads its time from the bridge (D-166)

**Files:**
- Modify: `src/recorder_bridge.h/.cpp`, `src/event_feed_model.h/.cpp`, `qml/FlightPanel.qml`, `qml/StatusPill.qml`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Produces: `void RecorderBridge::setClock(std::function<std::chrono::system_clock::time_point()>)` (default
  `system_clock::now`); `std::chrono::system_clock::time_point RecorderBridge::now() const`;
  `Q_INVOKABLE double RecorderBridge::nowMs() const`; `void RecorderBridge::setReplay()` (connection `"replay"`);
  `void EventFeedModel::add(const QString& kind, const QString& text, const QDateTime& at)`.

- [ ] **Step 1: Write the failing tests**
  - `RecorderBridge.FeedUsesTheBridgeClock`: clock fixed at 2026-11-24T23:53:16Z, `applyLine("x", false)` → the
    row's time "23:53:16".
  - `RecorderBridge.ShutdownChecksUseTheBridgeClock`: block out, block in, then a sample with engines off at the
    fixed clock → the shutdown line's time is the fixed clock's, not today's.
  - `RecorderBridge.ReplayConnection`: `setReplay()` → `connection()` "replay".
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** `FlightPanel.qml`'s timer sets `panel.now = panel.recorder.nowMs()`; StatusPill:
  `"replay"` → label "Replay", tone `Theme.accent` (or the closest existing token).
- [ ] **Step 4: Run** tests and qmllint → PASS, quiet.
- [ ] **Step 5: Commit** "Live reads the time from its bridge (D-166)".

### Task 6: A planned flight Live can show without settling it

**Files:**
- Create: `src/planned_flight_display.h/.cpp`, `src/replay_planned_flight.h/.cpp` (app and qt-tests)
- Modify: `src/planned_flight_model.h/.cpp`, `qml/LivePage.qml`, `qml/PlannedFlightPanel.qml`, `CMakeLists.txt`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Produces: `class PlannedFlightDisplay : public QObject` (QML_ELEMENT, uncreatable) with every QML property
  `PlannedFlightModel` has today plus `bool readOnly` (CONSTANT); protected `std::optional<PlannedFlightView>
  flight_`, `Rates rates_`, `QString airlineIcao_`; the formatting getters (`title` … `warning`) implemented once in
  the base; virtual `state()`, `stateText()`, `notRecording()`, and virtual Q_INVOKABLEs `reload`,
  `openDispatchPage`, `importPlan`, `cancelFlight`, `copyRoute`, `doneForNow`, `turnaround` (base: do nothing except
  `copyRoute`). Signals `changed()`, `notRecordingChanged()` in the base.
  `class ReplayPlannedFlight : public PlannedFlightDisplay { ReplayPlannedFlight(std::optional<PlannedFlightView>,
  QString airlineIcao, Rates, RecorderBridge&, QString logFlightId); }`: `readOnly` true; `state()` "flying" while
  the bridge's flight is open, "summary" after its `block_in` (then `hasFlight` false and `summaryFlightId` the id),
  else "replay"; `stateText()` "Replay".
  `LivePage.plannedFlight` and `PlannedFlightPanel.flight` take `PlannedFlightDisplay`.

- [ ] **Step 1: Write the failing tests**
  - `ReplayPlannedFlight.FollowsTheBridge`: a view for XLY1231 KLAS → KSAN; state "replay", title
    "XLY1231 · KLAS → KSAN"; after `applyBlockEvent(BlockOut)` "flying"; after `BlockIn` "summary", `hasFlight`
    false, `summaryFlightId` the id.
  - `ReplayPlannedFlight.IsReadOnly`: `readOnly()` true; `cancelFlight()` and `importPlan()` change nothing.
  - The existing `PlannedFlightModel` tests pass unchanged.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** `PlannedFlightPanel` hides Import plan, Open dispatch page and Cancel when
  `flight.readOnly`.
- [ ] **Step 4: Run** tests and qmllint → PASS, quiet.
- [ ] **Step 5: Commit** "A read-only planned flight for the replay (D-175)".

### Task 7: ReplaySession and its fixture

**Files:**
- Create: `src/replay_session.h/.cpp` (app and qt-tests), `tools/make_replay_fixture.py`,
  `tests/data/flight-ksan-ksjc/events-2026-11-25.jsonl`
- Modify: `tests/data/README.md`, `CMakeLists.txt`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Consumes: Tasks 1–3, 5, 6.
- Produces: `class ReplaySession : public QObject` (QML_ELEMENT, uncreatable):
  `ReplaySession(std::filesystem::path logDir, QString flightId, std::optional<PlannedFlightView>, QString
  airlineIcao, Rates)`; properties `RecorderBridge* bridge` and `PlannedFlightDisplay* plannedFlight` (NOTIFY
  `restarted`), `double speed`, `bool paused`, `bool finished`, `QString title` ("XLY1231 KLAS → KSAN · 24 Nov"),
  `QString error`; Q_INVOKABLE `setSpeed(double)`, `setPaused(bool)`, `restart()`; `bool seek(const QString& at)`;
  `void tick(std::chrono::milliseconds wall)` (the 50 ms timer calls it with the measured elapsed time).
  Routing: Sample → `applySample`; Block → `applyBlockEvent` + `applyLine(blockFeedText)`; Flight →
  `applyFlightEvent` + `applyLine(feedText)` when not empty. The bridge's clock is `timeline.timeAt(clock.offset())`.
  `finished` at the `block_in` item; the timer stops.

- [ ] **Step 1: Make the fixture.** `tools/make_replay_fixture.py <events.jsonl> <flight id> <out>`: the lines
  `loadFlightEvents` would read for Dave's KSAN → KSJC leg (`2026-11-25T18:43:04.053Z`), every field kept, minus
  `pmdg_data` and `probe_*` lines, telemetry thinned to one line in five except within 2 minutes of block out,
  touchdown and block in. Record what was dropped in `tests/data/README.md`. Check the file has no API key or URL
  with a key.
- [ ] **Step 2: Write the failing tests**
  - `ReplaySession.PlaysToBlockIn`: the fixture, `seek("0:00")` then ticks of 10 min at speed 10 until finished →
    `finished`, the bridge's `flightOpen` false, `lastFlightSummary` starts "Block in 1:18", the feed has a
    "BLOCK OUT" row and a "TOUCHDOWN" row, the planned flight's state "summary".
  - `ReplaySession.SeekToCruise`: `seek("cruise")` → the bridge's phase starts "Cruise", `flightOpen` true.
  - `ReplaySession.SpeedAndPause`: speed 10, `tick(1s)` advances 10 s of the flight (the bridge's `now()`); paused,
    `tick(1s)` advances nothing.
  - `ReplaySession.RestartStartsClean`: play to block in, `restart()` → a new bridge with `flightOpen` false,
    `hasSample` false, not finished.
  - `ReplaySession.MissingLogGivesAnError`: an empty log folder → `error` "The log for 25 Nov is no longer on this
    PC", no items.
  - `ReplaySession.WritesNothing`: copy the fixture folder to a temp dir, play to block in → its files' sizes and
    modification times unchanged, no new file.
- [ ] **Step 3: Run** → FAIL.
- [ ] **Step 4: Implement.** `restart()` makes a new bridge and planned flight (the old ones `deleteLater`), with
  `setReplay()` and the clock; `title` from the planned view, or from the block out's `atc_id` without one.
- [ ] **Step 5: Run** → PASS.
- [ ] **Step 6: Commit** "ReplaySession plays a flight's log into a fresh bridge (milestone 11)".

### Task 8: The replay window and `--replay`

**Files:**
- Create: `qml/ReplayWindow.qml`, `qml/ReplayBar.qml`
- Modify: `src/app_main.cpp`, `src/qml_hot_reload.h/.cpp` (root component name), `qml/LivePage.qml`,
  `qml/DebriefPanel.qml`, `qml/Theme.qml`, `CMakeLists.txt` (QML files, version 0.12.0)

**Interfaces:**
- Consumes: `ReplaySession` (Task 7), `Database::openReadOnly`, `flownFlightView`, `latestFlightId` (Task 4).
- Produces: `BlockTime.exe --replay <flight id | last> [--speed N] [--at <phase | block_out | H:MM>] [--grab
  <file>]`. `QmlHotReload(engine, dir, props, QString root = "Main")`. `LivePage.replaying: bool` (hides the summary's
  At Gate Turnaround / Done for now and the Replay button). `DebriefPanel` hides Retry when `reviews` is null.

- [ ] **Step 1: Implement the replay branch in `main`**, before the recorder and airline setup: read settings and
  map settings as now; open `defaultAirlinePaths().database` with `openReadOnly`; resolve `last`; build
  `FlightListModel(db, logDir, airlineIcao)` (no reviews) and `reload()`; load `rates.yaml` read-only for the
  estimate; build `ReplaySession`; apply `--speed` and `--at` (with `--grab`, pause after the seek); load root
  `ReplayWindow` with `replay`, `flights`, map settings, `airlineName`. Nothing else (no `SingleInstance`, no
  recorder thread, no `AirlineSession`, no writers). The airline ICAO comes from `loadAirlineConfig` (read only).
- [ ] **Step 2: Write `ReplayWindow.qml`** (the header: `"Replay · " + replay.title`, the StatusPill; `ReplayBar`;
  `LivePage` with `recorder: replay.bridge`, `plannedFlight: replay.plannedFlight`, `reviews: null`,
  `replaying: true`; `replay.error` shown in place of Live when set) and `ReplayBar.qml` (1× 2× 3× 10× as toggle
  buttons, Pause/Resume, Close; when `replay.finished`: Replay again and Close).
- [ ] **Step 3: Check it.** `BlockTime.exe --replay last --at cruise --grab cruise.png` and `--at block_out`, and
  `--replay <id> --speed 10` watched to block in in the debug build. Expected: Live shows the flight's header
  values, stages and feed with the day's times; the pill reads Replay; at block in the summary with Replay again.
  qmllint quiet, no warnings.
- [ ] **Step 4: Commit** "The replay window: BlockTime.exe --replay (D-175, version 0.12.0)".

### Task 9: The Replay button

**Files:**
- Modify: `src/flight_list_model.h/.cpp`, `qml/FlightSummary.qml`, `qml/FlightsPage.qml`, `qml/LivePage.qml`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Produces: the summary map's `replayNote` ("" when the log exists, else "The log for 24 Nov is no longer on this
  PC"); `Q_INVOKABLE void FlightListModel::replay(const QString& flightId)` (`QProcess::startDetached` of
  `QCoreApplication::applicationFilePath()` with `--replay <id>`); `FlightSummary.replayable: bool` and
  `signal replay()`.

- [ ] **Step 1: Write the failing test** `FlightListModel.ReplayNote`: a settled flight with its day file → "";
  without → "The log for 24 Nov is no longer on this PC".
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** The button sits with the summary's other buttons, greyed with the note under it when
  the note isn't empty; shown on Flights and on Live (not in the replay window).
- [ ] **Step 4: Run** tests and qmllint; check with `--demo --page 1 --grab` that the button shows.
- [ ] **Step 5: Commit** "A Replay button on every Flight Summary (D-164, D-175)".

### Task 10: Docs and close-out

- [ ] `README.md`: the `--replay` command and its options.
- [ ] `docs/replay.md` true to what was built; smaller calls made while building in a `D-176` entry
  (`Status: Proposed`).
- [ ] `docs/progress.md` entry, with what Dave should see on his next look and the unverified items.
- [ ] `requesting-code-review`, then `receiving-code-review`; then commit and push `main` (D-121).
