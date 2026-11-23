# Milestone 5 (New Flight and Plan) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** From a parked airplane, Dave sets up a flight (destination, kind, loads), sees its money before committing,
and plans it: the app fetches ATIS runways, opens a pre-filled SimBrief dispatch page, and imports the generated
plan with a button. The Live page shows the planned flight before departure.

**Architecture:**
- All logic is plain C++ in `blocktime-core`, unit-tested:
  - airplane limits (D-100);
  - load rolling and the hold split (design.md §9);
  - pricing (§10–12);
  - SimBrief and SayIntentions URLs and parsing;
  - the planned flight in the database (D-103);
  - a `FlightDraft` that holds New Flight's state.
- The app adds Qt Network for the two web calls, behind a small `Planner` interface so the Qt models can be tested
  with a fake. There are two new models (`NewFlightModel`, `PlannedFlightModel`), a New Flight dialog, and a
  before-departure panel on the Live page.
- Nothing touches the recorder. The matching check and recording the planned flight are milestone 6.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, yaml-cpp,
gtest), Qt 6 Quick + **Qt Network** (new component, same Qt kit).

**Spec:**
- `docs/database.md`: "Milestone 5's tables" and "Milestone 5's processes" are the source of truth for the schema.
- `docs/design.md`: §3 (limits), §9 (loads), §10–12 (crew, costs, revenue), §13.1–13.3 (the flow).
- `docs/notes/simbrief-api.md`, `docs/notes/sayintentions-api.md`: the web APIs.
- Decisions D-070, D-089, D-099..D-106. D-106 (a default free-flight number per airplane) came during Task 1 and
  replaces D-102's empty field; Tasks 1, 5 and 6 below include it.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`, release and debug. All tests pass. `blocktime-app_qmllint` reports nothing.
- Every new source and QML file starts with the GPL header. QML files `import BlockTime` and use
  `pragma ComponentBehavior: Bound`; every colour, size and font comes from `Theme.qml`.
- `blocktime-core` and its tests include no Qt, `windows.h` or `SimConnect.h`.
- **D-099:** config-file code gets the happy path only. Don't add validation, drift warnings or error messages for
  `catalog.yaml` / `airline.yaml` beyond what the existing readers already do for a missing required key.
- **Database:** the schema exactly as `docs/database.md` says; times `YYYY-MM-DDTHH:MM:SSZ`; money in cents, every
  stored or shown money line a multiple of 500 (D-080).
- **Secrets:** `settings.json` gains `simbrief_user_id`, `sayintentions_api_key`, `sayintentions_developer_key`. The
  two SayIntentions keys are never logged or shown. The developer key goes only in the `X-Developer-Key` header.
  The Qt message handler redacts all three keys and the CARTO key.
- **SimBrief:** the OFP endpoint is called only when Dave clicks Import (D-101). Never on a timer.
- **Version:** **0.6.0** (`CMakeLists.txt`, `vcpkg.json`).
- **Commits:** straight to `main`, why not what, D-numbers, ending with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Don't push.
- Build `cmake --build --preset x64-release`; tests `ctest --preset x64-release`; one suite with
  `build\x64-release\blocktime-tests.exe --gtest_filter=<Suite>.*`.

## Review Focus

1. **Importing the wrong plan.** Dave forgets to press Generate, or generates a different route or airframe.
   Expected: nothing is stored and the dashboard says exactly why. Tests: Task 4 `SimBrief.PlanMismatch*`, Task 6
   `PlannedFlightModel.ImportRefusesAStalePlan`.
2. **SayIntentions slow, down, wrong key, or no ATIS.** Expected: planning still opens SimBrief, without runways, and
   the dashboard says "No ATIS runways: SimBrief chose". Tests: Task 4 `Atis.*`, Task 6
   `NewFlightModel.PlanWithoutAtisStillPlans`.
3. **The app restarted between Plan flight and Import.** Expected: the Live page shows the same planned flight
   waiting for its plan, and Import works. Test: Task 5 `Planned.SurvivesReopening`.
4. **Toggling choices in New Flight.** Switching belly cargo, or VIP and Charter, never re-rolls the passengers
   (D-089); a delivery flight has no load and shows a loss. Tests: Task 5 `Draft.*`.
5. **A SimBrief error body or HTTP 400** (wrong user id). Expected: the message shows SimBrief's status text, not a
   parse error. Test: Task 4 `SimBrief.OfpErrorStatus`.

## Before Task 4: Dave checks the cargo unit (2 minutes, no sim)

SimBrief's docs don't say whether `cargo` is in pounds or thousands. Dave opens both URLs (logged in to SimBrief) and
reports what the Freight / cargo field shows:
- `https://dispatch.simbrief.com/options/custom?orig=KBFI&dest=KSAN&type=730097_1790536204614&pax=40&cargo=5&units=LBS`
- the same with `cargo=5000`

Record the answer in `docs/notes/simbrief-api.md`. Task 4's `dispatchUrl` writes cargo in that unit: whole pounds, or
thousands with one decimal (`5.2`). The same check confirms that `type` selects N814SX's airframe and that `pax`
sticks.

---

### Task 1: Airplane limits, fuel price, and schema version 3

**Files:**
- Modify: `src/catalog.h/.cpp`, `src/airline_config.h/.cpp`, `src/airline_db.h/.cpp`, `src/leasing.h/.cpp`,
  `src/airline_queries.h/.cpp`, `src/airline_session.cpp`
- Modify: `config/catalog.yaml`, `config/airline.example.yaml`
- Test: `tests/catalog_test.cpp`, `tests/airline_config_test.cpp`, `tests/airline_db_test.cpp`,
  `tests/leasing_test.cpp`
- Create: `tests/test_airline.h` (shared fixture, below)

**Interfaces:**
- Produces (`catalog.h`):
  - `struct AirplaneLimits { int minRunwayFt, mlwLb, passengerWeightLb, bagWeightLb, maxPayloadLb; };`
  - `CatalogAirframe::limits` (an `AirplaneLimits`), read from a required `limits:` map with keys `min_runway_ft`,
    `mlw_lb`, `passenger_weight_lb`, `bag_weight_lb`, `max_payload_lb`.
