# Milestone 15: The Predictions, the Passenger Header and Developer Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, D-033) to implement this plan
> task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The header becomes the arrivals board, and the in-flight panel gets the plan's predictions:
- the header's `Arrival 12:35 PM` with `On time` / `Early ~4m` / `Delayed ~4m` under it;
- the header's distance line with the altitude and the speed (`… · FL350 · M.74`) and a top-of-descent tick on the arc;
- TOP OF CLIMB, TOP OF DESCENT, LANDING and FUEL AT LANDING readouts in the climb and cruise;
- the plan's fuel bug on the fuel gauge and the plan's cruise speed bug on the airspeed gauge;
- taxi times from Dave's own flights (D-212).

Last, `--dev` shows Live's old flight tiles and event feed, which are hidden without it (D-211).

**Architecture:** Plain C++ in `blocktime-core`, tests first:
- the navlog's times, fuel and speeds (`parsePlanFixes`) and the OFP's taxi times;
- Dave's taxi history from the database (`taxiHistory`), carried on `PlannedFlightView`;
- one calculation, `predict()`, over a `PlanTimeline` that maps distance along the route to the plan's elapsed time and fuel.

`DashboardHeader` owns the route progress already, so it builds the timeline, calls `predict()` and exposes the
result to C++ (`predictions()`) and its own texts to QML. `InFlightModel` gets the header by reference and fills the
new readouts and bugs from it.

**Tech Stack:** C++20, MSVC, Qt 6 / QML, nlohmann::json, SQLite (sqlite3 C API), GoogleTest.

