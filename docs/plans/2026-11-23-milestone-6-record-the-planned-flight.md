# Milestone 6 (Record the Planned Flight) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dave flies the planned flight and it counts. The recorder only opens a flight when the sim matches the
plan (aircraft type, origin, fuel). At block in the app settles it: money in the ledger, the airplane moved, fuel and
hours updated. Live then shows a plain summary with At Gate Turnaround and Done for now. An interrupted flight is
discarded with a notice, and the Flights screen lists the settled flights.

**Architecture:**
- **Plain C++ in `blocktime-core`, unit-tested:**
  - the rates file (D-107);
  - the matching check (`flight_match.h`), which `BlockDetector` asks at each brake release;
  - reading the flown flight back from the log and settling it in one transaction (`flight_settlement.h`);
  - fleet slots from hours.
- **The recorder** gets the planned flight through a mutex-guarded `FlightTargetSlot` that the GUI thread sets.
  A refused release is logged as `block_out_refused`. A matched one logs `block_out` with the `planned_flight_id`.
- **Settling reads the log, not the observer (D-060, D-113).** It runs on every block event and at startup, so a crash
  between block in and settling loses nothing. `flight.log_flight_id` is `UNIQUE`, so nothing is ever settled twice.
- **Qt:**
  - `PlannedFlightModel` owns the flight's life on screen: the target, the "Not recording" banner, settling, the
    summary, turnaround.
  - `FlightListModel` lists flights from the database, with tracks still from the log.
- **The probe (Task 9)** sits behind `--probe` and is only for Dave's test flights.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, yaml-cpp,
gtest), Qt 6 Quick + Network + Location, SimConnect (MSFS 2020 SDK).

**Spec:**
- `docs/roadmap.md`, milestone 6, is the scope.
- `docs/design.md`: §6 (slots), §11 (costs, fuel stays in the tanks), §13.4 (matching), §13.7 (after the flight),
  §13.8 (interrupted), §14 (diversions).