- Produces (`airline_config.h`):
  - `struct LimitOverrides`: the same five fields as `std::optional<int>`, read from an optional `limits:` map on
    each airplane.
  - `ConfigAirplane::limits`, a `LimitOverrides`.
  - `AirlineConfig::fuelPriceCentsPer100Lb`, a `Cents`, default 4100. Read from optional `fuel_price_per_100lb`
    (dollars, up to 2 decimals).
  - `AirplaneLimits withOverrides(const AirplaneLimits&, const LimitOverrides&)`.
- Produces (`airline_db.h`): `kAirlineSchemaVersion = 3`, plus migration 3.
- Produces (`airline_queries.h`):
  `std::optional<AirplaneLimits> airplaneLimits(db::Database&, std::int64_t airplaneId)`.
- Produces (D-106): `ConfigAirplane::flightNumber` (optional `flight_number`, "" = none), the column
  `airplane.free_flight_number`, and `bool syncFreeFlightNumbers(db::Database&, const AirlineConfig&,
  std::string* error)`, called at start. Tests `Config.FlightNumberIsOptional`,
  `AirlineDb.FreeFlightNumberIsUniqueDigits`, `Leasing.FreeFlightNumberFollowsTheConfig`.
- Produces (`leasing.h`): `bool fillMissingLimits(db::Database&, const Catalog&, const AirlineConfig&,
  std::string* error)`. For every airplane without an `airplane_limits` row: its airframe's limits (by
  `airframe_name`), with its config overrides (by registration). Leasing also writes the row, inside the lease's
  transaction.
- Produces (`tests/test_airline.h`, header-only): `db::Database testAirline()`. It returns an in-memory database
  that has:
  - all migrations applied;
  - the repo's `config/catalog.yaml` synced;
  - airports KBFI (47.53, -122.302, `America/Los_Angeles`, 10,007 ft), KSAN (32.7336, -117.19,
    `America/Los_Angeles`, 9,401 ft) and KSHT (47.0, -120.0, `America/Los_Angeles`, 5,000 ft);
  - two airplanes at KBFI, leased 2026-11-22: N814SX ("737-800 BBJ2 VIP", awaiting delivery, SimBrief
    `730097_1790536204614`) and N738BT ("737-800 Two-class", SimBrief `test_738`).

  Later tasks' tests use it.

