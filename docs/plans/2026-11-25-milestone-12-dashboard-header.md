# Milestone 12: The Dashboard Header Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, D-033) to implement this plan
> task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Live gets the dashboard's header: the identity row, the route arc coloured by altitude, the phase pill and
three status lines. Below it are the planned flight panel (or the Flight Summary after block in), the flight tiles and
the event feed. The real app and the replay window both get it.

**Architecture:** The logic is plain C++ in `blocktime-core`, written tests first. It covers:
- the city and the navlog on `PlannedFlightView`;
- `RouteProgress` and `AltitudeArc`;
- the flight-level rule and the header's text helpers;
- the pill's sections;
- a `StatusLines` class fed the same events as `FlightStages`.

`RecorderBridge` owns a `StatusLines` next to its `FlightStages`. A new Qt object, `DashboardHeader`, combines the
bridge, the planned flight and the settled flight into QML properties. A new `DashboardHeader.qml` draws them, with
every colour and size in `Theme.qml`.

**Tech Stack:** C++20, MSVC, Qt 6 / QML (Qt Quick Shapes for the arc), nlohmann::json, sqlite3, GoogleTest.

**Spec:** `docs/dashboard.md`: "The header", "The status lines" and "Milestone 12: how the header is built"
(D-150 to D-154, D-180 to D-182).

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`; `blocktime-app_qmllint` reports nothing.
- Every new file starts with the GPL header. Every QML file has `import BlockTime`, plus `pragma ComponentBehavior:
  Bound` and `required` properties in delegates.
- Every colour, size and font goes in `qml/Theme.qml` (D-054). That includes the arc's four colour stops: red
  `#EF4444` at 0, yellow at 1/3, green `#22C55E` at 2/3 and blue `#3B82F6` at 1. Core returns shares, not colours.
- No continuous animations. The header's one timer ticks once a second, and only while the block clock runs or the
  plan's block-in estimate shows.
- Nothing on Live reads the wall clock directly (D-166). Use only `RecorderBridge::now()`.
- Times on the dashboard are Dave's local time, `h:mm AP` (`1:33 PM`). Core keeps ISO UTC strings, and the Qt layer
  formats them.
- Database rules (CLAUDE.md): migration 9 runs in a transaction and is tracked by `user_version`. The tests build an
  empty database through every migration.
- Version 0.13.0.
- Test commands: `cmake --build --preset x64-debug`, then `ctest --preset x64-debug`, and the same with `x64-release`
  (`README.md`).

## Review Focus

1. **A flight with no SimBrief plan:**
   - the route is the straight line between the airports;
   - the status line reads `Climbing`;
   - Block in shows `--:--`;
   - colours are scaled to 35,000 ft, and flight levels start at 18,000 ft.

   Tests: `RouteProgress.NoPlanIsTheStraightLine` (Task 3), `HeaderText.NoPlanDefaults` (Task 4) and
   `StatusLines.ClimbWithoutPlan` (Task 6).
2. **A SID or STAR that doubles back near a fix flown earlier:** the marker never jumps back or ahead. Test
   `RouteProgress.DoublingBackNeverGoesBack` (Task 3).
3. **The app starts with a settled flight's summary already showing** (no planned flight cached): the header is filled
   from `flownFlightView`. Test `DashboardHeader.SummaryAtStartupUsesSettledFlight` (Task 8).
4. **A go-around and a second approach:**
   - the lines run `Go-around` → `Approach into San Jose` → `Final approach, runway 30L` → `Landed on runway 30L`;
   - the pill's approach division is current through all of it.

   Tests: `StatusLines.GoAroundAndSecondApproach` (Task 6) and `PhasePill.GoAroundIsApproach` (Task 5).
5. **A flight interrupted mid-air (sim quit):** the status lines drop the flight's lines and show the next leg's again,
   and the arc, distance and pill reset. Tests: `StatusLines.InterruptedClearsTheFlight` (Task 6) and
   `DashboardHeader.InterruptResetsTheArc` (Task 8).

---

### Task 0: Baseline on the laptop

The laptop's `build/x64-debug` holds only a `CMakeCache.txt`.