- `docs/recorder.md`: "Flight lifecycle", "Matching the planned flight", "Output: local event log".
- `docs/database.md`: the conventions, and "Later milestones". Task 2 replaces that sketch with milestone 6's tables.
- Decisions:
  - D-072 (discard), D-083 (slots), D-088 (match), D-103 (planned flight), D-107 (rates file);
  - **D-108..D-116**, written with this plan.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`, release and debug. All tests pass. `blocktime-app_qmllint` reports nothing.
- **Files:**
  - every new source and QML file starts with the GPL header;
  - QML files `import BlockTime`, use `pragma ComponentBehavior: Bound` and take every colour, size and font from
    `Theme.qml`;
  - `blocktime-core` and its tests include no Qt, `windows.h` or `SimConnect.h`.
- **D-099:** config-file code (`rates.yaml`, `catalog.yaml`, `airline.yaml`) gets the happy path only: a missing
  required key is an error from the existing reader helpers, and nothing more.
- **Database:**
  - the schema exactly as Task 2 writes it into `docs/database.md`;
  - times `YYYY-MM-DDTHH:MM:SSZ`, money in cents, every line a multiple of 500 (D-080);
  - settling is **one transaction**;
  - flights and ledger lines are never updated or deleted.
- **Numbers:**
  - origin radius **3,000 m** from the airport reference point (D-088);
  - fuel slack **300 lb** (D-110);
  - the fuel on record after a flight is `shutdown_fuel_lb` (D-110).
- **Recorder budget:** no new SimVars in the normal build. The probe's extra data definitions are registered only with
  `--probe`.
- **SimBrief:** still fetched only on Dave's click (D-101).
- **Version:** **0.7.0** (`CMakeLists.txt`, `vcpkg.json`).
- **Commits:** straight to `main`, why not what, D-numbers, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Don't push.
- **Commands:**
  - build `cmake --build --preset x64-release`;
  - tests `ctest --preset x64-release`;
  - one suite `build\x64-release\blocktime-tests.exe --gtest_filter=<Suite>.*`, and the Qt suites in
    `blocktime-qt-tests.exe`.

## Review Focus

1. **The app is closed or crashes between block in and settling.** Expected: the next start settles the flight once,
   and Live shows its summary. Tests: Task 5 `Settle.RunsOnceFromTheLog`, Task 7
   `PlannedFlightModel.SettlesAtStartup`.
2. **A planned flight id is reused.** SQLite can reuse a deleted `planned_flight` row id, so an older settled flight
   in the log carries the same id. Expected: the old pair is never taken for the new flight. Test: Task 5
   `FlownFlight.IgnoresAnOlderFlightWithTheSameId`.
3. **An interrupted attempt, then the same planned flight flown again.** Expected: only the second attempt settles,
   and the first leaves nothing in the database. Tests: Task 5 `FlownFlight.InterruptedThenFlownAgain`, Task 7
   `PlannedFlightModel.InterruptedKeepsThePlanWithANotice`.
4. **Brake releases before the sim matches.** The PMDG startup profile, a release before the plan exists, or one
   before refuelling. Expected: each is refused once, and a later matching release opens the flight. Test: Task 3
   `Detector.RefusedReleaseThenMatchingReleaseOpens`.
5. **A diversion.** Expected:
   - the airplane moves to the airport where it parked;
   - fares and the other per-flight lines are as previewed;
   - the summary says "Diverted to …".

   Test: Task 5 `Settle.DiversionMovesToWhereItParked`.

---

### Task 1: Rates file (D-107) and settled-money inputs

**Files:**
- Create: `src/rates.h/.cpp`, `config/rates.yaml`, `tests/rates_test.cpp`
- Modify:
  - `src/pricing.h/.cpp`, `src/airline_config.h/.cpp` (remove the fuel price);
  - `src/airline_session.h/.cpp`, `src/flight_draft.h/.cpp`, `src/planned_flight.cpp`;
  - `src/new_flight_model.h/.cpp`, `src/planned_flight_model.h/.cpp`, `src/app_main.cpp`;
  - `config/airline.example.yaml`.
- Test: `tests/pricing_test.cpp`, `tests/flight_draft_test.cpp`, `tests/qt_models_test.cpp`

**Interfaces:**
- Produces (`rates.h`):
  - `struct Rates`: every constant now at the top of `pricing.cpp`, in **cents** (per-nm and per-lb values as
    `double` cents), plus `Cents fuelPer100Lb`. The defaults equal today's constants, so every existing pricing test
    still passes on `Rates{}`.
  - `std::optional<Rates> loadRates(const std::filesystem::path& file, std::string* error)`
  - `std::filesystem::path defaultRatesFile()`, which is `%LOCALAPPDATA%\BlockTime\rates.yaml`.
- `config/rates.yaml` layout (dollars, commented like `catalog.yaml`). Every key is required:
  ```yaml
  fuel_per_100lb: 41
  landing_per_1000lb_mlw: 4.50
  cargo_handling_per_1000lb: 40
  flight_attendant_per_hour: 55
  narrowbody: { gate: 300, handling: 900, atc_per_nm: 1.00, pilot_per_hour: 200, maintenance_per_hour: 300 }
  widebody:   { gate: 900, handling: 2800, atc_per_nm: 2.00, pilot_per_hour: 300, maintenance_per_hour: 700 }
  catering_per_passenger: { under_3h: 5, 3h_to_8h: 15, 8h_and_over: 35 }
  fares: { economy_base: 90, economy_per_nm: 0.075, business_times_economy: 3, first_times_economy: 5 }
  vip: { fare_base: 180, fare_per_nm: 0.48, pilot_per_hour: 250, flight_attendant_per_hour: 75,
         flight_attendants_times: 3, catering_times: 2 }
  charter: { per_hour: 7500, min_hours: 2, ga_services: 1500 }
  cargo: { base_per_100lb: 15, per_100lb_per_nm: 0.028 }
  ```
- Produces (`pricing.h`):
  - `Preview pricePreview(const PriceInput& in, const Rates& rates)`; the old overload goes.
  - `PriceInput::actualBlockHours`, a `std::optional<double>`. When set, it replaces `blockHours` for pilots (and
    their count), flight attendants and maintenance only (D-109).
  - `MoneyLine::category`, a `std::string` holding the `ledger_category` code Task 2 adds: `economy_fares`,
    `business_fares`, `first_fares`, `vip_fares`, `charter`, `cargo`, `fuel`, `landing_fee`, `gate`, `handling`,
    `ga_services`, `atc`, `catering`, `cargo_handling`, `pilots`, `flight_attendants`, `maintenance`.
- `AirlineConfig::fuelPriceCentsPer100Lb` and `AirlineSession::fuelPriceCentsPer100Lb()` are removed.
- `FlightDraft` takes a `Rates` instead of the fuel price, and locks `rates.fuelPer100Lb` on the planned flight.
- `NewFlightModel(db, airlineIcao, ratesFile, planner)` reads `rates.yaml` in `open()`. If it can't, the dialog shows
  the reader's error and Plan flight stays disabled.
- `PlannedFlightModel` gains a `ratesFile` constructor argument and reads it in `reload()` for the estimate. Task 7
  adds more arguments.

- [ ] **Step 1: Write the failing tests:**
  - `Rates.RepoFileMatchesTheDefaults`: `loadRates("config/rates.yaml")` equals `Rates{}`, field by field.
  - `Rates.MissingKeyNamesFileAndKey`: a copy without `fuel_per_100lb` gives nullopt, and the error contains
    `rates.yaml` and `fuel_per_100lb`.
  - `Pricing.ActualBlockHoursOnlyChangeCrewAndMaintenance`: the same 737 passenger input with `blockHours = 2.0`,
    with and without `actualBlockHours = 3.5`. Pilots, flight attendants and maintenance scale to 3.5 h. Fares,
    catering (still the under-3 h band), landing, gate, handling and ATC are unchanged.
  - `Pricing.LinesCarryTheirCategory`: a VIP preview's lines are `vip_fares`, `landing_fee`, … in order.
  - `Pricing.RatesAreUsed`: doubling `rates.fuelPer100Lb` doubles the Fuel line.
- [ ] **Step 2:** Run `blocktime-tests --gtest_filter=Rates.*:Pricing.*`. Expected: they don't compile, since
  `rates.h` is missing.
- [ ] **Step 3:** Implement `rates.h/.cpp` with the existing `config_yaml.h` helpers. Move the constants out of
  `pricing.cpp` and thread `Rates` through every caller listed above. `app_main` passes `defaultRatesFile()`. Remove
  `fuel_price_per_100lb` from `airline.example.yaml` and the reader. The tests read `config/rates.yaml` from the repo,
  like `catalog.yaml` (`CMakeLists.txt` line 131's comment).
- [ ] **Step 4:** Run all tests. Expected: PASS. Fix the existing pricing, draft and Qt tests to pass `Rates{}`, without
  changing any expected amount.
- [ ] **Step 5: Docs:**
  - README "Set up your airline": copy `config\rates.yaml` to `%LOCALAPPDATA%\BlockTime\`, and remove
    `fuel_price_per_100lb` from `airline.yaml`;
  - design.md §11: the rates live in `rates.yaml`, read when New Flight opens;
  - D-104/D-107 need no new entry.
- [ ] **Step 6: Commit:** "Rates move to rates.yaml so Dave can tune them without rebuilding (D-107)".

### Task 2: Schema version 4 and the catalog's sim folders

**Files:**
- Modify: `src/airline_db.h/.cpp`, `src/catalog.h/.cpp`, `src/leasing.cpp` (type sync), `config/catalog.yaml`
- Modify: `docs/database.md` (replace the milestone 6 sketch: tables, diagram, processes)
- Test: `tests/airline_db_test.cpp`, `tests/catalog_test.cpp`

**Interfaces:**
- Produces: `kAirlineSchemaVersion = 4`, migration 4:
  - `ALTER TABLE aircraft_type ADD COLUMN sim_folder TEXT NOT NULL DEFAULT ''`. It's written by the type sync at
    every start (D-093), so the default never survives a start.
  - **`flight`**, one settled flight, written once:
    - `flight_id` INTEGER PK;
    - `log_flight_id` TEXT `UNIQUE` `<> ''`: the event log's `flight_id`;
    - `airplane_id` FK `RESTRICT`;
    - `flight_number` (digits, 1–4);
    - `kind` (the same `CHECK` as `planned_flight`);
    - `origin_airport_id`, `planned_destination_airport_id` and `arrival_airport_id`, each FK `RESTRICT`;
    - `cargo_lb` ≥ 0;
    - `fuel_price_cents_per_100lb` > 0;
    - `planned_at`, `block_out_at` and `block_in_at`, as instants, with `CHECK (block_in_at > block_out_at)`;
    - `fuel_on_record_lb`, `fuel_out_lb` and `fuel_in_lb`, each ≥ 0;
    - `origin_runway` and `destination_runway`, TEXT NULL (the ATIS runways, as on `planned_flight`).

    Indexes on the four FKs and on `block_in_at` (the Flights list sorts by it). Block time and fuel added aren't
    stored: they follow from the columns (D-114).
  - **`flight_cabin`**: `flight_id` FK `CASCADE` (only for tests: flights are never deleted), `cabin_class` and
    `passengers`, as `planned_flight_cabin`. Primary key `(flight_id, cabin_class)`.
  - **`flight_plan`**: every `simbrief_plan` column, with `flight_id` as PK and FK `RESTRICT` in place of
    `planned_flight_id`. Absent when Dave flew without importing.
  - **`flight_money_line`**: `ledger_line_id` PK FK `RESTRICT` and `flight_id` FK `RESTRICT`, indexed (the shape of
    `lease_charge`).
  - The 17 `ledger_category` rows from Task 1, labelled as the preview labels them ("Economy fares", "Fuel", …).
- Produces (`catalog.h`): `CatalogType::simFolder`, from a required `sim_folder` key. `config/catalog.yaml` values
  (D-112, read from Dave's Community folder on 2026-11-23):

  | Type | `sim_folder` |
  |---|---|
  | 737-600 | `PMDG 737-600` |
  | 737-800 | `PMDG 737-800` |
  | 737-800 BBJ2 | `PMDG 737-800BBJ2` |
  | 737-800 BCF | `PMDG 737-800BCF` |
  | 777-200ER | `PMDG 777-200ER` |
  | 777-300ER | `PMDG 777-300ER` |
  | 777F | `PMDG 777F` |

- [ ] **Step 1: Write the failing tests:**
  - `AirlineDb.EmptyDatabaseMigratesToVersion4`: rename the version-3 test.
  - `AirlineDb.Version3DatabaseMigratesTo4`: build to 3 with `migrateAirlineDatabaseTo`, then migrate. Expected:
    version 4, and `aircraft_type.sim_folder` exists.
  - `AirlineDb.RestoredVersionStillGetsMigration4`: extend the existing restore test.
  - `AirlineDb.FlightChecks`: each of these inserts fails:
    - `block_in_at` ≤ `block_out_at`;
    - a duplicate `log_flight_id`;
    - a negative `fuel_out_lb`;
    - a `kind` of `ferry`;
    - a `flight_money_line` for a missing flight.
  - `AirlineDb.FlightTablesIndexTheirForeignKeys`: extend the existing foreign-key index test, if there is one, to
    the four new tables.
  - `AirlineDb.FlightLedgerCategoriesExist`: all 17 codes.
  - `Catalog.TypesHaveSimFolders`: the repo catalog's 737-800 BBJ2 has `PMDG 737-800BBJ2`.
- [ ] **Step 2:** Run the suites. Expected: FAIL (version 3, no tables, no `simFolder`).
- [ ] **Step 3:** Write migration 4 and the catalog key. The type sync writes `sim_folder` with the rest.
- [ ] **Step 4:** Run all tests. Expected: PASS.
- [ ] **Step 5: Docs.** `docs/database.md`:
  - the status line says schema version 4;
  - a "Milestone 6's tables (schema version 4)" section with the four tables in the house format;
  - `sim_folder` on `aircraft_type`;
  - diagram edges: `airplane ||--o{ flight`, three `airport` edges, `flight ||--o{ flight_cabin`,
    `flight ||--o| flight_plan`, `flight ||--o{ flight_money_line`, `ledger_line ||--o| flight_money_line`;
  - a "Milestone 6's processes" stub that Task 5 fills in.

  README: the upgrade note to copy the new `catalog.yaml` again.
- [ ] **Step 6: Commit:** "Schema 4: flights as history, with their money lines and plan (D-115)".

### Task 3: The matching check and refused releases

**Files:**
- Create: `src/flight_match.h/.cpp`, `tests/flight_match_test.cpp`
- Modify: `src/block_detector.h/.cpp`, `tests/block_detector_test.cpp`, `src/flight_history.cpp` (ignore the new
  event)

**Interfaces:**
- Produces (`flight_match.h`):
  ```cpp
  inline constexpr double kOriginRadiusM = 3000.0;  // D-088
  inline constexpr int kFuelSlackLb = 300;          // D-110
  struct FlightTarget {
      std::int64_t plannedFlightId = 0;
      std::string plannedAt;   // UTC instant; Task 5 ignores older log pairs with the same id
      std::string simFolder, typeCode, registration, originIcao;
      double originLat = 0, originLon = 0;
      int fuelOnRecordLb = 0;
  };
  enum class MatchProblem { None, NoPlannedFlight, WrongAircraftType, WrongAirport, FuelBelowRecord };
  struct MatchResult {
      MatchProblem problem = MatchProblem::NoPlannedFlight;
      std::int64_t plannedFlightId = 0;  // 0 without a target
      std::string aircraftFolder;        // as seen, e.g. "pmdg 737-800bbj2"; "" if not known yet
      double distanceM = 0;              // from the origin's reference point
      int fuelLb = 0, minFuelLb = 0;     // minFuelLb = max(0, fuelOnRecordLb - kFuelSlackLb)
      bool operator==(const MatchResult&) const = default;
  };
  std::string_view problemCode(MatchProblem p);  // "", "no_planned_flight", "wrong_aircraft_type", "wrong_airport", "fuel_below_record"
  std::string aircraftFolder(std::string_view aircraftCfgPath);          // the folder holding aircraft.cfg
  bool folderMatches(std::string_view folder, std::string_view simFolder); // case-insensitive: equal, or simFolder + " " prefix
  MatchResult checkMatch(const std::optional<FlightTarget>& target, const BlockSample& s);  // first problem in enum order
  class FlightTargetSlot {  // set on the GUI thread, read on the recorder thread
  public:
      void set(std::optional<FlightTarget> target);
      std::optional<FlightTarget> get() const;
  private:
      mutable std::mutex mutex_;
      std::optional<FlightTarget> target_;
  };
  ```
- Modifies (`block_detector.h`):
  - `BlockSample::aircraftFolder` (a `std::string`, filled by the recorder);
  - `BlockOut::plannedFlightId` and `BlockOut::aircraftFolder`;
  - a new `struct BlockOutRefused { TimePoint time; MatchResult match; double latDeg, lonDeg; }` added to the
    `BlockEvent` variant, with event type `block_out_refused`. Its `data` has `time`, `reason` (= `problemCode`),
    `planned_flight_id`, `aircraft_folder`, `lat`, `lon`, `distance_m`, `fuel_lb` and `min_fuel_lb`.
  - `using ReleaseCheck = std::function<MatchResult(const BlockSample& release)>;` and
    `explicit BlockDetector(ReleaseCheck check = {})`. An empty check always matches, so the existing tests are
    unchanged. At a release with no flight open, a mismatch returns `BlockOutRefused` and keeps no candidate. A match
    keeps the candidate as before and remembers `plannedFlightId` for its `BlockOut`.
  - `block_out` data gains `planned_flight_id` and `aircraft_folder`.

- [ ] **Step 1: Write the failing tests** (`flight_match_test.cpp`, `block_detector_test.cpp`). Use a BBJ2 target at
  KBFI (47.53, -122.302) with fuel on record 5,000 lb:
  - `Match.FolderFromBothPathForms`:
    - `SimObjects\Airplanes\PMDG 737-800BBJ2\aircraft.CFG` gives `PMDG 737-800BBJ2`;
    - `i:\flightsim\community\pmdg-aircraft-738\simobjects\airplanes\pmdg 737-800bbj2\aircraft.cfg` gives
      `pmdg 737-800bbj2`;
    - `""` gives `""`.
  - `Match.FolderMatchRule`:
    - `pmdg 737-800bbj2` matches `PMDG 737-800BBJ2`;
    - `PMDG 777-200ER GE` matches `PMDG 777-200ER`;
    - `PMDG 737-800BBJ2` does **not** match `PMDG 737-800`;
    - `""` matches nothing.
  - `Match.NoTarget`: the problem is `NoPlannedFlight` and the id is 0.
  - `Match.WrongType`: in the 737-800 folder, the problem is `WrongAircraftType`.
  - `Match.WrongAirport`: 3,500 m from KBFI gives `WrongAirport` with `distanceM` about 3,500; 2,500 m gives `None`.
  - `Match.FuelSlack`:
    - 4,700 lb is `None`, and 4,699 lb is `FuelBelowRecord` with `minFuelLb` 4,700;
    - fuel on record 0 (a delivery) never refuses.
  - `Match.TypeIsCheckedBeforeAirport`: wrong type and wrong airport together report `WrongAircraftType`.
  - `Detector.RefusedReleaseIsReportedOnce`: a check returning `WrongAirport`, then a brake release, then 30 m of
    movement. Expected: one `BlockOutRefused` at the release, then nothing (no `BlockOut`).
  - `Detector.RefusedReleaseThenMatchingReleaseOpens`: the first release is refused. The check then flips to `None`,
    and a set, a release and 10 m of movement follow. Expected: a `BlockOut` with `plannedFlightId` 42.
  - `Detector.ToJsonOfRefusedAndBlockOut`: the field names above.
- [ ] **Step 2:** Run `--gtest_filter=Match.*:Detector.*`. Expected: they don't compile.
- [ ] **Step 3:** Implement. `distanceM` uses `geo.h`'s great-circle distance. `flight_history.cpp` skips
  `block_out_refused` lines.
- [ ] **Step 4:** Run all tests. Expected: PASS, the old detector tests included.
- [ ] **Step 5:**
  - `docs/recorder.md` "Matching the planned flight": the folder rule, the fuel check and its slack, the event's
    fields;
  - the event table row for `block_out_refused`, and `block_out`'s new fields;
  - design.md §13.4: the fuel check.
- [ ] **Step 6: Commit:** "A release opens a flight only when the sim matches the plan (D-088, D-110, D-112)".

### Task 4: Recorder wiring and the Not recording banner

**Files:**
- Modify: `src/sim_connection.h/.cpp`, `src/recorder.h/.cpp`, `src/recorder_bridge.h/.cpp`, `src/app_main.cpp`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- `SimConnection::aircraftPath() const`, returning `const std::string&`: the normalized path last seen ("" until
  known).
- `Recorder(std::filesystem::path logDir, RecorderObserver& observer, const FlightTargetSlot& target)`:
  - constructs its `BlockDetector` with a check that calls `checkMatch(target.get(), s)`;
  - fills `BlockSample::aircraftFolder` from `aircraftFolder(sim.aircraftPath())`;
  - on every valid sample with no flight open, evaluates the same check and calls `observer.onMatch(result)` when
    it differs from the last one reported (and once after each connect).
- `RecorderObserver::onMatch(const MatchResult&)`, with a default empty body so test observers needn't change.
  `BridgeObserver` posts it.
- `RecorderBridge`:
  - `const std::optional<blocktime::MatchResult>& match() const`, empty until the first report and cleared when the
    connection is lost;
  - a `matchChanged()` signal;
  - `blockEvent(QString type, QString flightId)`, emitted from `applyBlockEvent` for every event type.
  - `block_out_refused` adds an event-feed line, `HH:MM:SS  NOT RECORDING  wrong_airport`, and doesn't touch
    `flightOpen`.
- `app_main`: one `blocktime::FlightTargetSlot target;`, declared before the recorder thread starts and handed to it.

- [ ] **Step 1: Write the failing tests:**
  - `Bridge.MatchArrivesAndClearsOnDisconnect`;
  - `Bridge.RefusedReleaseIsAFeedLineAndNoFlight`;
  - `Bridge.BlockEventSignalCarriesTypeAndId` (a `QSignalSpy` on `blockEvent` for block_out, block_in and
    block_out_refused).
- [ ] **Step 2:** Run the `Bridge.*` suite. Expected: FAIL.
- [ ] **Step 3:** Implement. The match is evaluated in `Recorder::run`'s sample lambda, after the junk filter.
- [ ] **Step 4:** Run all tests and the release build. Expected: PASS, no warnings.
- [ ] **Step 5: Commit:** "The recorder checks each release against the planned flight from the GUI (D-088)".

### Task 5: Reading the flown flight back and settling it

**Files:**
- Create: `src/flight_settlement.h/.cpp`, `tests/flight_settlement_test.cpp`
- Modify: `src/airline_queries.h/.cpp` (nearest airport, hours, flights), `docs/database.md` (processes)

**Interfaces:**
- Produces (`flight_settlement.h`):
  ```cpp
  struct FlownFlight {
      std::string logFlightId, blockOutAt, blockInAt;  // instants, whole seconds (ms dropped)
      int fuelOutLb = 0, fuelInLb = 0;                 // block_out fuel_lb; block_in shutdown_fuel_lb (D-110)
      double inLat = 0, inLon = 0;
  };
  // Reads up to maxFiles daily logs, oldest first. The first block_out with this planned_flight_id whose time is
  // after plannedAt, followed by a block_in with its flight_id, is the flown flight. An interrupted attempt is
  // skipped, and so is a pair whose log_flight_id is in `alreadySettled`.
  std::optional<FlownFlight> findFlownFlight(const std::filesystem::path& logDir, std::int64_t plannedFlightId,
                                             std::string_view plannedAt, const std::set<std::string>& alreadySettled,
                                             std::size_t maxFiles = kOpenFlightLookbackFiles);
  struct SettledFlight {
      std::string logFlightId, arrivalIcao;
      bool diverted = false;
      Preview money;
  };
  // One transaction: the flight with its cabins and plan, its money lines (each a ledger_line posted at block_in_at
  // plus a flight_money_line), the airplane moved to the arrival airport with fuel_in_lb on board and
  // awaiting_delivery 0, and the planned flight deleted.
  std::optional<SettledFlight> settleFlight(db::Database& db, const PlannedFlightView& planned, const FlownFlight& flown,
                                            const Rates& rates, std::string* error);
  // findFlownFlight, then settleFlight, for the current planned flight. nullopt when nothing was flown (or an error).
  std::optional<SettledFlight> settleIfFlown(db::Database& db, const std::filesystem::path& logDir, const Rates& rates,
                                             std::string* error);
  ```
- Produces (`airline_queries.h`):
  - `struct NearestAirport { std::int64_t id; std::string icao, name; double distanceM; };`
  - `std::optional<NearestAirport> nearestAirport(db::Database&, double lat, double lon)`: the closest imported
    airport, at any distance (D-113).
  - `std::set<std::string> settledLogFlightIds(db::Database&)`
  - `double hoursFlown(db::Database&)`: the sum of `block_in_at − block_out_at` over all flights, in hours.
- **Money (D-109):** `priceInput(planned)` with `actualBlockHours` = the real block time, and `fuelBurnLb` =
  `max(0, fuelOutLb − airplane.fuel_on_record)`. So the "Fuel" line is the fuel added, at the planned flight's locked
  price. Income and the other lines use the planned flight's distance and plan block time, even after a diversion.
  Lines are `pricePreview(in, rates)`'s, each posted with its `category`.

- [ ] **Step 1: Write the failing tests.** Use `testAirline()` plus a temp log dir with hand-written JSONL. N814SX's
  delivery flight is KBFI→KSAN, planned at `2026-11-23T10:00:00Z`, with fuel on record 0.
  - `FlownFlight.FindsTheMatchingPair`: a block_out (planned id 1, 10:30:05.120Z, fuel 20,000) and a block_in
    (13:20:41.900Z, shutdown fuel 9,400). Expected: times `10:30:05Z` and `13:20:41Z`, fuel 20,000 and 9,400.
  - `FlownFlight.InterruptedThenFlownAgain`: block_out A, flight_interrupted A, block_out B, block_in B. Expected:
    B.
  - `FlownFlight.IgnoresAnOlderFlightWithTheSameId`: a complete pair with planned id 1 at 08:00, before
    `plannedAt`. Expected: nullopt. Also, a pair after `plannedAt` whose id is in `alreadySettled`: nullopt.
  - `FlownFlight.OpenFlightIsNotFlown`: a block_out alone gives nullopt.
  - `Settle.DeliveryFlight`:
    - one `flight` row, KSAN arrival, not diverted;
    - N814SX at KSAN with 9,400 lb on board, no longer awaiting delivery;
    - no planned flight left;
    - the money lines are fuel (20,000 lb × $41 / 100 = -$8,200), landing fee, ATC, pilots and maintenance for
      2.84 h;
    - cash went down by exactly the sum of the lines;
    - every ledger line is posted at `13:20:41Z`.
  - `Settle.FuelAddedNotBurned`: fuel on record 12,000 and fuel out 18,000 gives a Fuel line of -$2,460 (6,000 lb).
    With fuel out 11,800 there's no Fuel line at all.
  - `Settle.DiversionMovesToWhereItParked`: block in at KSHT's position. Expected: arrival KSHT, `diverted`, the
    airplane at KSHT, and the fares equal to the preview's.
  - `Settle.RunsOnceFromTheLog`: `settleIfFlown` twice. Expected: the second returns nullopt and the database is
    unchanged (count the flights and ledger lines).
  - `Settle.KeepsThePlanAndCabins`: with an imported plan, `flight_plan` has its route and `ofp_json`, and
    `flight_cabin` has the passengers.
  - `Settle.FailureWritesNothing`: force an error (for example, delete the planned destination airport row first).
    Expected: nothing written, the planned flight still there, and the error set.
  - `Queries.NearestAirportAndHours`: a point 1 km from KSAN gives KSAN. Two flights of 1:30 and 2:15 give 3.75
    hours.
- [ ] **Step 2:** Run `--gtest_filter=FlownFlight.*:Settle.*:Queries.*`. Expected: they don't compile.
- [ ] **Step 3:** Implement. Reuse `planned_flight.cpp`'s readers. `INSERT … SELECT` copies the cabins and the plan.
- [ ] **Step 4:** Run all tests. Expected: PASS.
- [ ] **Step 5: Docs:**
  - database.md "Milestone 6's processes": settling (what the one transaction writes), the arrival airport, hours;
  - design.md §11: fuel charged is fuel added;
  - design.md §13.8: the planned flight stays (D-108);
  - design.md §14: diversion money (D-109);
  - roadmap milestone 6: hours are derived (D-114).
- [ ] **Step 6: Commit:** "Settle a flown flight from the log in one transaction (D-109, D-113, D-114)".

### Task 6: Fleet slots and hours on Fleet

**Files:**
- Modify: `src/leasing.h/.cpp`, `src/airline_queries.h/.cpp`, `src/airline_session.h/.cpp`,
  `src/fleet_table_model.h/.cpp`, `qml/FleetPage.qml`
- Test: `tests/leasing_test.cpp`, `tests/qt_models_test.cpp`

**Interfaces:**
- `int slotsEarned(double hoursFlown)`: the largest n with `5·n·(n−1) ≤ hours`, at least 1 (design.md §6's table).
  `double hoursForSlot(int n)` = `5·n·(n−1)`.
- `settleLeases` leases a new airplane only while the airplanes in the fleet (not returned) number fewer than
  `slotsEarned(hoursFlown(db))`. Otherwise it gives a notice and tries again at the next start or tick: "Can't lease
  N738BT yet: 1 of 1 slots in use. Slot 2 comes at 10 hours flown (you have 3.2)."
- `AirplaneView::flights` (int) and `AirplaneView::blockHours` (double), counted from `flight`.
- `AirlineSession::hoursFlown()`.
- `FleetTableModel`:
  - Flights and Hours columns (D-097 undone, now from the database);
  - a `summaryText` property: "3.2 h flown · 1 of 1 slots in use · slot 2 at 10 h";
  - `reset(airplanes, notices, summaryText)`.
- `FleetPage.qml` shows `summaryText` under the title.

- [ ] **Step 1: Write the failing tests:**
  - `Leasing.SlotsFromHours`: 0 h gives 1 slot, 9.9 h 1, 10 h 2, 29.9 h 2, 30 h 3, 450 h 10.
  - `Leasing.NoFreeSlotMeansNoLeaseAndANotice`: a second airplane in the config with 0 hours. Expected: not leased,
    the notice text above, and no ledger line. Add 10 hours of flights, settle again: leased.
  - `Fleet.FlightsAndHoursColumns`, `Fleet.SummaryText`.
- [ ] **Step 2:** Run them. Expected: FAIL.
- [ ] **Step 3:** Implement.
- [ ] **Step 4:** Run all tests. Expected: PASS.
- [ ] **Step 5:** design.md §6: slots are enforced at lease time (D-083), and returned airplanes don't count. Commit:
  "Fleet slots follow Dave's flown hours (D-083, D-114)".

### Task 7: The flight's life on Live

**Files:**
- Modify:
  - `src/planned_flight_model.h/.cpp`, `src/planned_flight.h/.cpp` (the target);
  - `src/app_main.cpp`;
  - `qml/LivePage.qml`, `qml/PlannedFlightPanel.qml`.
- Create: `qml/NotRecordingBanner.qml`
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- `std::optional<FlightTarget> flightTarget(db::Database&, const PlannedFlightView&)`, in `planned_flight.h`: the
  airplane's type `sim_folder`, the origin's position and the airplane's `fuel_on_board_lb`.
- `PlannedFlightModel(db, airlineIcao, planner, bridge, FlightTargetSlot& target, std::filesystem::path logDir,
  std::filesystem::path ratesFile, QObject* parent = nullptr)`:
  - `reload()` also sets `target` (or clears it with no planned flight).
  - **On `bridge.blockEvent`:**
    - `block_in`: `settleIfFlown`;
    - `flight_interrupted` while a flight is planned (the recorder only opens matched flights, so it's this one's
      attempt; a `recorder_crash` at startup counts too): `notice` = "KBFI–KSAN interrupted and discarded". The planned flight stays, and the state goes back to "Waiting for sim" (D-108).
  - `checkFlown()`, `Q_INVOKABLE`, is called once by `app_main` after construction: the startup settle.
  - After a settle:
    - `summaryFlightId` = the log flight id;
    - emits `settled()`, which `app_main` connects to the Fleet, Ledger and Flights refresh;
    - `reload()`.
  - New `state` values: `"flying"` while `bridge.flightOpen()`, and `"summary"` while `summaryFlightId` is set and
    nothing is planned.
  - `Q_PROPERTY QString notRecording`: "" while matching, in flight, or without a sample. Otherwise one sentence:
    - "Not recording: you're in a PMDG 737-800; this flight expects the 737-800 BBJ2 N814SX." (the folder's catalog
      type by `sim_folder`, else the raw folder);
    - "Not recording: you're 4.2 km from KBFI (nearest: KSEA); load in at KBFI.";
    - "Not recording: 4,100 lb of fuel on board; load at least 4,700 lb (N814SX had 5,000 lb at its last block in)."
  - `Q_PROPERTY QString notice` and `Q_PROPERTY QString summaryFlightId`.
  - `Q_INVOKABLE void doneForNow()` clears `summaryFlightId`.
  - `Q_INVOKABLE void turnaround()` clears it and emits `turnaroundRequested(QString registration)`. `LivePage`
    opens the New Flight dialog for that airplane.
- `NotRecordingBanner.qml`: a full-width warning strip (Theme colours) on Live, above the planned-flight panel, shown
  while `notRecording` isn't "".

- [ ] **Step 1: Write the failing tests.** Use a fake planner, a temp log dir and `testAirline()` with a planned
  KBFI→KSAN delivery flight:
  - `PlannedFlightModel.ReloadSetsTheTarget`: the slot holds `PMDG 737-800BBJ2`, KBFI's position and fuel 0.
    Cancelling clears it.
  - `PlannedFlightModel.NotRecordingTexts`: feed the bridge matches for each of the three problems. Expected: the
    three sentences above. A `None` match gives "".
  - `PlannedFlightModel.SettlesOnBlockIn`: write the pair to the log, then emit `blockEvent("block_in", id)`.
    Expected: `settled` emitted, `state` "summary", `summaryFlightId` set, and no planned flight.
  - `PlannedFlightModel.SettlesAtStartup`: the pair is already in the log before construction. Expected:
    `checkFlown()` settles it.
  - `PlannedFlightModel.InterruptedKeepsThePlanWithANotice`.
  - `PlannedFlightModel.TurnaroundAsksForNewFlight`: `turnaroundRequested("N814SX")`, then `state` "none".
- [ ] **Step 2:** Run `--gtest_filter=PlannedFlightModel.*`. Expected: FAIL.
- [ ] **Step 3:** Implement the model and wire `app_main`. In `LivePage.qml`:
  - the planned panel shows for `waiting_*` states;
  - the existing FlightPanel shows while flying;
  - the summary slot is left for Task 8, showing a placeholder `Text` until then.
- [ ] **Step 4:** Run all tests and qmllint. Expected: PASS, nothing reported.
- [ ] **Step 5: Commit:** "Live follows the flight from plan to block in, and settles it (D-108, D-113)".

### Task 8: The summary, and Flights from the database

**Files:**
- Create: `qml/FlightSummary.qml`
- Modify:
  - `src/airline_queries.h/.cpp` (`listFlights`);
  - `src/flight_list_model.h/.cpp`, `src/app_main.cpp`;
  - `qml/FlightsPage.qml`, `qml/FlightCard.qml`, `qml/LivePage.qml`, `qml/MoneyLines.qml` (header comment only).
- Test: `tests/flight_settlement_test.cpp`, `tests/qt_models_test.cpp`

**Interfaces:**
- `struct FlightView`:
  - `logFlightId`, `flightNumber`, `registration`, `typeCode`;
  - `origin`, `plannedDestination`, `arrival`;
  - `blockOutAt`, `blockInAt`, `blockS`;
  - `fuelOutLb`, `fuelInLb`;
  - `std::vector<MoneyLine> income, costs`;
  - `Cents totalCents`.

  `std::vector<FlightView> listFlights(db::Database&)` returns them newest block in first. The lines are read back
  through `flight_money_line` → `ledger_line` → `ledger_category`, in the category order of Task 1.
- `FlightListModel(blocktime::db::Database& db, std::filesystem::path logDir, QObject* parent = nullptr)`:
  - rows from `listFlights`, read on the GUI thread;
  - tracks from `loadFlightHistory` on the worker as today, joined on `flightId`. A flight with no track in the log
    shows only its two airports.
  - Roles: `FlightIdRole`, `DateTextRole`, `RouteTextRole` ("KBFI–KSAN", or "KBFI–KSAN, diverted to KSHT"),
    `FlightNumberRole` ("XLY8"), `RegistrationRole`, `BlockTextRole`, `FuelUsedLbRole` (out − in), `TotalTextRole`
    (the short form), `TotalCentsRole`.
  - `Q_INVOKABLE QVariantMap summary(int row) const` returns `{ title, dateText, outText, inText, blockText,
    fuelText, routeText, income: [{label, amount}], costs: [...], totalText }`. The amounts are in full form.
  - `Q_INVOKABLE int rowOf(const QString& flightId) const` gives -1 if absent.
  - `reload()` keeps its worker behaviour for the tracks.
- `FlightSummary.qml`:
  - required `property var summary` and `property var track`;
  - the title line, times and route, `MoneyLines` with the total, and a `RouteMap`;
  - an optional `property bool showActions`, with **At Gate Turnaround** (primary) and **Done for now** (secondary)
    buttons signalling `turnaround()` and `done()`.

  Live uses it with actions for `plannedFlight.summaryFlightId`. Flights uses it without actions for the selected
  row.

- [ ] **Step 1: Write the failing tests:**
  - `Queries.ListFlightsReadsLinesBack`: after `Settle.DeliveryFlight`'s settle, one view whose lines and total equal
    the `SettledFlight` money.
  - `FlightList.RowsFromDatabaseAndTrackFromLog`: one settled flight, plus a log with its telemetry. Expected: one
    row, a `track(0)` longer than 2 points, and `summary(0).totalText` in full form.
  - `FlightList.InterruptedFlightsAreNotListed`: a log with an interrupted flight and an empty database gives 0
    rows.
- [ ] **Step 2:** Run them. Expected: FAIL.
- [ ] **Step 3:** Implement, and update the two pages.
- [ ] **Step 4:** Run all tests and qmllint. Then check with the debug build's hot reload, on a scratch data folder
  (`$env:LOCALAPPDATA` pointed at a copy, with a hand-written log pair): Live shows the summary with both buttons;
  Flights lists the flight and shows the same summary; nothing clips at the smallest window.
- [ ] **Step 5:** design.md §13.7: the plain summary as built (no score until milestone 10). Commit: "A summary after
  block in, and Flights lists the settled flights (D-111)".

### Task 9: The probe (`--probe`)

**Files:**
- Create: `src/sim_probe.h/.cpp` (app library, SimConnect), `docs/notes/probe-flight.md`
- Modify: `src/sim_connection.h/.cpp`, `src/recorder.h/.cpp`, `src/app_main.cpp`, `CMakeLists.txt`

**Interfaces:**
- `--probe` sets `Recorder::Options::probe`, passed through to `SimConnection`. Without it nothing below is
  registered (Global Constraints).
- **GSX:**
  - a second data definition of these L:vars, `PERIOD_SECOND` + `CHANGED`, logged as `probe_gsx` with every value
    on each change;
  - the names come from community tools and are **unverified**, which is what the probe is for:
    `L:FSDT_GSX_COUATL_STARTED`, `L:FSDT_GSX_CATERING_STATE`, `L:FSDT_GSX_REFUELING_STATE`,
    `L:FSDT_GSX_BOARDING_STATE`, `L:FSDT_GSX_DEBOARDING_STATE`, `L:FSDT_GSX_DEPARTURE_STATE`,
    `L:FSDT_GSX_NUMPASSENGERS`, `L:FSDT_GSX_NUMPASSENGERS_BOARDING_TOTAL`, `L:FSDT_GSX_NUMPASSENGERS_DEBOARDING_TOTAL`,
    `L:FSDT_GSX_BOARDING_CARGO_PERCENT`, `L:FSDT_GSX_DEBOARDING_CARGO_PERCENT`, `L:FSDT_GSX_FUELHOSE_CONNECTED`.
- **PMDG:**
  - with the CMake path `BLOCKTIME_PMDG_SDK_DIR` set (the folder holding `PMDG_NG3_SDK.h` and `PMDG_777X_SDK.h`,
    never committed, like the SimConnect SDK), subscribe to `PMDG_NG3_Data` or `PMDG_777X_Data` by the loaded
    aircraft's family;
  - log `probe_pmdg` with the fields of the probe list in `docs/notes/pmdg-variables-from-spad.md` ("What the probe
    flight should log"), by their header names, on change, at most once a second;
  - next to them, the SimVars `LIGHT BEACON`, `LIGHT STROBE`, `LIGHT LANDING`, `LIGHT TAXI`, `LIGHT NAV`,
    `CABIN SEATBELTS ALERT SWITCH`, `APU SWITCH`, `GEAR HANDLE POSITION` and `GEAR TOTAL PCT EXTENDED`;
  - without the SDK path, the probe logs GSX and facilities only, and the README says so.
- **Facilities:**
  - when the sim connects with a target set, and again at each block in, request the facility data of the origin and
    the destination (`SimConnect_AddToFacilityDefinition` / `SimConnect_RequestFacilityData`, MSFS 2020 SU9+);
  - the airport's position, its runways (designators, position, heading, length) and its `TAXI_PARKING` (name,
    number, suffix, type, radius, and position or bias);
  - one `probe_facility` line per airport;
  - take the field names from the SDK docs, and note any that differ in the probe note.

- [ ] **Step 1:** Write `docs/notes/probe-flight.md`: what's logged, how to switch it on, and the questions each part
  answers (milestones 7–10). Link it from `docs/README.md`.
- [ ] **Step 2:** Implement. There are no unit tests for the SimConnect side. `blocktime-core` is unchanged.
- [ ] **Step 3:** Build release and debug with and without `BLOCKTIME_PMDG_SDK_DIR`. Expected: no warnings. Start with
  `--probe` and without the sim. Expected: only the normal lines.
- [ ] **Step 4:** Log the decision (D-116 is written with this plan; update it if the details changed). README: the
  probe command line. Commit: "A --probe mode for milestone 6's test flights (D-116)".

### Task 10: Version, review, and Dave's test list

- [ ] **Step 1:** Version 0.7.0 in `CMakeLists.txt` and `vcpkg.json`. README: the build, the new config files
  (`rates.yaml`, the new `catalog.yaml`), `--probe`, and the screens (summary, Flights).
- [ ] **Step 2:** verification-before-completion: clean release and debug builds with no warnings, all tests, and
  qmllint. Then a scratch-folder run of the app with Dave's real config, which should start with no notices.
- [ ] **Step 3:** requesting-code-review on the whole milestone, then receiving-code-review. Fix Important and
  Critical findings with a failing test first.
- [ ] **Step 4:** `docs/progress.md` entry, with Dave's checks printed in the reply too (CLAUDE.md). The sim checks are
  ordered to need as few MSFS restarts as possible:
  1. **No sim:** copy `rates.yaml` and the new `catalog.yaml`, and remove `fuel_price_per_100lb` from
     `airline.yaml`. Start BlockTime. Expected: Fleet's "0.0 h flown · 1 of 1 slots in use · slot 2 at 10 h", and
     no notices.
  2. **Sim session 1, wrong aircraft:** plan the KBFI→KSAN delivery flight. Start the sim with `--probe` in the PMDG
     737-800 (Soxxly) at KBFI. Expected: "Not recording: you're in a PMDG 737-800…", and a brake release logs
     `block_out_refused` (`wrong_aircraft_type`) without opening a flight. Exit to the main menu.
  3. **Same session, BBJ2 at the wrong airport:** load the BBJ2 at KSEA. Expected: the wrong-airport banner, naming
     KSEA. Main menu again.
  4. **Same session, the delivery leg:**
     - load the BBJ2 at KBFI. Expected: the banner goes away (confirming D-112's folder rule for the BBJ2);
     - fly KBFI→KSAN with GSX. At block in, expected:
       - the summary: fuel, landing, ATC, pilots and maintenance;
       - N814SX at KSAN with the shutdown fuel, no longer awaiting delivery;
       - cash down by the total;
       - Fleet shows 1 flight and its hours.
     - Also note the gate name SayIntentions' ATC gives you at KSAN, to compare with the probe's parking names.
  5. **Same session, turnaround:**
     - click At Gate Turnaround and plan a VIP leg KSAN→KBFI (or anywhere);
     - release the brake with less fuel than at block in, minus 300 lb. Expected: the fuel banner and
       `fuel_below_record`;
     - refuel, then fly it. Expected: a VIP summary, and Fuel charging only what was added.
  6. **Same session, discard:** plan another leg, push back, and exit to the main menu. Expected: "… interrupted and
     discarded", the plan still waiting, the airplane where it was, and no money.
  7. **No sim:** Flights lists exactly the two flights with their summaries and maps. Close and reopen: everything is
     unchanged. Send the day's log file, so the probe results go into `docs/notes/`.
- [ ] **Step 5: Commit:** "Milestone 6 built: record the planned flight (version 0.7.0)".