- [ ] **Step 1: Write the failing tests:**
  - `Catalog.AirframeLimitsAreRead`: the repo catalog's "737-800 BBJ2 VIP" has limits 7000 / 146300 / 190 / 55 /
    36113.
  - `Config.LimitOverridesAndFuelPrice`:
    - an airplane with `limits: { mlw_lb: 140000 }` has only `mlwLb` set;
    - `fuel_price_per_100lb: 41.5` gives 4150;
    - leaving it out gives 4100.
  - `AirlineDb.EmptyDatabaseMigratesToVersion3`: rename the existing `...ToVersion2` test and expect 3.
  - `AirlineDb.Version2DatabaseMigratesTo3`: build version 2 only (run migration 2's SQL by hand, or a test hook),
    then migrate. Expect version 3 and an `airplane_limits` table.
  - `AirlineDb.RestoredVersionStillGetsMigration3`:
    - a file at `user_version` 1 that has an `airplane` table (milestone 4's restore case);
    - `openAirlineDatabase` ends at version 3, with `planned_flight` present.

    Today the restore writes `kAirlineSchemaVersion`, which would skip migration 3. It must write **2**.
  - `AirlineDb.PlannedFlightChecks`: each insert fails:
    - `cargo_lb` 500 on a `vip` flight;
    - the same origin and destination;
    - `flight_number` `'A1'`;
    - a second planned flight for the same airplane.
  - `AirlineDb.QueryPlansUseIndexes`: add `SELECT planned_flight_id FROM planned_flight WHERE
    destination_airport_id = ?`.
  - `Leasing.LeaseCopiesLimitsWithOverrides`: a config airplane with `mlwLb` overridden to 140000. After leasing,
    `airplaneLimits` is 7000 / 140000 / 190 / 55 / 36113.
  - `Leasing.AirplaneLeasedBeforeLimitsGetsThem`: delete the airplane's `airplane_limits` row, call
    `fillMissingLimits`, and it's back with the airframe's values.
- [ ] **Step 2: Run them.** Expected: compile errors (`AirplaneLimits` doesn't exist).
- [ ] **Step 3: Implement.**
  - Migration 3 is the four tables of `docs/database.md` "Milestone 5's tables", with their checks and indexes.
  - Add `limits:` to every airframe in `config/catalog.yaml`, marking every value except the BBJ2's `# estimate`.
    Bag weight is 55 and passenger weight 190 everywhere.

    | Airframe(s) | min_runway_ft | mlw_lb | max_payload_lb |
    |---|---|---|---|
    | 737-600 | 6000 | 120500 | 33800 |
    | 737-800 | 7000 | 146300 | 47000 |
    | BBJ2 VIP | 7000 | 146300 | 36113 |
    | 737-800 BCF | 7000 | 146300 | 52000 |
    | 777-200ER | 9000 | 470000 | 135500 |
    | 777-300ER | 10000 | 554000 | 154000 |
    | 777F | 10000 | 575000 | 228700 |

  - Add the commented `limits:` override and `fuel_price_per_100lb: 41` to `config/airline.example.yaml`.
  - `AirlineSession::start` calls `fillMissingLimits` after `settleLeases`.
- [ ] **Step 4: Run all tests.** Expected: PASS, including every milestone 4 test.
- [ ] **Step 5: Commit** ("Airplane limits with airframe defaults (D-100), schema 3").

### Task 2: Loads and the hold split

**Files:** Create `src/loads.h/.cpp`, `tests/loads_test.cpp`; modify `src/dates.h/.cpp`, `tests/dates_test.cpp`,
`CMakeLists.txt`.

**Interfaces:**
- Produces (`loads.h`):
  - `enum class FlightKind { Passenger, Vip, Charter, Cargo, Delivery };`
  - `std::string_view kindCode(FlightKind)`, which returns the database codes (`passenger`…`delivery`);
    `std::optional<FlightKind> parseKind(std::string_view)`.
  - `using Roll = std::function<double()>;` returns a uniform value in [0, 1). The app uses `std::mt19937_64` seeded
    from `std::random_device`; tests pass fixed sequences.
  - `struct CabinLoad { std::string cabinClass; int passengers; };`
  - `struct LoadRoll { std::vector<CabinLoad> cabins; int cargoLb = 0; };`
  - `LoadRoll rollLoads(const std::vector<Cabin>& cabins, const std::vector<Hold>& holds, const AirplaneLimits&,
    FlightKind, bool daytime, const Roll&)`
  - `int bagsLb(const std::vector<CabinLoad>&, const AirplaneLimits&)`, which is passengers × bag weight.
  - `struct HoldLoad { std::string hold; int weightLb; };`
    `std::vector<HoldLoad> splitHolds(const std::vector<Hold>&, int totalLb)`.
  - `bool isDaytime(int localHour)`, true for 7 to 18.
- Produces (`dates.h`):
  `std::optional<int> localHour(std::string_view ianaZone, std::chrono::sys_seconds at)`, using
  `std::chrono::locate_zone`. It returns nullopt for an unknown zone; the caller then uses the UTC hour.

**Rules** (design.md §9, D-089):
- `roll()` is called once per cabin in the given order, then once for cargo if the kind has cargo.
- A cabin's passengers are `floor(seats × (lo + (hi − lo) × r))`. The bands:

  | Kind | lo–hi |
  |---|---|
  | Passenger or Vip, daytime | 0.90–1.00 |
  | Passenger or Vip, night | 0.40–0.80 |
  | Charter | 0.10–1.00 |

- **Passenger** also rolls belly cargo:
  - the cargo is `floor((0.10 + 0.90 × r) × (sum of hold max − bags))`;
  - it's capped at `maxPayload − passengers × passengerWeight − bags`, never below 0;
  - it's always rolled. Whether it flies is the draft's choice (Task 5).
- **Cargo** (freighter): no cabins; the cargo is `floor((0.50 + 0.50 × r) × sum of hold max)`, capped at
  `maxPayload`.
- **Vip, Charter:** no cargo roll. **Delivery:** no cabins, no cargo, no rolls.
- `splitHolds`:
  - give each hold `floor(remaining × ratio / sum of open ratios)`, capped at its max;
  - holds at their max close, and the rest is shared again among the open holds;
  - leftover pounds from flooring go to the open hold with the largest ratio;
  - the sum always equals `totalLb`. A total above capacity can't happen, because cargo is capped. Use an `assert`.

- [ ] **Step 1: Write the failing tests:**
  - `Loads.DaytimeBand`: 48 economy seats, Vip, day, r = 0.5 → 45.
  - `Loads.NightBand`: the same at night with r = 0 → 19.
  - `Loads.CharterBand`: r = 0 → 4.
  - `Loads.EachCabinRolledSeparately`: business 16 and economy 144, rolls 0.0 then 0.999, day. Expect 14 and 143.
  - `Loads.BellyCargoFromSpaceLeftAfterBags`:
    - 737-800 Two-class (holds 7,500 + 9,500, BBJ2-style limits but max payload 47,000), Passenger, day;
    - rolls 0, 0, 0 → 14 + 129 passengers, bags 7,865, cargo 913.
  - `Loads.CargoTrimmedToMaxPayload`: the same with max payload 36,000 and cargo r = 0.999. Expect cargo 965.
  - `Loads.FreighterLoad`: holds 40,000 + 7,500 + 9,500, max payload 52,000, r = 0 → 28,500.
  - `Loads.DeliveryHasNoLoadAndNoRolls`: the Roll counts its calls and stays at 0.
  - `Holds.SplitByRatio`: 4:5 holds (7,842 / 9,803), 9,000 → 4,000 / 5,000.
  - `Holds.OverflowSpillsToRoom`: forward max 1,000 ratio 4, aft max 9,000 ratio 5, 9,000 → 1,000 / 8,000.
  - `Holds.RemainderToLargestRatio`: 1,001 into 4:5 → 444 / 557.
  - `Dates.LocalHourInSeattle`:
    - `2026-11-23T03:00:00Z` → 19 (PST);
    - `2026-07-01T03:00:00Z` → 20 (PDT);
    - `"Not/AZone"` → nullopt.
- [ ] **Step 2: Run them.** Expected: compile errors.
- [ ] **Step 3: Implement** `loads.cpp` and `localHour`.
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** ("Roll loads and split holds (design.md §9, D-089)").

### Task 3: Pricing

**Files:** Create `src/pricing.h/.cpp`, `tests/pricing_test.cpp`; modify `CMakeLists.txt`.

**Interfaces:**
- Consumes: `FlightKind`, `CabinLoad` (Task 2), `Cents` and `roundToFiveDollars` (`money.h`).
- Produces (`pricing.h`):
  - `struct PriceInput`. Fields:
    - `FlightKind kind`
    - `std::string family` ("737" or "777")
    - `std::string role`
    - `int seatsInstalled`
    - `std::vector<CabinLoad> cabins`
    - `int cargoLb`
    - `double distanceNm`
    - `double blockHours`
    - `int mlwLb`
    - `std::optional<int> fuelBurnLb`
    - `Cents fuelPriceCentsPer100Lb`
  - `struct MoneyLine { std::string label; Cents cents; };`: income is positive, a cost is negative.
  - `struct Preview { std::vector<MoneyLine> income, costs; Cents totalCents; bool fuelIncluded; };`
  - `Preview pricePreview(const PriceInput&)`
  - `int pilotsFor(double blockHours)`
  - `int flightAttendantsFor(int seatsInstalled)`
  - `double estimatedBlockHours(double distanceNm, int cruiseSpeedKt)`, which is distance ÷ speed + 0.5.
- Rates are `constexpr` values at the top of `pricing.cpp`, copied from design.md §10–12 (D-104).
- Each line is computed in exact cents, rounded to cents, then to $5 (`roundToFiveDollars`). The total is the sum of
  the rounded lines. Lines worth 0 are left out.
- Income lines, in this order: "Economy fares", "Business fares", "First fares", "VIP fares", "Charter", "Cargo".
- Cost lines, in this order:
  - "Fuel" (only when `fuelBurnLb` is set);
  - "Landing fee";
  - "Departure / gate";
  - "Ground handling";
  - "GA services";
  - "ATC / navigation";
  - "Catering";
  - "Cargo handling";
  - "Pilots";
  - "Flight attendants";
  - "Maintenance".
- **Executive role:** VIP rates for crew and catering. **Charter:** "GA services" replaces gate and handling.
  **Delivery:** only fuel, landing fee, ATC, pilots and maintenance (D-104).
- **Crew:** a freighter (role) has no flight attendants. Flight attendants count seats installed, × 3 for
  executive.

- [ ] **Step 1: Write the failing tests** (900 nm, 2.5 h, MLW 146,300, no fuel unless stated):
  - `Pricing.VipFlight`: executive, 48 seats, 46 passengers.
    - Income: VIP fares 28,150.
    - Costs: landing 660, gate 300, handling 900, ATC 900, catering 460, pilots 1,250, flight attendants 565
      (562.50 rounds away from zero), maintenance 750.
    - Total 22,365.
  - `Pricing.CharterFlight`: the same airplane and passengers.
    - Income: Charter 18,750.
    - Costs: GA services 1,500, then landing, ATC, catering, pilots, flight attendants and maintenance as above.
    - Total 12,665.
  - `Pricing.CharterTwoHourMinimum`: 1.5 h → Charter 15,000.
  - `Pricing.AirlinerWithBellyCargo`: 737 airliner, 160 seats, 140 economy and 15 business, cargo 3,000.
    - Income: 22,050, 7,090 and 1,205.
    - Costs: 660, 300, 900, 900, catering 775, cargo handling 120, pilots 1,000, flight attendants 550,
      maintenance 750.
    - Total 24,390.
  - `Pricing.DeliveryIsALoss`: executive, Delivery. Costs only: 660, 900, 1,250, 750. Total −3,560.
  - `Pricing.FuelLineWhenBurnKnown`: burn 6,000 lb at 4,100 → Fuel −2,460, and `fuelIncluded` is true.
  - `Pricing.Crew`:
    - `flightAttendantsFor`: 19 → 0, 20 → 1, 50 → 1, 51 → 2, 100 → 2, 101 → 3, 150 → 3, 151 → 4;
    - `pilotsFor`: 8.0 → 2, 8.1 → 3, 12.0 → 3, 12.1 → 4.
  - `Pricing.CateringBands`: 46 passengers, not executive. At 2.9 h → 230; 3.0 h → 690; 8.0 h → 1,610.
  - `Pricing.EstimatedBlockHours`: 900 nm at 450 kt → 2.5.
- [ ] **Step 2: Run them.** Expected: compile errors.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** ("Preview pricing from design.md §10–12 (D-080, D-104)").

### Task 4: SimBrief, SayIntentions and the planning keys

**Files:**
- Create: `src/simbrief.h/.cpp`, `src/sayintentions.h/.cpp`, `tests/simbrief_test.cpp`, `tests/atis_test.cpp`,
  `tests/data/ofp_v2_min.json`, `tests/data/getwx_live.json`
- Modify: `src/app_settings.h/.cpp`, `tests/app_settings_test.cpp`, `CMakeLists.txt`

**Interfaces:**
- Produces (`simbrief.h`):
  - `struct DispatchRequest`. Fields:
    - `std::string origin, destination, airlineIcao, flightNumber, airframeId`
    - `std::string originRunway, destinationRunway`: "" = leave out
    - `int passengers, cargoLb`
  - `std::string dispatchUrl(const DispatchRequest&)` returns `https://dispatch.simbrief.com/options/custom?` with
    `orig`, `dest`, `airline`, `fltnum`, `type`, `pax`, `cargo` (in Dave's checked unit), `units=LBS`, then
    `origrwy` and `destrwy` when set, all percent-encoded.
  - `std::string ofpFetchUrl(std::string_view userId)` returns
    `https://www.simbrief.com/api/xml.fetcher.php?userid=<id>&json=v2&client=BlockTime`.
  - `struct SimbriefPlan`. Fields:
    - `std::string requestId, generatedAt, originIcao, destinationIcao, airframeId, units, route, originRunway,
      destinationRunway`
    - `int cruiseAltitudeFt, estBlockS, rampFuelLb, enrouteBurnLb, estLandingWeightLb,
      originTransitionAltitudeFt, destinationTransitionLevelFt`
  - `std::optional<SimbriefPlan> parseOfp(std::string_view json, std::string* error)`:
    - field paths are those in `docs/notes/simbrief-api.md`;
    - every value is a string in the OFP, so numbers are parsed from text;
    - `times.est_block` is `HH:MM:SS`;
    - if `fetch.status` isn't `Success`, the error is that status text;
    - a missing field gives "SimBrief plan has no <path>".
  - `struct PlanKey { std::string origin, destination, airframeId, registration, plannedAt; };`
  - `std::optional<std::string> planMismatch(const SimbriefPlan&, const PlanKey&)` returns nullopt when the plan
    fits, else the message (D-105). The checks, in order, with their exact messages:
    - `units` isn't `lbs`: "The SimBrief plan is in kilograms; BlockTime needs pounds."
    - the airports differ: "The latest SimBrief plan is KBFI–KSEA; this flight is KBFI–KSAN."
    - the airframe differs: "The latest SimBrief plan isn't for N814SX's SimBrief airframe."
    - generated more than 5 minutes before `plannedAt`: "The latest SimBrief plan was generated at 14:02Z, before
      this flight was planned. Press Generate on SimBrief, then import again."
- Produces (`sayintentions.h`):
  - `std::string getWxUrl(std::string_view apiKey, std::string_view origin, std::string_view destination)` returns
    `https://apipri.sayintentions.ai/sapi/getWX?api_key=<key>&icao=<origin>,<destination>`.
  - `struct AtisRunways { std::string origin, destination; };`: "" means none.
  - `AtisRunways parseAtisRunways(std::string_view json, std::string_view origin, std::string_view destination)`:
    - the origin takes the first of `active_runways_departing`, the destination the first of
      `active_runways_arriving`; either falls back to `active_runway`;
    - unparseable JSON, an `error` body or a missing airport gives "";
    - runway fields may be strings or numbers.
  - `std::string normalizeRunway(std::string_view)`: "9R" → "09R", "16L" → "16L", "27" → "27". Anything not
    `[0-9]{1,2}[LRC]?` gives "".
- Produces (`app_settings.h`):
  - `AppSettings` gains `simbriefUserId`, `sayIntentionsApiKey`, `sayIntentionsDeveloperKey`;
  - `std::vector<std::string> AppSettings::secrets() const`: the CARTO key and both SayIntentions keys, non-empty
    only;
  - `std::string redact(std::string_view text, const std::vector<std::string>& secrets)`.

- [ ] **Step 0:** Dave's cargo-unit check (above) is recorded in `docs/notes/simbrief-api.md`.
- [ ] **Step 1: Write the fixtures and failing tests.**
  - `ofp_v2_min.json` is hand-written, not copied from the research fixture. It holds:
    - `fetch.status` Success;
    - `params`: `request_id` 188511307, `time_generated` 2026-11-23T14:02:00Z, `units` lbs;
    - `origin`: KBFI, `plan_rwy` 14R, `trans_alt` 18000;
    - `destination`: KSAN, `plan_rwy` 27, `trans_level` 18000;
    - `general.route` "BANGR9 ... SEAVU2", `general.initial_altitude` 37000;
    - `times.est_block` 02:31:00;
    - `fuel.plan_ramp` 14200, `fuel.enroute_burn` 10900;
    - `weights.est_ldw` 125400;
    - `aircraft.internal_id` 730097_1790536204614.
  - `getwx_live.json` uses the live shape from `docs/notes/sayintentions-api.md`:
    - KBFI departing "14R,14L";
    - KSAN arriving "27";
    - a numeric `wind_direction`.
  - The tests:
    - `SimBrief.DispatchUrl`: the exact URL for KBFI→KSAN, XLY 101, N814SX's airframe, 46 passengers, cargo 0,
      runways 14R and 27.
    - `SimBrief.DispatchUrlWithoutRunways`: no `origrwy` or `destrwy`.
    - `SimBrief.ParsesOfp`: every field above, with `estBlockS` 9060.
    - `SimBrief.OfpErrorStatus`: the error is `Error: Unknown UserID`.
    - `SimBrief.PlanMismatchUnits`, `SimBrief.PlanMismatchAirports`, `SimBrief.PlanMismatchAirframe`: the exact
      messages above.
    - `SimBrief.PlanMismatchStale`: the plan was generated at 14:02Z, and it's stale when generated before
      `plannedAt` minus 5 minutes.
      - planned at 14:00Z: fine;
      - planned at 14:07Z: fine (the boundary);
      - planned at 14:08Z: stale, with the message above.
    - `SimBrief.PlanFits`: returns nullopt.
    - `Atis.ParsesActiveRunways`: 14R and 27.
    - `Atis.FallsBackToActiveRunway`: no list fields but `active_runway: "9"` gives 09.
    - `Atis.NoAirportsOrErrorGivesNone`: both `{"airports":[]}` and `{"error":"The provided API key is invalid."}`.
    - `Atis.NormalizeRunway`.
    - `AppSettings.ReadsPlanningKeys`.
    - `AppSettings.RedactsEverySecret`.
- [ ] **Step 2: Run them.** Expected: compile errors.
- [ ] **Step 3: Implement** with nlohmann (`parse(..., nullptr, false)`; no exceptions escape).
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** ("SimBrief dispatch URL and OFP import check, SayIntentions runways (D-101, D-105)").

### Task 5: The planned flight and the New Flight draft

**Files:**
- Create: `src/planned_flight.h/.cpp`, `src/flight_draft.h/.cpp`, `tests/planned_flight_test.cpp`,
  `tests/flight_draft_test.cpp`
- Modify: `src/airline_session.h/.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: Tasks 1–4.
- Produces (`planned_flight.h`):
  - `struct FlightAirplane`. Fields:
    - `std::int64_t airplaneId`
    - `std::string registration, typeCode, family, role, simbriefAirframeId, originIcao, originName,
      originTimeZone`
    - `double originLat, originLon`
    - `int cruiseSpeedKt`
    - `bool awaitingDelivery`
    - `std::string freeFlightNumber`: "" = none (D-106)
    - `std::vector<Cabin> cabins; std::vector<Hold> holds; AirplaneLimits limits`
  - `std::optional<FlightAirplane> loadFlightAirplane(db::Database&, std::string_view registration)`: nullopt if
    the airplane is unknown or returned.
  - `std::vector<FlightKind> allowedKinds(const FlightAirplane&)`:
    - awaiting delivery → `{Delivery}`;
    - executive → `{Vip, Charter}`;
    - airliner → `{Passenger}`;
    - freighter → `{Cargo}`.
  - `struct DestinationCheck`. Fields: `bool ok; std::string message, icao, name; double lat = 0, lon = 0,
    distanceNm = 0;`
  - `DestinationCheck checkDestination(db::Database&, const FlightAirplane&, std::string_view typed)`. It trims
    and uppercases the input. The exact messages:
    - "Type a 4-letter ICAO code."
    - "KXYZ isn't in the airport data."
    - "N814SX is already at KBFI."
    - "KSHT's longest runway is 5,000 ft; N814SX needs 7,000 ft."
    - "" when it's OK.

    The distance is great-circle (`geo.h`) in nm.
  - `bool validFlightNumber(std::string_view)`: 1–4 digits.
  - `struct NewPlannedFlight`. Fields:
    - `std::int64_t airplaneId`
    - `std::string originIcao, destinationIcao, flightNumber`
    - `FlightKind kind`
    - `std::vector<CabinLoad> cabins; int cargoLb`
    - `Cents fuelPriceCentsPer100Lb`
    - `std::string plannedAt, originRunway, destinationRunway`: "" = NULL
  - `std::optional<std::int64_t> createPlannedFlight(db::Database&, const NewPlannedFlight&, std::string* error)`:
    - one transaction;
    - refuses while any planned flight exists: "XLY101 KBFI–KSAN is already planned." (D-103).
  - `struct PlannedFlightView`. Fields:
    - `std::int64_t id`
    - `std::string registration, typeCode, airframeId, origin, originName, destination, destinationName,
      flightNumber`
    - `FlightKind kind`
    - the same pricing inputs as `FlightAirplane` (`family, role, seatsInstalled, cruiseSpeedKt, limits`)
    - `std::vector<CabinLoad> cabins; int cargoLb; double distanceNm; Cents fuelPriceCentsPer100Lb`
    - `std::string plannedAt, originRunway, destinationRunway`
    - `std::optional<SimbriefPlan> plan`
  - `std::optional<PlannedFlightView> currentPlannedFlight(db::Database&)`
  - `bool cancelPlannedFlight(db::Database&, std::string* error)`
  - `bool storeSimbriefPlan(db::Database&, std::int64_t plannedFlightId, const SimbriefPlan&,
    std::string_view ofpJson, std::string_view importedAt, std::string* error)`: inserts, or replaces the row.
  - `PriceInput priceInput(const PlannedFlightView&)`:
    - with a plan: the plan's block time (`estBlockS / 3600.0`) and en-route burn;
    - without: `estimatedBlockHours` and no fuel.
  - `bool nearMlw(const PlannedFlightView&)`: the plan's estimated landing weight ≥ 95% of MLW.
- Produces (`flight_draft.h`), New Flight's state, so toggles never re-roll:

  ```cpp
  class FlightDraft {
  public:
      FlightDraft(FlightAirplane airplane, Cents fuelPrice, bool daytime, Roll roll);
      const FlightAirplane& airplane() const;
      const std::vector<FlightKind>& kinds() const;
      FlightKind kind() const;
      void setKind(FlightKind);          // rolls that kind the first time it's chosen, keeps it after
      bool bellyCargoAvailable() const;  // airliner role only
      bool bellyCargo() const;           // starts on (D-089)
      void setBellyCargo(bool);
      void setDestination(DestinationCheck);
      void setFlightNumber(std::string);
      LoadRoll loads() const;            // cargoLb 0 when belly cargo is off
      std::vector<HoldLoad> holds() const;   // bags + cargo split
      std::optional<Preview> preview() const;  // nullopt until the destination is ok
      bool canPlan() const;              // destination ok and valid flight number
      NewPlannedFlight toNew(std::string plannedAt, const AtisRunways&) const;
  };
  ```

  The first kind in `kinds()` is chosen at construction, and the flight number starts as the airplane's
  `freeFlightNumber` (D-106). Add `Draft.FlightNumberStartsAsTheAirplanesDefault`.
- `AirlineSession` gains:
  - `db::Database* database()`: nullptr when the database didn't open;
  - `std::string airlineIcao() const`;
  - `Cents fuelPriceCentsPer100Lb() const`.

- [ ] **Step 1: Write the failing tests** (on `testAirline()`):
  - `Planned.KindsByRole`: N814SX → `{Delivery}`. After its `awaiting_delivery` is set to 0, `{Vip, Charter}`.
    N738BT → `{Passenger}`.
  - `Planned.DestinationChecks`:
    - "" and "KS" → the ICAO message;
    - "KXYZ" → unknown;
    - "kbfi" → already there;
    - "KSHT" → runway;
    - " ksan " → ok, KSAN, a distance of 918 ± 1 nm.
  - `Planned.CreateLoadCancel`: create a Vip flight with 46 economy passengers and runways 14R / "". Then:
    - `currentPlannedFlight` returns it, `destinationRunway` "" and `plan` empty;
    - after cancel, nullopt;
    - no `planned_flight_cabin` rows are left.
  - `Planned.OnlyOneAtATime`: a second create fails with the exact message.
  - `Planned.StorePlanReplaces`: store twice with different `requestId`s. One row, the second id.
  - `Planned.SurvivesReopening`: on a temp file, create, close the database, reopen, and `currentPlannedFlight`
    has the same fields.
  - `Planned.PriceInputUsesThePlanOnceImported`: without a plan, 2.54 ± 0.01 h and no fuel. With the fixture's
    plan, 2.5167 h and burn 10,900.
  - `Planned.NearMlw`: with MLW 146,300, an estimated landing weight of 139,000 → true and 138,900 → false.
  - `Draft.TogglingBellyCargoDoesNotReroll`:
    - for N738BT, count the Roll's calls;
    - toggle belly cargo off and on three times: no new calls;
    - `loads().cargoLb` is 0 when off and the rolled value when on.
  - `Draft.SwitchingKindKeepsEachKindsRoll`: for N814SX delivered, go Vip → Charter → Vip. The Vip passengers are
    unchanged, and there are only two rolls in total, one per kind (48 economy seats, one cabin).
  - `Draft.DeliveryPreviewIsALoss`: N814SX, destination KSAN: no income lines, and the total is below 0.
  - `Draft.CannotPlanWithoutFlightNumber`: destination ok, flight number "" → false, "101" → true, "10A" → false.
- [ ] **Step 2: Run them.** Expected: compile errors.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run.** Expected: PASS.
- [ ] **Step 5: Commit** ("Planned flight and the New Flight draft (D-089, D-103)").

### Task 6: Qt side: network, models and wiring

**Files:**
- Create:
  - `src/planner.h`: the interface, plus `QtPlanner` (`src/qt_planner.h/.cpp`, Qt Network)
  - `src/new_flight_model.h/.cpp`
  - `src/planned_flight_model.h/.cpp`
- Modify: `src/fleet_table_model.h/.cpp`, `src/app_main.cpp`, `CMakeLists.txt` (add `Network` to `find_package`,
  link `Qt6::Network` to the app and the Qt tests, add the new sources to both), `tests/qt_models_test.cpp`

**Interfaces:**
- `planner.h`:

  ```cpp
  class Planner {
  public:
      virtual ~Planner() = default;
      // getWX for both airports; done is always called once, with empty runways and a note on any failure.
      virtual void fetchRunways(QString origin, QString destination,
                                std::function<void(blocktime::AtisRunways, QString note)> done) = 0;
      // The latest OFP; done gets the body (even on HTTP 400, which carries SimBrief's status) or a network error.
      virtual void fetchLatestOfp(std::function<void(QByteArray body, QString networkError)> done) = 0;
      virtual void openUrl(const QString& url) = 0;
  };
  ```

  - `QtPlanner(blocktime::AppSettings, QObject* parent)` uses one `QNetworkAccessManager`:
    - getWX has an 8 s transfer timeout and the `X-Developer-Key` header;
    - the OFP fetch has a 15 s timeout;
    - `openUrl` is `QDesktopServices::openUrl`.
  - Without SayIntentions keys, `fetchRunways` calls `done` at once with the note "No SayIntentions keys in
    settings.json". Without `simbrief_user_id`, `fetchLatestOfp` gives the error "Add simbrief_user_id to
    settings.json".
- `NewFlightModel` (`QML_ELEMENT`, uncreatable), built with `AirlineSession&` and `Planner&`:
  - `Q_INVOKABLE bool open(QString registration)`: builds a `FlightDraft` with a seeded `mt19937_64`. Day or night
    comes from the origin's local hour now.
  - Read/write properties:
    - `kind`: a string code; `kinds` is the list;
    - `bellyCargo`;
    - `destination`: setting it runs `checkDestination`;
    - `flightNumber`: starts as the airplane's default (D-106).
  - Read-only properties:
    - `registration`, `airplaneText` ("N814SX · 737-800 BBJ2"), `origin`, `bellyCargoAvailable`;
    - `destinationMessage`;
    - `loadLines` (`QStringList`, e.g. "46 VIP passengers", "Bags 2,530 lb", "Forward 1,124 lb · Aft 1,406 lb");
    - `incomeLines` and `costLines` (`QVariantList` of `{label, amount}`, amount as `formatMoneyFull`);
    - `totalText`;
    - `fuelNote` ("+ fuel" when not included);
    - `endsUpText` ("N814SX ends up at KSAN");
    - `canPlan`, `busy`, `error`.
  - `Q_INVOKABLE void plan()`:
    1. `busy`.
    2. `fetchRunways`.
    3. `createPlannedFlight`, with `plannedAt` set to the UTC time of the click.
    4. `openUrl(dispatchUrl(...))`.
    5. Emit `planned()`.

    On a create error, `error` is set and nothing opens.
- `PlannedFlightModel` (`QML_ELEMENT`, uncreatable), built with `AirlineSession&`, `Planner&` and
  `RecorderBridge&`:
  - Properties:
    - `hasFlight`;
    - `state`: `"waitingForPlan"`, `"waitingForSim"` (plan in, not connected), or `"simConnected"`;
    - `stateText`: "Waiting for plan", "Waiting for sim", "Sim connected";
    - `title` ("XLY101 · KBFI → KSAN"), `airplaneText`, `kindText`;
    - `loadInText`: "Load in at KBFI, Boeing Field";
    - `runwayText`: "ATIS runways: 14R → 27", or "No ATIS runways: SimBrief chose";
    - `route`;
    - `planText` ("Plan generated 14:02Z · FL370 · 2h 31m · burn 10,900 lb");
    - `estimateLines` and `totalText`;
    - `warning`: the MLW warning text, or "";
    - `message`: the latest import result or error;
    - `busy`.
  - `Q_INVOKABLE` methods:
    - `openDispatchPage()`: rebuilds the URL from the stored flight;
    - `importPlan()`: fetch, then `parseOfp`, then `planMismatch`, then `storeSimbriefPlan`; the message is
      "Imported the plan generated 14:02Z" or the refusal;
    - `cancelFlight()`;
    - `copyRoute()`: `QGuiApplication::clipboard()`;
    - `reload()`.
  - Signal: `flightChanged()`, which the Fleet model's refresh listens to.
- `FleetTableModel` gains `Q_INVOKABLE QString registrationAt(int row) const`.
- `app_main.cpp`:
  - builds a `QtPlanner` from `loadAppSettings`;
  - builds both models and passes them as initial properties `newFlight` and `plannedFlight`;
  - installs the message handler with `AppSettings::secrets()`.

- [ ] **Step 1: Write the failing Qt tests** with a `FakePlanner`. It records opened URLs, returns canned runways
  and an OFP body, and calls back synchronously. Use `testAirline()` with N814SX delivered.
  - `NewFlightModel.PlanWritesTheFlightAndOpensDispatch`: open N814SX, kind `vip`, destination KSAN, number 101.
    `plan()` leaves one planned flight, and the opened URL contains `origrwy=14R&destrwy=27`.
  - `NewFlightModel.PlanWithoutAtisStillPlans`: the fake returns empty runways with a note. The URL has no
    `origrwy`, and the planned flight's runways are "".
  - `NewFlightModel.DestinationMessageUpdates`: "KSHT" → the runway message, and `canPlan` is false.
  - `PlannedFlightModel.ImportStoresMatchingPlan`: the fixture OFP. State `waitingForSim`, `planText` set, `message`
    "Imported the plan generated 14:02Z".
  - `PlannedFlightModel.ImportRefusesAStalePlan`: planned at 14:20Z. Nothing is stored, the state stays
    `waitingForPlan`, and `message` is the stale text.
  - `PlannedFlightModel.ImportShowsSimbriefError`: a 400 body with `Error: Unknown UserID`. `message` contains it.
  - `PlannedFlightModel.CancelClears`: `hasFlight` becomes false and `flightChanged` is emitted.
  - `PlannedFlightModel.StateFollowsConnection`: with a plan in, `bridge.applyConnection(Connected)` →
    `simConnected`.
- [ ] **Step 2: Run them.** Expected: compile errors.
- [ ] **Step 3: Implement** the models, `QtPlanner` and the wiring.
- [ ] **Step 4: Run all tests and build the app.** Expected: PASS, no warnings.
- [ ] **Step 5: Commit** ("New Flight and planned-flight models, SimBrief and SayIntentions calls (D-101, D-105)").

### Task 7: The screens

**Files:**
- Create: `qml/NewFlightDialog.qml`, `qml/PlannedFlightPanel.qml`
- Modify: `qml/Main.qml`, `qml/FleetPage.qml`, `qml/LivePage.qml`, `qml/Theme.qml` (only if a new size is needed),
  `CMakeLists.txt` (`BLOCKTIME_QML_FILES`)

The look: Card, PrimaryButton and Theme, like the existing pages.
- **Fleet:** clicking a row selects it (highlighted with `Theme.surfaceRaised` plus an accent left border). A
  **New Flight** button in the card's header row is enabled when an airplane is selected and no flight is planned.
  When a flight is planned, a hint next to it says "XLY101 KBFI–KSAN is planned (Live)". The button opens the dialog.
- **NewFlightDialog** (a modal `Dialog`), in this order:
  - the airplane and origin;
  - the destination field, with its message under it in `Theme.warn`;
  - the flight number field;
  - the kind choice: a row of buttons when `kinds.length > 1`, otherwise a line such as "Delivery flight: no
    passengers or cargo";
  - the belly cargo checkbox when available;
  - the load lines;
  - the income and cost lines (two columns: label left, amount right, tabular figures);
  - the total and `fuelNote`;
  - `endsUpText`;
  - **Plan flight**, enabled by `canPlan` and showing "Getting runways…" while `busy`;
  - Cancel.

  On `planned()` it closes and the app switches to the Live page.
- **PlannedFlightPanel**, on the Live page above the flight panel, when `plannedFlight.hasFlight`:
  - the title and `stateText` (a StatusPill-like pill), the airplane, `loadInText`, `runwayText`;
  - the route in a monospace box, with **Copy route**;
  - `planText`, the estimate lines and the total, and the `warning` in `Theme.warn`;
  - `message`;
  - buttons: **Import Active SimBrief Plan** (primary while waiting for the plan), **Open SimBrief again**,
    **Cancel flight**.

  Cancel flight asks first in a small confirm dialog: "Cancel XLY101 KBFI–KSAN? It's free."

- [ ] **Step 1:** Build debug and run `BlockTime.exe --demo` with hot reload. Go through: select N814SX → New
  Flight → type KSAN and 101 → Plan flight. (Against a scratch data folder: copy Dave's configs and a database
  holding KBFI and KSAN, so his real database isn't touched.)
- [ ] **Step 2:** Check that nothing clips at the minimum window size (1100 × 600), in both the dialog and the
  panel.
- [ ] **Step 3:** `blocktime-app_qmllint` reports nothing.
- [ ] **Step 4: Commit** ("New Flight dialog and the before-departure panel (design.md §13.1–13.3)").

### Task 8: Docs, version, review, and Dave's checks

**Files:** `CMakeLists.txt`, `vcpkg.json` (0.6.0), `README.md`, `docs/design.md`, `docs/database.md`,
`docs/progress.md`, `docs/decisions.md` (any new calls).

- [ ] **Step 1:** Version 0.6.0. In `README.md`, document `settings.json`'s three new keys (and where to find each)
  and the New Flight flow, and mention `fuel_price_per_100lb` and `limits` in the airline-setup section.
- [ ] **Step 2:** Make sure `docs/database.md` says milestone 5 is built and matches migration 3. Update the specs
  if the implementation differs.
- [ ] **Step 3:** Use `verification-before-completion`: a clean release and debug build with no warnings, all tests
  passing, and qmllint clean.
- [ ] **Step 4:** Use `requesting-code-review` for the whole milestone, then `receiving-code-review`. Fix Critical
  and Important findings, and list deferred Minor ones in progress.md.
- [ ] **Step 5:** Write the progress entry, with these checks for Dave (no sim needed until the last):
  1. Add the three keys to `settings.json`. Start BlockTime. Fleet shows N814SX as before, and errors.log has only
     its "started" line.
  2. Select N814SX and click New Flight. The dialog offers only the delivery flight. Type KSAN and a flight number.
     The preview shows costs only (a loss) and "+ fuel", and says "N814SX ends up at KSAN".
  3. Plan flight. The browser opens SimBrief's dispatch page with KBFI, KSAN, XLY and the number, N814SX's airframe,
     0 passengers, and the ATIS runways if SayIntentions has them. Live shows "Waiting for plan".
  4. Click Import before pressing Generate. The panel says the latest plan is older (or for another route). Press
     Generate, then Import. The panel shows the plan, the route, and the estimate with fuel.
  5. Close and reopen BlockTime. The planned flight is still on Live, with its plan.
  6. Copy route and paste it into Navigraph Charts.
  7. Cancel flight, then plan it again (the delivery flight stays to fly in milestone 6).
- [ ] **Step 6: Commit** ("Milestone 5 built (version 0.6.0)").