- [ ] **Step 1:** `cmake --preset x64-debug`, `cmake --build --preset x64-debug`, then `ctest --preset x64-debug`.
  Expect 686 tests to pass with no warnings. The release build already exists; build and test it the same way.
- [ ] **Step 2:** If either fails for a laptop reason (Qt path, SimConnect SDK), record the fix in
  `docs/notes/qt-setup.md` and `README.md`. No commit if nothing changed.

### Task 1: Airports get a city (migration 9, D-181)

**Files:**
- Modify: `src/airline_db.cpp` (`kMigration9`, `kMigrations`)
- Modify: `src/airport_import.h/.cpp` (`ImportedAirport::city`, the `municipality` column, the upsert)
- Modify: `src/planned_flight.h/.cpp` (`originCity`, `destinationCity` on `PlannedFlightView`, filled by
  `currentPlannedFlight` and `flownFlightView`)
- Modify: `docs/database.md`, `README.md` (re-run `blocktime-import` once after updating)
- Test: `tests/airline_db_test.cpp`, `tests/airport_import_test.cpp`, `tests/planned_flight_test.cpp`

**Interfaces:**
- Produces: `std::string PlannedFlightView::originCity, destinationCity`. Each is the airport's `city`, or its `name`
  when the city is empty, so callers never test for "".

- [ ] **Step 1: Write the failing tests:**
  - `AirlineDb.Migration9AddsCity`: a new database has `user_version` 9, and `airport.city` defaults to `''`.
  - `AirportImport.ReadsMunicipality`: a CSV row with `municipality` "San Diego" gives `city` "San Diego". Writing it
    twice, the second time with "San Diego CA", updates the row.
  - `PlannedFlight.ViewHasCities`: with KSAN (city "San Diego") and KLAS (city ""), the view has `originCity` "San
    Diego" and `destinationCity` set to KLAS's name. `flownFlightView` gives the same.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "Migration9|Municipality|ViewHasCities"` → FAIL.
- [ ] **Step 3: Implement.**
  - `kMigration9`: `ALTER TABLE airport ADD COLUMN city TEXT NOT NULL DEFAULT '';`
  - The import reads `municipality` (OurAirports always has it; the column list is in `notes/airport-data.md`), and the
    upsert sets `city = excluded.city`.
  - The views' existing airport join selects `city` too.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "Airports get a city for the dashboard's header (D-181, migration 9)".

### Task 2: The plan's navlog becomes the route