**Spec:** `docs/dashboard.md`, "The header as the passenger view", "The predictions" and "Gauges"; D-192, D-210,
D-211, D-212. The scope is in `docs/roadmap.md`, "Milestone 15". Claude's calls made while planning are D-213
(Proposed), written with this plan.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`. `blocktime-app_qmllint` reports nothing new.
- Every new file starts with the GPL header. QML: `import BlockTime`, `pragma ComponentBehavior: Bound`.
- Every colour, size and font goes in `qml/Theme.qml` (D-054). Early is `Theme.good`, Delayed is `Theme.bad`.
- No continuous animations. The prediction readouts tick (`ticks = true`): they never flip.
- Nothing on Live reads the wall clock (D-166). Use only `RecorderBridge::now()`.
- Core keeps `TimePoint`s and ISO UTC strings. The Qt layer formats local times (`h:mm AP`).
- Within 2 minutes either way reads `On time` (D-192). Before block out there is no line under the arrival time.
  `~` only before block in.
- Taxi times: the median of the last 5 completed flights at the airport, else SimBrief's (D-212).
- History is read-only here: nothing in this milestone writes to the database.
- Version 0.16.0.
- Test commands: `cmake --build --preset x64-debug`, then `ctest --preset x64-debug`, and the same with `x64-release`
  (`README.md`). 914 tests pass at the start.

## Review Focus

1. **A plan without navlog times** (a trimmed fixture, an old OFP): no predictions. The header falls back to today's
   `blockInEstimate`, without the `~`, and the prediction readouts show a dash. Nothing crashes. Test in Task 3 and
   Task 4.
2. **A hold or vectors:** the distance flown doesn't grow, so the arrival time slips with the clock and `Delayed`
   grows. Test in Task 3.
3. **Flights in the history without phases** (Dave's flights 1–3 have no `flight_phase` rows) **or with a go-around**
   (two `landing` phases): the first are skipped, and the second use the last landing. Test in Task 2.
4. **A replay must not feed on itself:** its taxi history is only flights that blocked in before its block out.
   Test in Task 2.
5. **A long taxi in past the expected one:** the remaining taxi never goes below 0, so the arrival is at least now
   and `Delayed` grows. The same holds for taxi out before liftoff. Test in Task 3.

---

### Task 1: The plan's times, fuel and speeds from the OFP

**Files:**
- Modify: `src/simbrief.h`, `src/simbrief.cpp` (`parsePlanFixes`; `parseNavlog` built on it)
- Modify: `src/plan_brief.h`, `src/plan_brief.cpp` (taxi and enroute times)
- Modify: `tests/data/ofp-ksan-ksjc-navlog.json`, `tests/data/ofp-ksan-ksjc-brief.json` (add the fields)
- Test: `tests/simbrief_test.cpp`, `tests/plan_brief_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct PlanFix {          // simbrief.h
      RoutePoint at;
      std::string ident;    // "TOC", "TOD", "KSJC"
      int elapsedS = -1;    // time_total, from takeoff; -1 = missing
      int fuelLb = -1;      // fuel_plan_onboard; -1 = missing
      int iasKt = 0;        // ind_airspeed; 0 = missing
      std::string stage;    // "CLB", "CRZ", "DSC"
  };
  std::vector<PlanFix> parsePlanFixes(std::string_view ofpJson);  // same fixes, same skips as parseNavlog
  // PlanBrief gains: int taxiOutS = 0, taxiInS = 0, enrouteS = 0;  // times.taxi_out, taxi_in, est_time_enroute
  ```

- [ ] **Step 1: Refresh the fixtures.** Add `time_total`, `fuel_plan_onboard`, `ind_airspeed` and `stage` to each
  fix in `ofp-ksan-ksjc-navlog.json`, and `times.taxi_out`, `times.taxi_in` and `times.est_time_enroute` to
  `ofp-ksan-ksjc-brief.json`. Take them from flight 7's stored OFP (`flight_plan.ofp_json`, KSAN → KSJC, in
  `%LOCALAPPDATA%\BlockTime\blocktime.db`, opened read-only), in the stored `HH:MM:SS` form. Keep the 17 fixes and
  their order. Record the TOC and TOD rows' values in the test as read.
- [ ] **Step 2: Write the failing tests.**
  - `Simbrief.PlanFixesFromNavlog`: 17 fixes; `[5].ident == "TOC"`, `[8].ident == "TOD"`; the last is `KSJC` with
    `elapsedS` equal to `est_time_enroute` in seconds. TOC's `elapsedS`, `fuelLb` and `iasKt` match the fixture.
  - `Simbrief.PlanFixesMatchNavlog`: `parsePlanFixes(x)[i].at` equals `parseNavlog(x)[i]` for every i.
  - `Simbrief.PlanFixesWithoutTimes`: a fix with no `time_total` keeps `elapsedS == -1`. `"00:22:00"` and `"1320"`
    both read as 1320.
  - `PlanBrief.TaxiAndEnrouteTimes`: the fixture's `taxiOutS == 1200`, `taxiInS == 1200` and `enrouteS` as stored.
- [ ] **Step 3: Run them and see them fail.** Run `ctest --preset x64-debug -R "Simbrief|PlanBrief"`. Expected: the
  new tests fail.
- [ ] **Step 4: Implement them.** `parseNavlog` maps `parsePlanFixes` to its positions. Reuse plan_brief's
  seconds-or-`HH:MM:SS` reader for `time_total` (move it to a shared internal helper if it's file-local).
- [ ] **Step 5: Run the tests.** All pass.
- [ ] **Step 6: Commit.** Message: "Read the navlog's times, fuel and speeds, and the OFP's taxi times (D-212)".

### Task 2: Dave's taxi times from the database

**Files:**
- Create: `src/taxi_history.h`, `src/taxi_history.cpp` (add to `blocktime-core` in `CMakeLists.txt`)
- Modify: `src/planned_flight.h`, `src/planned_flight.cpp`
- Test: `tests/taxi_history_test.cpp` (add to `blocktime-tests`), `tests/planned_flight_test.cpp`

**Interfaces:**
- Consumes: `PlanFix`, `parsePlanFixes`, `PlanBrief::taxiOutS/taxiInS` (Task 1).
- Produces:
  ```cpp
  struct TaxiHistory { std::optional<int> outS, inS; };
  // The median taxi out from `originIcao` and taxi in to `arrivalIcao` over the last 5 completed flights each, among
  // flights that blocked in before `beforeInstant` ("" = all). Out: block_out_at to the first `takeoff` phase. In:
  // the last `landing` phase to block_in_at. Flights without that phase are skipped. nullopt where none are left.
  TaxiHistory taxiHistory(db::Database& db, std::string_view originIcao, std::string_view arrivalIcao,
                          std::string_view beforeInstant);
  int medianOf(std::vector<int> values);  // even count: the mean of the middle two, rounded down; values non-empty
  // PlannedFlightView gains:
  std::vector<PlanFix> planFixes;  // the navlog, in route order (route[k + 1] is planFixes[k])
  int taxiOutS = 0, taxiInS = 0;   // D-212: Dave's median, else the plan's; 0 with neither
  ```

- [ ] **Step 1: Write the failing tests** (`TaxiHistory`, on a test database from `tests/test_airline.h`, with
  flights and `flight_phase` rows inserted directly):
  - `MedianOfLastFive`: six flights out of KSAN with taxi outs of 60, 540, 600, 660, 720 and 3000 s. The oldest of
    them (60 s) drops out, so the median is 660.
  - `MedianEvenCount`: two flights, 120 and 180 s, give 150.
  - `SkipsFlightsWithoutPhases`: a flight with no `flight_phase` rows is not counted; with only that one, `outS` is
    nullopt.
  - `GoAroundUsesTheLastLanding`: two `landing` phases 600 s apart; taxi in runs from the second.
  - `OnlyFlightsBefore`: with `beforeInstant` before the newest flight's block in, that flight isn't counted.
  - `InIsByArrivalAirport`: taxi in counts flights whose `arrival_airport_id` is the airport, not the planned
    destination.
  - `PlannedFlightTest.ViewCarriesTaxiTimes`: `currentPlannedFlight` uses Dave's median where there is one and the
    plan's 1,200 s where there isn't. Its `planFixes` has 17 entries.
  - `PlannedFlightTest.FlownViewTaxiTimesExcludeItself`: `flownFlightView` for a flight with a 300 s taxi out, the
    only flight out of its origin, gives the plan's 1,200 s, not 300.
- [ ] **Step 2: Run them and see them fail.** Run `ctest --preset x64-debug -R "TaxiHistory|PlannedFlight"`.
- [ ] **Step 3: Implement them.** Use one query per direction with `strftime('%s', …)` differences, ordered by
  `block_in_at DESC LIMIT 5`. In `readFlightView`, fill `planFixes` next to `route`. `currentPlannedFlight` passes
  `""` and the planned destination. `flownFlightView` passes the flight's own `block_out_at` (add it to its query)
  and its planned destination.
- [ ] **Step 4: Run the tests.** All pass. Add `EXPLAIN QUERY PLAN` to `OnlyFlightsBefore` only if the query scans
  `flight_phase` without its primary key. With 8 flights, it's fine either way.
- [ ] **Step 5: Commit.** Message: "Taxi times from Dave's own flights: the median of the last 5 (D-212)".

### Task 3: The predictions

**Files:**
- Create: `src/predictions.h`, `src/predictions.cpp` (`blocktime-core`)
- Modify: `src/route_progress.h`, `src/route_progress.cpp` (`pointNm`)
- Modify: `src/header_text.h` (the `blockInEstimate` comment: the fallback without navlog times)
- Test: `tests/predictions_test.cpp` (`blocktime-tests`), `tests/route_progress_test.cpp`

**Interfaces:**
- Consumes: `PlanFix` (Task 1), `RouteProgress`.
- Produces:
  ```cpp
  double RouteProgress::pointNm(std::size_t i) const;  // distance along the route to route point i

  struct TaxiTimes { int outS = 0, inS = 0; };

  class PlanTimeline {
  public:
      // fixes[k] sits at route.pointNm(k + 1); the origin (0 NM) is 0 s and takeoffFuelLb. Empty when there are no
      // fixes or the last has no elapsed time or fuel.
      PlanTimeline(const std::vector<PlanFix>& fixes, const RouteProgress& route, int takeoffFuelLb);
      bool empty() const;
      double elapsedS(double nm) const;   // the plan's time from takeoff, linear between fixes, clamped to the route
      double fuelLb(double nm) const;     // the plan's fuel on board, the same way
      int iasKt(double nm) const;         // the next fix's planned IAS; 0 = none
      std::optional<double> fixNm(std::string_view ident) const;  // the first fix with that ident
      int enrouteS() const;               // the last fix's elapsed time
      int landingFuelLb() const;          // the last fix's fuel
  };

  struct FlightNow {
      TimePoint now;
      std::optional<TimePoint> blockOut, liftoff, touchdown;
      double flownNm = 0;
      int fuelLb = 0;
      std::string phase;  // the recorder's code; "" before block out
  };

  struct Predictions {
      std::optional<TimePoint> arrival;    // at the gate
      std::optional<TimePoint> reference;  // from block out: block out + taxi out + enroute + taxi in (D-212)
      std::optional<double> tocNm, todNm;  // to go along the route; nullopt once passed or with no such fix
      std::optional<TimePoint> tocAt, todAt;
      std::optional<TimePoint> landing, planLanding;   // planLanding = reference − taxi in
      std::optional<int> planFuelLb;                   // the fuel gauge's bug, block out to touchdown
      std::optional<int> fuelAtLandingLb, planLandingFuelLb;
      std::optional<int> cruiseSpeedKt;                // the plan's IAS at the next fix, in the cruise phase only
  };
  Predictions predict(const PlanTimeline& plan, TaxiTimes taxi, const FlightNow& f);

  enum class Punctuality { OnTime, Early, Delayed };
  struct PunctualityView { Punctuality kind; int minutes; };  // minutes ≥ 0, rounded
  PunctualityView punctuality(TimePoint arrival, TimePoint reference);  // |difference| ≤ 2 min → OnTime
  std::string punctualityText(const PunctualityView& p, bool final);    // "On time", "Early ~4m", "Delayed 3m"
  ```

- [ ] **Step 1: Write the failing tests** (`Predictions`, on the KSAN → KSJC fixture: `parsePlanFixes` and a
  `RouteProgress` over `parseNavlog`, taxi `{540, 120}`):
  - `RouteProgress.PointNm`: point 0 is 0, the last is `totalNm()`, and the values only go up.
  - `TimelineInterpolates`: halfway between TOC and the next fix by distance, `elapsedS` is halfway between their
    times. Below 0 NM it gives 0, and beyond the end it gives `enrouteS()`.
  - `TimelineEmptyWithoutTimes`: the trimmed fixes (elapsedS −1) give `empty()`.
  - `BeforeBlockOut`: `arrival == now + 540 + enroute + 120`, with no `reference`.
  - `TaxiingOut`: 300 s after block out, `arrival == now + 240 + enroute + 120` and
    `reference == blockOut + 540 + enroute + 120`.
  - `TaxiOutLongerThanExpected` (Review Focus 5): 900 s after block out, still on the ground,
    `arrival == now + enroute + 120`.
  - `Airborne`: flown at TOC's distance, `arrival == now + (enroute − tocElapsed) + 120`, `tocNm` is nullopt and
    `todNm == todDistance − tocDistance`.
  - `HoldSlips` (Review Focus 2): the same `flownNm` 10 minutes later puts `arrival` 10 minutes later.
  - `TaxiInLongerThanExpected` (Review Focus 5): 300 s after touchdown, `arrival == now`.
  - `Fuel`: at TOD with 7,500 lb on board, `planFuelLb` is TOD's planned fuel, and
    `fuelAtLandingLb == 7500 − (todFuel − landingFuel)`. `planFuelLb` is nullopt after touchdown.
  - `CruiseSpeedOnlyInCruise`: `cruiseSpeedKt` is the next fix's `iasKt` in `cruise` and nullopt in `climb`.
  - `PunctualityBands`: ±2 min gives OnTime, 3 min late gives Delayed 3, and 4 min early gives Early 4. The texts are
    `"On time"`, `"Delayed ~3m"` (not final), `"Early 4m"` (final).
- [ ] **Step 2: Run them and see them fail.** Run `ctest --preset x64-debug -R "Predictions|RouteProgress"`.
- [ ] **Step 3: Implement them.** Remaining taxi out = `max(0, taxi.outS − (now − blockOut))` until liftoff, and the
  same for taxi in after touchdown. Before liftoff the TOC and TOD times add the remaining taxi out. `landing` is
  `arrival − taxi in` while airborne, and nullopt after touchdown.
- [ ] **Step 4: Run the tests.** All pass.
- [ ] **Step 5: Commit.** Message: "The predictions: arrival, top of climb and descent, landing and fuel (D-192, D-212)".

### Task 4: The header as the arrivals board

**Files:**
- Modify: `src/dashboard_header.h`, `src/dashboard_header.cpp`
- Modify: `qml/FlightHeader.qml`, `qml/Theme.qml` (the tick's colour and size, if new)
- Test: `tests/qt_models_test.cpp` (`DashboardHeader`)

**Interfaces:**
- Consumes: `predict`, `PlanTimeline`, `punctuality`, `punctualityText` (Task 3); `PlannedFlightView::planFixes`,
  `taxiOutS`, `taxiInS` (Task 2); `isFlightLevel`, `altitudeText`, `effectiveTransitionFt` (`header_text.h`);
  `RecorderBridge::iasKt()`, `showMach()`, `machText()`, `altitudeFt()`, `phaseCode()`.
- Produces:
  ```cpp
  blocktime::Predictions DashboardHeader::predictions() const;  // C++ only, for InFlightModel (Task 5)
  // QML properties (NOTIFY changed):
  QString destinationLine;   // "Arrival 12:35 PM"; "Arrived 12:35 PM · Gate 4" at block in
  QString punctualityText;   // "", "On time", "Early ~4m", "Delayed ~4m"; at block in "Delayed 3m"
  QString punctuality;       // "", "on_time", "early", "delayed": the colour
  QString distanceText;      // "127 NM flown · 103 NM to go · FL350 · M.74" from liftoff to touchdown
  double todShare;           // the TOD fix's share of the route (0..1); -1 without one
  ```

- [ ] **Step 1: Write the failing tests** (on the test bridge and planned flight, with the KSAN → KSJC plan):
  - `DashboardHeader.ArrivalBeforeBlockOut`: `destinationLine` starts with `"Arrival "` with no `~`, and
    `punctualityText` is empty.
  - `DashboardHeader.OnTimeAtBlockOut`: right after block out, `punctualityText == "On time"` and
    `punctuality == "on_time"`.
  - `DashboardHeader.DelayedInAHold`: airborne, with samples held at one position for 10 simulated minutes, it
    reads `"Delayed ~10m"`, or whatever whole minutes the fixture gives, and `punctuality == "delayed"`.
  - `DashboardHeader.ArrivedFreezes`: at block in, `"Arrived h:mm AP · Gate 4"`, and the punctuality text has no `~`.
    It doesn't change when the clock moves on.
  - `DashboardHeader.DistanceLineAltitudeAndSpeed`: in cruise at 35,000 ft and Mach 0.78, the line ends with
    `" · FL350 · M.78"`. In the climb at 12,000 ft and 290 kt below the transition altitude, it ends with
    `" · 12,000 FT · 290 KT"`. Before liftoff it has no altitude or speed.
  - `DashboardHeader.TodShare`: it's between 0 and 1 with the plan, and -1 without a plan.
  - `DashboardHeader.NoNavlogTimesFallsBack` (Review Focus 1): with the trimmed fixes it still shows
    `"Arrival …"` from `blockInEstimate`, and `predictions().tocNm` is empty.
  - The existing tests that expect `"Arriving ~"` are updated to `"Arrival "`.
- [ ] **Step 2: Run them and see them fail.** Run `ctest --preset x64-debug -R DashboardHeader`.
- [ ] **Step 3: Implement them in C++.**
  - Build the `PlanTimeline` in `refreshFlight`. The takeoff fuel is `brief.rampLb − brief.taxiLb`.
  - Track `touchdown_` from the `touchdown` flight event, and clear it in `resetFlight`.
  - `predictions()` fills `FlightNow` from the bridge and `progress_`.
  - Reference against `blockIn_` once blocked in.
  - The timer already ticks each second while the flight is open.
- [ ] **Step 4: Implement them in QML.** In `FlightHeader.qml`:
  - Put the punctuality line under the destination line, coloured `Theme.good` (early), `Theme.bad` (delayed) or
    the line's normal colour (on time).
  - When collapsed, show it small under the destination code, as in the v1 mockup.
  - Draw a short tick across the arc at `todShare`.
- [ ] **Step 5: Run the tests and check the grabs.** All pass, and qmllint is clean. Grab the KSAN → KSJC replay at
  `--at block_out`, `--at cruise` and `--at block_in`, both expanded and collapsed. Look at the line, the colour and
  the tick.
- [ ] **Step 6: Commit.** Message: "The header as the arrivals board: Arrival, On time / Early / Delayed, altitude
  and speed, the TOD tick (D-192, D-210)".

### Task 5: The predictions on the in-flight panel

**Files:**
- Modify: `src/inflight_readouts.h`, `src/inflight_readouts.cpp`
- Modify: `src/inflight_model.h`, `src/inflight_model.cpp`, `src/app_main.cpp`, `src/replay_session.cpp`
- Modify: `qml/InFlightPanel.qml`, `qml/FuelGauge.qml`
- Test: `tests/inflight_readouts_test.cpp`, `tests/qt_models_test.cpp` (`InFlightModel`)

**Interfaces:**
- Consumes: `DashboardHeader::predictions()` (Task 4), `Predictions` (Task 3).
- Produces:
  ```cpp
  // InFlightFacts gains (the Qt model formats the times):
  std::optional<int> tocNm, todNm, fuelAtLandingLb, planLandingFuelLb;
  std::string tocTime, todTime, landingTime, planLandingTime;  // "11:01 AM"; "" = none
  // InFlightModel's constructor gains `DashboardHeader& header` after `planned`.
  // Q_PROPERTY vrefBugKt is renamed speedBugKt: VREF from Approach to touchdown, else the plan's cruise speed in
  // cruise, else -1.
  Q_PROPERTY(double planFuelLb ...)  // the fuel gauge's bug; -1 = none
  ```

- [ ] **Step 1: Write the failing tests.**
  - `InFlightReadouts.ClimbPredictions`: the titles are SOULS, FUEL REMAINING, CRUISE, TOP OF CLIMB,
    TOP OF DESCENT, LANDING.
    - TOP OF CLIMB is `{value "42", unit "NM", tag "11:01 AM", ticks true}`.
    - LANDING is `{value "12:20 PM", tag "PLAN 12:24 PM", ticks true}`.
  - `InFlightReadouts.CruisePredictions`: the titles are SOULS, FUEL REMAINING, CRUISE, DEST ATIS, TOP OF DESCENT,
    LANDING, FUEL AT LANDING, ARR GATE. FUEL AT LANDING is `{value "6,380", unit "LB", tag "PLAN 6,175", ticks true}`.
  - `InFlightReadouts.PredictionsMissing`: with nothing set, the four readouts are listed with an empty value and no
    tag.
  - `InFlightModel.SpeedBug`: VREF on approach; the plan's cruise IAS in cruise; -1 in the climb.
  - `InFlightModel.PlanFuelBug`: a value from block out, and -1 after touchdown.
  - `InFlightModel.PredictionReadoutsFollowTheHeader`: a header tick that changes only the TOD time changes the
    readout's tag.
- [ ] **Step 2: Run them and see them fail.** Run `ctest --preset x64-debug -R "InFlightReadouts|InFlightModel"`.
- [ ] **Step 3: Implement them.**
  - `InFlightModel` listens to `header.changed` and rebuilds only when the facts it takes from `predictions()`
    differ from the last ones.
  - Update both constructions (`app_main.cpp` with the live or demo header, `replay_session.cpp` with its header).
- [ ] **Step 4: Update the QML.** `InFlightPanel.qml` binds `bugKt: panel.model.speedBugKt`. `FuelGauge.qml` gains
  `property real bugLb: -1` and draws the amber bug as `SpeedGauge.qml` does (`Theme.gaugeBug`).
- [ ] **Step 5: Run the tests and check the grabs.** All pass, and qmllint is clean. Grab the replay at `--at climb`
  and `--at cruise`. The readouts fit in two rows of four, and the bugs sit where the plan says.
- [ ] **Step 6: Commit.** Message: "The predictions on the in-flight panel: TOC, TOD, landing, fuel at landing and
  the plan's bugs (D-192)".

### Task 6: Developer mode (`--dev`)

**Files:**
- Modify: `src/app_main.cpp` (the flag, the usage comment, both props maps)
- Modify: `qml/Main.qml`, `qml/ReplayWindow.qml`, `qml/LivePage.qml`

**Interfaces:**
- Produces: the initial property `devMode` (bool) on both windows, passed to `LivePage`'s
  `property bool devMode: false`.

- [ ] **Step 1: Add the flag.** `--dev` sets `bool dev`. It goes into the main window's props and into
  `ReplayOptions`, which carries it to `runReplay`'s props. Add a line to the usage comment at the top of
  `app_main.cpp`.
- [ ] **Step 2: Hide the row without it.** In `LivePage.qml`, the `RowLayout` with `FlightPanel` and `EventFeed`
  gets `visible: page.devMode`. Its `Layout.minimumHeight` only applies when visible. Update the comment, which
  cites D-180, to cite D-211.
- [ ] **Step 3: Build and grab it.** qmllint is clean. Grab Live with `--demo` and with `--demo --dev`. The row shows
  only in the second.
- [ ] **Step 4: Commit.** Message: "Developer mode: --dev shows Live's flight tiles and event feed (D-211)".

### Task 7: Docs, version and close

**Files:**
- Modify: `CMakeLists.txt` (0.16.0), `README.md` (`--dev`), `docs/dashboard.md`, `docs/replay.md` if it lists
  flags, `docs/progress.md`, `docs/decisions.md` (D-213's status after Dave's review)

- [ ] **Step 1: Write the docs.** In `dashboard.md`:
  - mark the predictions, the header and the gauges' bugs as built;
  - replace "Arriving ~" in the header's table;
  - remove the "(M15)" markers in the readouts table.

  Add `--dev` to `README.md`'s run steps.
- [ ] **Step 2: Bump the version.** `project(blocktime VERSION 0.16.0 …)`.
- [ ] **Step 3: Verify.** Use `superpowers:verification-before-completion`:
  - build and test in debug and release, with no warnings;
  - qmllint is clean;
  - check the release app's CPU and memory during a replay at speed 1 (under 1% CPU, under 100 MB).
- [ ] **Step 4: Review.** Use `superpowers:requesting-code-review` on the milestone's commits, then
  `superpowers:receiving-code-review`. Fix what's real, tests first.
- [ ] **Step 5: Log the session.** Write a `progress.md` entry covering what was built, what was verified, what's
  unverified, "on your next flight you should see …", and the decisions waiting on Dave (D-213).
- [ ] **Step 6: Commit.** Message: "0.16.0: milestone 15 docs, version and review fixes". Push `main` only when Dave
  closes the milestone (D-121).