**Files:**
- Create: `tests/data/ofp-ksan-ksjc-navlog.json`
- Modify: `src/simbrief.h/.cpp` (`RoutePoint`, `parseNavlog`)
- Modify: `src/planned_flight.h/.cpp` (`PlannedFlightView::route`, filled from `ofp_json` by both views)
- Modify: `docs/notes/simbrief-api.md` (the navlog's fields)
- Test: `tests/simbrief_test.cpp`, `tests/planned_flight_test.cpp`

**Interfaces:**
- Produces:
  - `struct RoutePoint { double lat = 0, lon = 0; };`
  - `std::vector<RoutePoint> blocktime::parseNavlog(std::string_view ofpJson)`. It returns the fixes in order. It
    accepts `navlog` as an array of fixes (how Dave's stored plans have it) or as `{"fix": [...]}`. `pos_lat` and
    `pos_long` are strings. It skips fixes whose position doesn't parse, and returns empty for no navlog or bad JSON.
  - `std::vector<RoutePoint> PlannedFlightView::route`: the origin airport, the navlog's fixes, then the destination
    airport; with no plan, only the two airports.

- [ ] **Step 1: Make the fixture.** Copy the KSAN → KSJC flight's `ofp_json` from
  `%LOCALAPPDATA%\BlockTime\blocktime.db`. That's the flight in `tests/data/flight-ksan-ksjc/`, and its log id is in
  that log's `block_out`. Use a throwaway program in the scratchpad, linked to `blocktime-core` and not committed;
  there's no sqlite3 shell on the laptop. Cut it down to these fields only:
  - `origin.icao_code`, `destination.icao_code`;
  - `general.initial_altitude`;
  - `navlog` with `ident`, `pos_lat`, `pos_long` and `stage`.

  Remove `params.user_id`, `fetch` and everything else.
- [ ] **Step 2: Write the failing tests:**
  - `Simbrief.NavlogFromStoredPlan`: the fixture gives more than 3 points. The first is near KSAN (within 30 NM of
    32.73, -117.19) and the last is near KSJC (within 30 NM of 37.36, -121.93).
  - `Simbrief.NavlogUnderFix`: `{"navlog":{"fix":[{"pos_lat":"1.5","pos_long":"2.5"}]}}` gives one point (1.5, 2.5).
  - `Simbrief.NavlogMissingIsEmpty`: `{}` and `not json` both give empty.
  - `PlannedFlight.ViewHasRoute`: with a plan, `route` is the origin, the fixes, then the destination. Without one,
    it's the two airports.
- [ ] **Step 3: Run** → FAIL.
- [ ] **Step 4: Implement** `parseNavlog` with `nlohmann::json::parse(..., nullptr, false)`. The views read `ofp_json`
  in the query they already run for the plan.
- [ ] **Step 5: Run** all tests → PASS. Write the navlog's fields in `notes/simbrief-api.md`.
- [ ] **Step 6: Commit** "The stored plan's navlog becomes the dashboard's route (D-182)".

### Task 3: Route progress and the altitude arc

**Files:**
- Create: `src/route_progress.h`, `src/route_progress.cpp` (core); add to `CMakeLists.txt`
- Test: `tests/route_progress_test.cpp`

**Interfaces:**
- Consumes: `RoutePoint` (Task 2) and `distanceMeters` (`geo.h`).
- Produces:
  - `class RouteProgress { explicit RouteProgress(std::vector<RoutePoint> route); double totalNm() const; double
    flownNm() const; double toGoNm() const; double share() const; void update(double lat, double lon); void finish();
    void reset(); }`
    - `share()` is flown / total, from 0 to 1.
    - `finish()` sets flown to the total.
    - With fewer than 2 points, the total is 0 and the share is 0.
  - `class AltitudeArc { static constexpr int kPieces = 100; void record(double share, double altFt); void reset();
    std::vector<double> peaks() const; }`
    - `peaks()` has `kPieces` entries: the highest altitude recorded in each 1% of the route, or -1 where none is.
  - `double altitudeShare(double altFt, int cruiseAltitudeFt)`: altitude / cruise, clamped to 0..1. It uses 35,000
    when the cruise altitude is 0.

- [ ] **Step 1: Write the failing tests:**
  - `RouteProgress.FollowsTheRoute`: an L-shaped route (0,0) → (0,1) → (1,1). At (0,0.5), flown is about 30 NM
    (±0.5). At (0.5,1), it's about 90 NM. `toGoNm + flownNm == totalNm`.
  - `RouteProgress.NeverGoesBack`: `update` at (0,0.8), then at (0,0.5): flown stays at the 0.8 value.
  - `RouteProgress.DoublingBackNeverGoesBack`: a route that leaves a fix and later passes within 1 NM of it again.
    Positions along the first leg, then near the shared point late in the route, keep the late leg's distance. A
    position near the shared point early on doesn't jump to the late leg.
  - `RouteProgress.NoPlanIsTheStraightLine`: a two-point route KSAN → KSJC is about 362 NM (the great circle, ±2).
  - `RouteProgress.FinishIsTheWholeRoute`.
  - `RouteProgress.KsanKsjcFixture`: the fixture's navlog with the airports, fed every telemetry `lat`/`lon` from
    `tests/data/flight-ksan-ksjc/` between `liftoff` and `touchdown`. Flown never decreases and ends within 5 NM of
    the total.
  - `AltitudeArc.KeepsThePeakPerPiece`: record (0.005, 3000), (0.009, 5000) and (0.5, 33000). `peaks()[0]` is 5000,
    `[50]` is 33000 and `[10]` is -1.
  - `AltitudeArc.ShareScales`: `altitudeShare(11000, 33000)` ≈ 0.333; `altitudeShare(40000, 33000)` is 1;
    `altitudeShare(17500, 0)` is 0.5.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "RouteProgress|AltitudeArc"` → FAIL.
- [ ] **Step 3: Implement.** Leg lengths and cumulative distances come from `distanceMeters`. `update` projects onto
  each leg in a local flat frame centred on the leg's start: x = Δlon·cos(lat)·60, y = Δlat·60, in NM, with t clamped
  to 0..1. It considers only legs from the last matched leg to that leg + 3. It picks the closest of those and keeps
  `flown = max(flown, cumulative[leg] + t·length[leg])`. Without the forward window, test 3 fails.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "Route progress and the altitude arc for the dashboard (D-182)".

### Task 4: The flight-level rule and the header's texts

**Files:**
- Create: `src/header_text.h`, `src/header_text.cpp` (core)
- Modify: `src/flight_tracker.cpp` (`flightLevel` calls the shared rule), `src/flight_stages.cpp` (`altitude` moves
  out)
- Test: `tests/header_text_test.cpp`

**Interfaces:**
- Produces:
  - `bool isFlightLevel(std::string_view phase, int altFt, int transitionAltitudeFt, int transitionLevelFt)`
    - Climbing phases (`takeoff`, `climb`, `cruise`) compare against the transition altitude.
    - Descending phases (`descent`, `approach`, `go_around`) compare against the transition level.
    - A transition of 0 gives false. `FlightTracker::flightLevel` calls it, and its output is unchanged.
  - `int effectiveTransitionFt(int planFt)`: `planFt`, or 18000 when it's 0. The dashboard uses this; the recorder
    doesn't.
  - `std::string altitudeText(int ft, bool flightLevel)`: `FL330`, or `8,500 ft`. Moved from `flight_stages.cpp`.
  - `std::string durationText(std::chrono::seconds s, bool withSeconds)`: `1:05`, or `0:41:10`.
  - `std::optional<TimePoint> blockInEstimate(std::optional<TimePoint> blockOut, TimePoint now, int planBlockS)`. It
    returns nullopt when `planBlockS` is 0, otherwise `(blockOut ? *blockOut : now) + planBlockS`.

- [ ] **Step 1: Write the failing tests:**
  - `HeaderText.FlightLevelRule`:
    - climb at 18,000 with TA 18,000 is true; at 17,900 it's false;
    - descent at 18,000 with TL 18,000 is true; approach at 17,000 is false;
    - TA 0 is false.
  - `HeaderText.AltitudeText`: (33000, true) → `FL330`; (8500, false) → `8,500 ft`; (32960, true) → `FL330`.
  - `HeaderText.Durations`: 3900 s → `1:05`; 2470 s with seconds → `0:41:10`.
  - `HeaderText.BlockInEstimate`: before block out it's now + 3900 s; after, it's block out + 3900 s; with plan 0
    it's nullopt.
  - `HeaderText.NoPlanDefaults`: `effectiveTransitionFt(0)` is 18000, and `isFlightLevel("climb", 18000, 18000, 0)`
    is true.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** Move `altitude` and the `climbing`/`descending` helpers into `header_text.cpp`.
- [ ] **Step 4: Run** all tests → PASS. `FlightTracker` and `FlightStages` tests are unchanged.
- [ ] **Step 5: Commit** "One flight-level rule and the header's texts (D-182)".

### Task 5: The phase pill, and Pre-Departure becomes Pre-Flight

**Files:**
- Create: `src/phase_pill.h`, `src/phase_pill.cpp` (core)
- Modify: `src/flight_stages.h/.cpp` (`kStageNames[0]` becomes "Pre-Flight", plus `const std::string& phase() const`)
- Modify: `tests/flight_stages_test.cpp` (the name)
- Test: `tests/phase_pill_test.cpp`

**Interfaces:**
- Consumes: `StageView`, `StageState` and `FlightStages::phase()`.
- Produces:
  - `struct PillSection { std::string name; StageState state; std::vector<StageState> divisions; };`
  - `std::vector<PillSection> pillSections(const std::vector<StageView>& stages, std::string_view phase)`. It always
    returns six sections.
    - Departure's divisions are `pushback`, `taxi_out`, `takeoff`.
    - Descent's divisions are `descent`, `approach`, `landing`, `taxi_in`, with `go_around` counting as `approach`.
    - The other stages have none.
    - In a Current stage, divisions before the phase are Done, the phase's own is Current and later ones are Upcoming.
    - A Done stage has all its divisions Done; an Upcoming stage has all of them Upcoming.

- [ ] **Step 1: Write the failing tests:**
  - `PhasePill.SixSectionsWithFixedDivisions`: the division counts are 0, 3, 0, 0, 4, 0, and the first name is
    "Pre-Flight".
  - `PhasePill.TaxiOutInDeparture`: Departure's divisions are Done, Current, Upcoming.
  - `PhasePill.GoAroundIsApproach`: phase `go_around` → Descent's divisions are Done, Current, Upcoming, Upcoming.
  - `PhasePill.DoneStageAllDone`: in cruise, Departure's divisions are all Done.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** all tests → PASS (after updating the stage name in the existing tests).
- [ ] **Step 5: Commit** "The phase pill's sections; Pre-Departure becomes Pre-Flight (D-150, D-182)".

### Task 6: The status lines

**Files:**
- Create: `src/status_lines.h`, `src/status_lines.cpp` (core)
- Modify: `src/flight_stages.h/.cpp`. Expose the shared pieces as free functions and keep using them there:
  - `std::vector<ChecklistItem> shutdownChecklist(const ShutdownChecks&, const std::vector<ServiceSpan>& arrival)`
    (today's `readyLine` items);
  - `bool anyRunning(...)`, `bool anyCompleted(...)` and `std::string serviceName(...)`.
- Test: `tests/status_lines_test.cpp`

**Interfaces:**
- Consumes: `GsxTimeline`, `ShutdownChecks`, `isFlightLevel`/`effectiveTransitionFt`/`altitudeText` (Task 4) and
  `shutdownChecklist`.
- Produces:
  - `struct StatusContext { int cruiseAltitudeFt = 0, transitionAltitudeFt = 0, transitionLevelFt = 0; std::string
    destinationCity, departureRunway; };`
  - `struct StatusLine { std::string text, startIso; };`
  - `std::string arrivalGateName(const nlohmann::json& arrivalGate)`: the `arrival_gate` event's `gate` when `result`
    is `assigned` or `unconfirmed`, else its `si_gate` (`arrivalGateText`'s rule). The header uses it too.
  - `class StatusLines` with these members:
    - `void setContext(StatusContext)`;
    - `void onEvent(std::string_view type, const nlohmann::json& data)`, taking the same events as `FlightStages`;
    - `bool onShutdownChecks(const ShutdownChecks&, const std::string& iso)`;
    - `void showNextLeg()`;
    - `std::vector<StatusLine> lines() const` (newest first, at most 3; `{"Preparing aircraft for flight", ""}`
      before anything else);
    - `std::vector<ChecklistItem> checklist() const` (empty until block in, then until `showNextLeg`).

  The wording is exactly the table in `docs/dashboard.md`, "Every status line":
  - "In place" means the newest line's text changes and nothing shifts down.
  - `landing_runway` rewrites the runway in whichever line holds `Landed`.
  - `flight_interrupted` and `sim_stopped` clear the flight's lines, as `FlightStages` does.
  - Ready follows `FlightStages::updateReady`'s rule (D-135, unchanged).

- [ ] **Step 1: Write the failing tests** with made-up events. Each checks `lines()` texts newest first:
  - `StatusLines.PreFlightStartsPreparing`: `Preparing aircraft for flight`.
  - `StatusLines.GsxSharesOneLine`:
    - catering performing → `Catering in progress`;
    - refuelling performing → `Catering in progress · Refuelling in progress`, still one line;
    - catering completed → `Catering complete · Refuelling in progress`.

    A `requested` state alone changes nothing.
  - `StatusLines.DepartureSequence`:
    - pushback, taxi_out (context runway "27"), takeoff, `liftoff` runway "27", climb (cruise 37000, TA 18000) →
      `Climbing to FL370`, `Departed runway 27`, `Takeoff roll`;
    - the second line's `startIso` is the liftoff's time.
  - `StatusLines.LevelOffInPlace`: in climb, `level_off` at 24000 with `flight_level` true → newest is `Level at
    FL240` and the next is still `Departed runway 27`. `level_off_ended` → `Climbing to FL370`.
  - `StatusLines.ClimbWithoutPlan`: context cruise 0 → `Climbing`.
  - `StatusLines.StepClimb`:
    - cruise at FL370, `step_climb` → `Step climb`;
    - `step_climb_ended` at 39000 → `Cruising at FL390`, in place;
    - a step down → `Step descent`, then `Cruising at FL350`.
  - `StatusLines.DescentAndLanding`:
    - descent → `Descending into San Jose`;
    - approach → `Approach into San Jose`;
    - final (the event `FlightStages` uses for its final approach line, runway "30L") → `Final approach, runway 30L`;
    - `touchdown` → `Landed on runway 30L`;
    - taxi_in → `Taxiing to gate`;
    - then `landing_runway` "30R" → the second line reads `Landed on runway 30R` and the newest is unchanged.
  - `StatusLines.GoAroundAndSecondApproach`: final → go_around → approach → final → touchdown →
    `Landed on runway 30L`, `Final approach, runway 30L`, `Approach into San Jose`.
  - `StatusLines.ArrivalGateNames`:
    - `arrival_gate` with result `assigned` and gate "Gate 4" → taxi in reads `Taxiing to gate Gate 4`;
    - with result `kept` and si_gate "Terminal A Gate 4" → `Taxiing to gate Terminal A Gate 4`;
    - `block_in` with parking "Parking 70" → `Parked at Parking 70`.

    Check that wording on the fixtures. If "gate Gate 4" reads badly, drop the word "gate" when the name already
    starts with Gate, and say so in the plan's notes.
  - `StatusLines.ChecklistFromBlockIn`:
    - after `block_in`, `checklist()` has the four items, all not done;
    - checks passing → `Ready for next flight` and every item done;
    - a beacon switched back on afterwards → its item not done, and the lines unchanged;
    - `showNextLeg` → empty checklist, `Preparing aircraft for flight`.
  - `StatusLines.DeboardingThenReady`: GSX deboarding performing → `Deboarding in progress`; completed →
    `Ready for next flight`, with `Deboarding complete` on the line before it. The checklist has the deboarding item.
  - `StatusLines.InterruptedClearsTheFlight`: in climb, `flight_interrupted` → `Preparing aircraft for flight`.
  - `StatusLines.MarkersAreNotLines`: `flight_marker` above_10000 changes nothing.
  - `StatusLines.KsanKsjcFixture`: every event of `tests/data/flight-ksan-ksjc/` in order. The final `lines()` starts
    with `Parked at …`. No line is empty, and no line was empty at any point.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R StatusLines` → FAIL.
- [ ] **Step 3: Implement.** Keep the newest line plus a "current base line" (climb or descent text) so a level-off's
  end can restore it.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "The dashboard's three status lines (D-152, D-182)".

### Task 7: The bridge carries the status lines

**Files:**
- Modify: `src/recorder_bridge.h/.cpp`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Consumes: `StatusLines` (Task 6) and `pillSections` (Task 5).
- Produces, on `RecorderBridge`:
  - `Q_PROPERTY(QVariantList statusLines NOTIFY stagesChanged)`: `[{text, time}]`, newest first. `time` is local
    `h:mm AP`, or "".
  - `Q_PROPERTY(QVariantList checklist NOTIFY stagesChanged)`: `[{text, done}]`.
  - `Q_PROPERTY(QVariantList pill NOTIFY stagesChanged)`: `[{name, state, divisions: [state]}]`, with states as in
    `stages`.
  - `void setStatusContext(const blocktime::StatusContext&)`, which emits `stagesChanged`.
  - `const blocktime::TelemetrySample& lastSample() const`.

  Every place that feeds `stages_` (flight events, block events, GSX, sim session lines, shutdown checks,
  `showNextLeg`) feeds `status_` too.

- [ ] **Step 1: Write the failing test** `RecorderBridge.StatusLinesFollowEvents`: block out, the pushback phase, then
  `liftoff` → `statusLines[0].text` is `Departed runway 14R`, `pill[1].state` is "current" and `checklist` is empty.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "RecorderBridge gives Live the status lines, checklist and pill".

### Task 8: DashboardHeader

**Files:**
- Create: `src/dashboard_header.h`, `src/dashboard_header.cpp` (app and qt-tests, next to `planned_flight_display`)
- Modify: `src/planned_flight_display.h` (public `const std::optional<blocktime::PlannedFlightView>& view() const`)
- Modify: `src/app_main.cpp`, `src/replay_session.h/.cpp` (create one each and pass it to QML)
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Consumes: `RecorderBridge` (Task 7), `PlannedFlightDisplay::view()` and `summaryFlightId`, `RouteProgress` and
  `AltitudeArc` (Task 3), and the header texts (Task 4).
- Produces: `class DashboardHeader : public QObject` (QML_ELEMENT, uncreatable):
  - Constructor: `DashboardHeader(RecorderBridge&, PlannedFlightDisplay&, QString airlineIcao, QString airlineName,
    std::function<std::optional<blocktime::PlannedFlightView>(const QString& logFlightId)> settledFlight,
    QObject* parent)`.
    - The app's lookup is `flownFlightView` on its database.
    - The replay's lookup is the view it already holds.
  - `Q_PROPERTY`s, all `QString` unless noted, with one `changed` signal:
    - `bool visible`: a planned flight, an open flight or a summary;
    - `airlineIcao`, `airlineName`, `flightNumber`, `flightText` ("Flight 1231"), `citiesText`;
    - `clockText`, `bool clockIsPlan`, `blockOutText`, `blockInText` (with `~` while it's an estimate);
    - `originIcao`, `originName`, `originLine`, `destinationIcao`, `destinationName`, `destinationLine`;
    - `markerText`, `distanceText`, `double progress`, `QVariantList arcShares` (100 numbers, -1 unflown).
  - The flight comes from `view()` while there is one. It's kept when the state turns `"summary"`, and looked up with
    `settledFlight(summaryFlightId)` when nothing was kept. `RouteProgress` is rebuilt whenever the flight changes.
  - `update` runs on each sample after `liftoff`, and `AltitudeArc` records each sample at the current share.
    `finish()` runs at `block_in`, and `flight_interrupted` resets both.
  - `destinationLine` is `arrivalGateName` (Task 6) of the latest `arrival_gate` event. At `block_in` it becomes
    the event's `parking`.
  - `setStatusContext` is called on the bridge whenever the flight changes. The departure runway is the view's
    `originRunway`, else the plan's.
  - A 1 s `QTimer` runs only while `visible` and (the flight is open, or before block out with a plan).

- [ ] **Step 1: Write the failing tests:**
  - `DashboardHeader.FromPlannedFlight`: a planned flight XLY1231 KSAN → KLAS with plan block 3900 s, before block
    out:
    - `citiesText` is "San Diego to Las Vegas" and `clockText` is "1:05" with `clockIsPlan` true;
    - `blockOutText` is "--:--";
    - `originLine` is "N814SX · 737 BBJ2 · Gate 41" (with the type text `PlannedFlightDisplay` already shows).
  - `DashboardHeader.InFlight`: block out, `liftoff` (runway 27), a sample at a route midpoint at 33,000 ft and
    452 kt:
    - `originLine` starts with "Off " and ends with " · rwy 27";
    - `markerText` is "FL330 · 452 kt";
    - `progress` is between 0.4 and 0.6, and `distanceText` matches `^\d+ NM flown · \d+ NM to go$`.
  - `DashboardHeader.SummaryAtStartupUsesSettledFlight`: a display already in `"summary"` with no view → the fields
    come from the lookup, and `visible` is true.
  - `DashboardHeader.InterruptResetsTheArc`: after samples, `flight_interrupted` → `progress` is 0 and every
    `arcShares` entry is -1.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement.** Local times use `QDateTime::fromStdTimeT(...).toLocalTime().toString("h:mm AP")`.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "DashboardHeader: the header's data for Live and the replay (D-151, D-182)".

### Task 9: The header on Live

**Files:**
- Create: `qml/DashboardHeader.qml`; add it to `CMakeLists.txt` and remove `qml/FlightProgress.qml` from the list
- Delete: `qml/FlightProgress.qml`
- Modify: `qml/LivePage.qml`, `qml/Main.qml`, `qml/ReplayWindow.qml` (a `required property DashboardHeader header`),
  `qml/Theme.qml`
- Modify: `src/app_main.cpp` (`--demo` feeds `liftoff`, the cruise phase and a few samples, so the header shows in
  cruise)

**Interfaces:**
- Consumes: `DashboardHeader` (Task 8) and `RecorderBridge.statusLines`, `checklist` and `pill` (Task 7).

- [ ] **Step 1: Build `DashboardHeader.qml`** in the order `docs/dashboard.md` gives. The identity row has three
  columns, followed by the thin line.
  - **The arc:** a Qt Quick Shapes quadratic arc from origin to destination, drawn as 100 short segments. Each takes
    its colour from `arcShares` through Theme's four stops, blended linearly in QML. The unflown part is a dashed
    `Theme.textMuted` path. The marker sits at `progress` along the curve, with `markerText` above it, and
    `distanceText` under the arc's middle.
  - **The airport blocks:** a Segoe Fluent departure or arrival glyph, the ICAO code, the name (elided) and the line.
  - **The pill:** six equal sections, coloured Theme `good` when done, `warn` when current and `surfaceRaised` when
    upcoming. Thin separators show the divisions, and inside the current section each division takes its own state's
    colour.
  - **The status lines:** three centred lines in the sizes and greys D-152 gives, with `· time` on the older two.
    The checklist sits under them while it isn't empty, reusing `CheckList.qml`.
- [ ] **Step 2: Change `LivePage.qml`** to D-180's layout:
  - the header when `header.visible`;
  - then the Flight Summary loader (after block in) or `PlannedFlightPanel`;
  - then a row of `FlightPanel` and `EventFeed` (`Theme.liveFeedWidth`).

  The stages column is gone.
- [ ] **Step 3: Run** `cmake --build --preset x64-debug --target blocktime-app_qmllint` → nothing reported. Then
  build and run all tests.
- [ ] **Step 4: Look at it** with `set QT_QPA_PLATFORM=offscreen` and `BlockTime.exe --demo --grab demo.png` (cruise).
  Then grab Dave's last flight with `BlockTime.exe --replay last [--at <point>] --grab <png>` at these points:
  - no `--at` (the replay's opening, at the gate before block out, D-177);
  - `taxi_out`;
  - `climb`;
  - `cruise`;
  - `approach`;
  - `block_in`.

  Check the arc, the pill, the lines and the checklist against the sketches. Fix what looks wrong, then grab again.
  Also check that the window at the laptop's usual size still shows the tiles below the header.
- [ ] **Step 5: Commit** "The dashboard header on Live and the replay (milestone 12)".

### Task 10: Docs, version, review and close

- [ ] **Step 1: Docs.**
  - `design.md` sections 13.5 (Pre-Flight; the pill and lines replace the stages column) and 13.7 (the header stays
    at block in).
  - `docs/dashboard.md`: the header and status lines are built.
  - `decisions.md`: mark D-134, D-135 and D-139 "superseded in part by D-152/D-154/D-180".
  - Add an entry for the smaller calls made while building (Proposed).
  - `roadmap.md`: milestone 12 built.
  - `README.md`: re-run the import after updating.
- [ ] **Step 2: Version** 0.13.0 in `CMakeLists.txt`. Build both presets, run all tests and qmllint.
- [ ] **Step 3: Code review** (`requesting-code-review`, then `receiving-code-review`); fix the Important findings.
- [ ] **Step 4: Progress entry** in `docs/progress.md`:
  - what's verified (tests, grabs) and what isn't;
  - "what Dave should see" as numbered steps, including re-running `blocktime-import`.
- [ ] **Step 5: Commit** "Milestone 12 built: the dashboard header (0.13.0)". Push `main` once Dave has tried it and
  closes the milestone (D-121).
