# Milestone 15b — Fleet Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (CLAUDE.md, D-033: this project doesn't use
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking. Commit to `main` after each task
> (D-038).

**Goal:** Fleet becomes the slots screen; airplanes are leased in the app from a SimBrief screenshot read by Claude,
with factory or instant delivery; each has an aircraft screen with New screenshot and Return; supported aircraft are
shipped data files.

**Architecture:** Plain C++ in `blocktime-core` for everything that decides (the aircraft files, the slot rule, layout
matching, the hold split, the refund, the delivery price, the lease / return / re-read transactions, the Claude request
and answer). Three Qt models expose it to three QML pages. The Claude call reuses `ReviewClient` / `ClaudeReviewer`.

**Tech Stack:** C++20, MSVC, CMake + Ninja, Qt 6 QML, SQLite (sqlite3 C API), yaml-cpp (via `config_yaml.h`),
nlohmann::json, GoogleTest.

**Spec:** `docs/fleet.md` (approved 2026-11-27), decisions D-239 to D-246. Mockups: `docs/mockups/fleet/`.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`; `blocktime-app_qmllint` reports nothing new (today: one info line).
- QML only displays. Every colour and size in `qml/Theme.qml`. Every QML file starts with the GPL header and
  `import BlockTime`; `pragma ComponentBehavior: Bound`; `required` properties in delegates. No continuous animation.
- Database (D-078): `STRICT`, `NOT NULL` by default, foreign keys with explicit `ON DELETE`, an index on every foreign
  key, money in integer cents and every line a multiple of 500 (D-080), several-table changes in one transaction,
  ledger lines never updated.
- Config files get the happy path only (D-099); network calls get real error handling.
- The Claude key is never logged or shown (`app_settings.h`'s scrubbing). The screenshot isn't kept.
- Slots: `kStartingSlots = 2`, one per 10 h, `kMaxSlots = 20`.
- Model `claude-opus-5-5` (`kReviewModel`), same URL, version and beta headers as the debrief.
- Delivery fee: $8 per nm for 737 types, $18 for 777 types. Factories: KBFI for 737s, KPAE for 777s.
- Debug build checks use `--demo` (scratch copy, D-226); never touch Dave's airline except the migration-11 dry run on a
  copy in Task 9.

## Review Focus

1. **A screenshot with values Claude couldn't read** (a null field, or a field cut off the image): the form leaves it
   empty and marked, and Lease stays greyed until it's filled. Test in Task 7 (`AddAircraftModel` with a reading
   missing `mlwLb`).
2. **Two layouts with the same seat count** in the candidate list (e.g. two types sharing a base type): the first in
   file order is pre-selected and the other stays pickable. Test in Task 2 (`candidateLayouts`).
3. **Returning on the lease month's first or last day**: refund all but one day, or $0, never negative. Test in Task 2.
4. **Instant delivery to the factory itself**: no charge, the plane starts there delivered. Test in Task 3.
5. **A lease whose cash check passes in the form but not in the transaction** (cash changed by the minute tick
   between): nothing written, the reason shown. Test in Task 3 (`leaseAirplane` refuses with cash 1 cent short).

---

### Task 1: Supported aircraft as shipped files

Replace `config/catalog.yaml` with `data/aircraft/*.yaml`, one file per type, keeping the `Catalog` structs so callers
change little.

**Files:**
- Create: `data/aircraft/pmdg-737-600.yaml`, `pmdg-737-800.yaml`, `pmdg-737-800-bbj2.yaml`, `pmdg-737-800-bcf.yaml`,
  `pmdg-777-200er.yaml`, `pmdg-777-300er.yaml`, `pmdg-777f.yaml`
- Modify: `src/catalog.h`, `src/catalog.cpp`, `tests/catalog_test.cpp`, `CMakeLists.txt` (copy `data/aircraft` next
  to `BlockTime.exe` post-build and into the install/deploy step), `src/airline_session.cpp` (`AirlinePaths.catalog`
  becomes a directory; the "No catalog.yaml" notice becomes "No aircraft files in …")
- Delete: `config/catalog.yaml`

**Interfaces:**
- Produces:
  - `struct CatalogType` gains `std::vector<std::string> simbriefBaseTypes; std::string factoryIcao; int deliveryFeePerNmDollars;`
  - `struct Hold { std::string hold; int shareLb, ratio; }` (`maxLb` renamed: in a layout it's a share; on an airplane
    the split result is still stored as `max_lb`)
  - `struct CatalogAirframe` loses `limits` (the layout; keep the name `CatalogAirframe`, the field `name` is the
    layout name, `typeCode` its type)
  - `std::optional<Catalog> parseAircraftFile(std::string_view yaml, std::string* error);` → one type and its layouts
  - `Catalog loadCatalogDir(const std::filesystem::path& dir, std::vector<std::string>* notices);` → every `*.yaml`
    in file-name order; a file that doesn't parse is skipped with a notice
  - `std::filesystem::path defaultCatalogDir();` → `<exe dir>/data/aircraft`
  - `AirplaneLimits` stays (it's now the airplane's, written from a reading)

Each file has the keys of `docs/fleet.md`'s example: `code, family, role, turnaround_min, cruise_speed_kt,
lease_per_month, sim_folder, wingspan_m, simbrief_base_types, factory, delivery_fee_per_nm, layouts[name, cabins,
holds[hold, share_lb, ratio]]`. Values come from today's `config/catalog.yaml` (each airframe's `max_lb` becomes
`share_lb`). `simbrief_base_types`: `B736` (737-600), `B738` (737-800, BBJ2, BCF), `B772` (777-200ER), `B77W`
(777-300ER), `B77L` (777F); Dave corrects them from real screenshots. No `fuel_burn_lb_per_nm` (D-119 isn't built).
No `recorder:` key yet.

- [ ] **Step 1: Write the failing tests** in `tests/catalog_test.cpp` (replace the catalog.yaml ones):
  - `TEST(AircraftFiles, ShippedFilesAllLoad)`: `loadCatalogDir(BLOCKTIME_SOURCE_DIR "/data/aircraft", &notices)`
    gives 7 types, `notices` empty, and the same seven `sim_folder`s as before (`PMDG 737-600` … `PMDG 777F`).
  - `TEST(AircraftFiles, ParsesTypeAndLayouts)`: a 737-800 snippet gives `simbriefBaseTypes == {"B738"}`,
    `factoryIcao == "KBFI"`, `deliveryFeePerNmDollars == 8`, two layouts, the first with holds `{forward, 7500, 4}`,
    `{aft, 9500, 5}`.
  - `TEST(AircraftFiles, BadFileIsSkippedWithNotice)`: a temp dir with one good and one malformed file gives 1 type
    and one notice naming the bad file.
- [ ] **Step 2: Run** `ctest --preset debug -R AircraftFiles` → FAIL (functions not defined).
- [ ] **Step 3: Implement** the parser (reuse `config_yaml.h` helpers and today's validation in `catalog.cpp`), the
  seven files, the CMake copy, `defaultCatalogDir()`. Point `AirlineSession` and `tests/test_airline.h` at the dir.
  Remove `CatalogAirframe::limits` users: `fillMissingLimits` and `LimitOverrides` go in Task 3; until then
  `fillMissingLimits` is a no-op stub returning true (deleted in Task 3).
- [ ] **Step 4: Run** the full suite `ctest --preset debug` → PASS (the old catalog tests are gone, nothing else broke).
- [ ] **Step 5: Commit** `Aircraft files replace catalog.yaml (D-243)`.

### Task 2: The fleet rules (pure logic)

**Files:**
- Create: `src/fleet_rules.h`, `src/fleet_rules.cpp`, `tests/fleet_rules_test.cpp`
- Modify: `src/leasing.h`, `src/leasing.cpp` (slot rule), `src/pricing.h`, `src/pricing.cpp` (a public cost-line
  builder), `tests/leasing_test.cpp` (slot expectations), `CMakeLists.txt`

**Interfaces:**
- Consumes: `Catalog`, `CatalogType`, `CatalogAirframe`, `Hold` (Task 1); `pricePreview`, `PriceInput`, `Rates`.
- Produces (namespace `blocktime`):
  - `inline constexpr int kMaxSlots = 20;` in `leasing.h`; `slotsEarned(h) = min(20, 2 + floor(h / 10))`;
    `hoursForSlot(n) = n <= 2 ? 0 : 10.0 * (n - 2)`.
  - `struct SimbriefReading { std::string registration, finNumber, internalId, baseType, weightUnits; std::optional<int>
    maxPassengers, passengerWeightLb, bagWeightLb, emptyWeightLb, mzfwLb, mtowLb, mlwLb, maxFuelLb, maxCargoLb,
    minRunwayFt; };` (an empty string or nullopt = not read)
  - `std::vector<std::string> missingFields(const SimbriefReading&);` → labels of unset fields, in form order
  - `struct LayoutChoice { const CatalogType* type; const CatalogAirframe* layout; int seats; bool seatsMatch; };`
  - `std::vector<LayoutChoice> candidateLayouts(const Catalog&, std::string_view baseType, int maxPassengers);` →
    every layout of every type accepting `baseType`, file order
  - `std::optional<std::size_t> preselectedLayout(const std::vector<LayoutChoice>&);` → first with `seatsMatch`
  - `std::vector<Hold> splitHolds(const std::vector<Hold>& layoutHolds, int maxCargoLb);` → `shareLb` holds the
    airplane's maximum
  - `Cents leaseRefundCents(Cents monthlyCents, Date leasedOn, Date returnedOn);`
  - `MoneyLine costLine(std::string category, std::string label, std::vector<Quantity> quantities, double rateDollars);`
    in `pricing.h`, the public form of `pricing.cpp`'s `cost()` (negative, rounded to $5)
  - `Preview instantDeliveryPrice(const CatalogType& type, double distanceNm, int mlwLb, const Rates& rates);` →
    `pricePreview` with `kind = Delivery`, the type's family/role/cruise speed, no fuel; then the line
    `costLine("delivery_fee", "Delivery fee", {{distanceNm, "nm"}}, type.deliveryFeePerNmDollars)` first in `costs`,
    total updated

Refund: `start = leasePeriodStart(leasedOn, k)` for the period containing `returnedOn`, `next` the one after;
`unused = days(next − returnedOn) − 1`; `round5(monthly × unused / days(next − start))`, never below 0.

- [ ] **Step 1: Write the failing tests** in `tests/fleet_rules_test.cpp` and `tests/leasing_test.cpp`:
  - `Slots.TwoAtStartOnePerTenHoursCappedAtTwenty`: `slotsEarned(0)==2`, `(9.9)==2`, `(10)==3`, `(23.4)==4`,
    `(180)==20`, `(500)==20`; `hoursForSlot(3)==10`, `(5)==30`, `(20)==180`.
  - `Layouts.SeatsPickTheLayout`: catalog from `data/aircraft`, `candidateLayouts(c, "B738", 160)` contains the
    737-800 Two-class with `seatsMatch`, All-economy (184) without, and the BBJ2 VIP (48) and BCF (0) without;
    `preselectedLayout` is the Two-class.
  - `Layouts.BbjAndBcfBySeats`: `("B738", 48)` preselects `737-800 BBJ2 VIP`; `("B738", 0)` the BCF layout.
  - `Layouts.TieGoesToFileOrder` (Review Focus 2): a two-type test catalog with equal seats preselects index 0 and
    both have `seatsMatch`.
  - `Layouts.UnknownBaseTypeGivesNone`: `("A20N", 150)` is empty.
  - `Holds.SplitByShareRemainderLast`: `{7500,9500}` at 18000 → `{7941, 10059}`; ratios kept `4, 5`.
  - `Holds.ThreeHolds`: `{10000, 12000, 3000}` at 50000 → `{20000, 24000, 6000}`; at 50001 the last gets `6001`.
  - `Refund.UnusedDaysFromTomorrow`: monthly $150,000, leased 2026-11-22, returned 2026-11-27 → period 22 Nov–21 Dec
    (30 days), unused 24 → $120,000.
  - `Refund.FirstAndLastDay` (Review Focus 3): returned 2026-11-22 → 29/30 → $145,000; returned 2026-12-21 → $0.
  - `Refund.RoundsToFiveDollars`: monthly $235,000 over a 31-day month, 10 unused → $75,805 rounded to a multiple of
    500 cents.
  - `Delivery.FeeReplacesFuel`: 737-800 type, 943 nm, MLW 146,300, test rates → `costs[0]` is `delivery_fee` with
    working "943 nm × $8" and −$7,545; no `fuel` line; a `pilots` line and no `flight_attendants`; `totalCents` the
    sum of the lines.
- [ ] **Step 2: Run** `ctest --preset debug -R "Slots|Layouts|Holds|Refund|Delivery"` → FAIL.
- [ ] **Step 3: Implement** the functions above; update `settleLeases`'s slot notice text if it quotes the old curve.
- [ ] **Step 4: Run** the full suite → PASS.
- [ ] **Step 5: Commit** `Fleet rules: slots, layouts, holds, refund, delivery price (D-240, D-242, D-245, D-246)`.

### Task 3: Migration 11 and leasing in the app

**Files:**
- Modify: `src/airline_db.h` (`kAirlineSchemaVersion = 11`), `src/airline_db.cpp` (migration 11, `rebuild = true`),
  `src/leasing.h`, `src/leasing.cpp`, `src/airline_config.h`, `src/airline_config.cpp`, `src/airline_session.h`,
  `src/airline_session.cpp`, `src/airline_queries.h`, `src/airline_queries.cpp` (drop `fleet`; ledger text for the
  new categories), `src/fleet_table_model.cpp` (drop the Fleet column until Task 6 deletes it),
  `config/airline.example.yaml`, `tests/test_airline.h`, `tests/airline_db_test.cpp`, `tests/leasing_test.cpp`,
  `tests/airline_config_test.cpp`, `tests/airline_session_test.cpp`

**Migration 11** (one rebuild of `airplane`, like migration 10):
- `airplane` without `fleet_id`; `registration`, `simbrief_airframe_id` lose `UNIQUE`; partial unique indexes
  `airplane_registration_in_fleet`, `airplane_simbrief_in_fleet`, `airplane_free_flight_number` (the last replaces the
  plain one) `WHERE returned_on IS NULL`. Drop `airplane_fleet` index and the `fleet` table.
- `airplane_simbrief (airplane_id INTEGER PK REFERENCES airplane ON DELETE CASCADE, read_at TEXT instant, base_type
  TEXT, max_passengers ≥ 0, empty_weight_lb, mzfw_lb, mtow_lb, max_fuel_lb, max_cargo_lb INTEGER > 0)` STRICT.
- `INSERT INTO return_reason VALUES ('returned')`; ledger categories `('delivery_fee', 'Delivery fee')`,
  `('lease_refund', 'Lease refund')`.
- `instant_delivery (instant_delivery_id INTEGER PK, airplane_id INTEGER NOT NULL UNIQUE REFERENCES airplane ON
  DELETE RESTRICT, airport_id INTEGER NOT NULL REFERENCES airport ON DELETE RESTRICT)` + index on `airport_id`;
  `instant_delivery_line (ledger_line_id INTEGER PK REFERENCES ledger_line ON DELETE RESTRICT, instant_delivery_id
  INTEGER NOT NULL REFERENCES instant_delivery ON DELETE RESTRICT)` + index.
- `lease_refund (ledger_line_id INTEGER PK REFERENCES ledger_line ON DELETE RESTRICT, airplane_id INTEGER NOT NULL
  UNIQUE REFERENCES airplane ON DELETE RESTRICT)`.

**Interfaces:**
- Consumes: Task 1's `Catalog`; Task 2's `SimbriefReading`, `LayoutChoice`, `splitHolds`, `instantDeliveryPrice`,
  `slotsEarned`.
- Produces (in `leasing.h`):
  - `struct LeaseRequest { SimbriefReading reading; const CatalogType* type; const CatalogAirframe* layout;
    std::string flightNumber; std::optional<std::string> instantDeliveryIcao; Date today; };` (nullopt = factory)
  - `struct LeaseResult { std::optional<std::int64_t> airplaneId; std::string refusal; };` (refusal is the
    sentence the form shows: "Not enough cash: $248,270 needed, $200,000 in cash.")
  - `LeaseResult leaseAirplane(db::Database&, const LeaseRequest&, const Rates&);` one transaction: re-checks
    slots, the reading complete, seats match, registration / Internal ID / flight number free in the fleet, the
    delivery airport known with a long enough runway, cash ≥ first month + delivery; inserts airplane (tail = Fin
    Number or NULL, `airframe_name` = layout name, `simbrief_airframe_id` = Internal ID, location = factory or the
    delivery airport, fuel 0, `awaiting_delivery` = factory and not already there), cabins, split holds, limits
    (`max_payload = mzfw − empty`), `airplane_simbrief`, the first lease month, and the delivery lines.
  - `std::string leaseRefusal(db::Database&, const LeaseRequest&, const Rates&);` the same checks without writing
    ("" = would lease), for the form.
  - `settleLeases(db, today, std::optional<int> slots)` keeps charging months and returning unpaid airplanes; it no
    longer takes `Catalog` or `AirlineConfig` and leases nothing.
  - Removed: `syncFleets`, `fillMissingLimits`, `syncFreeFlightNumbers`, `configDriftWarnings`, `LimitOverrides`,
    `withOverrides`, `ConfigAirplane`, `AirlineConfig::fleets/airplanes`, `AirplaneView::fleet`.
  - `AirlineSession` gains `const Catalog& catalog() const;` and `Rates rates() const;` for the models.

- [ ] **Step 1: Write the failing tests:**
  - `AirlineDb.Migration11FromEmpty`: an empty DB migrates to 11; `PRAGMA foreign_key_check` empty; `fleet` gone;
    `EXPLAIN QUERY PLAN` for `SELECT … FROM airplane WHERE registration = ? AND returned_on IS NULL` uses
    `airplane_registration_in_fleet`.
  - `AirlineDb.Migration11KeepsAirplanes`: migrate to 10, insert a fleet and two airplanes with limits and a lease
    charge, migrate to 11 → both airplanes with every column but `fleet_id` unchanged, the charge still points at
    its airplane, no `airplane_simbrief` rows.
  - `AirlineDb.RegistrationReusableAfterReturn`: two rows with one registration, the first returned → accepted; both
    not returned → constraint failure.
  - `Leasing.LeaseFromReadingWritesEverything`: test airline, a 737-800 Two-class reading (Max Cargo 18,000) → one
    airplane at KBFI awaiting delivery, cabins 16/144, holds 7,941/10,059, limits from the reading, an
    `airplane_simbrief` row, one lease line −$235,000.
  - `Leasing.InstantDeliveryPostsItsLines`: same with `instantDeliveryIcao = "KSAN"` → located at KSAN, not awaiting
    delivery, fuel 0, one `instant_delivery` row, its lines equal `instantDeliveryPrice(...)`'s costs, cash down by
    the month plus that total.
  - `Leasing.InstantDeliveryToFactoryIsFree` (Review Focus 4): `"KBFI"` for a 737 → at KBFI, delivered, no
    `instant_delivery` row.
  - `Leasing.RefusalsWriteNothing`: each of — no slot, seats ≠ layout, duplicate registration in fleet, duplicate
    Internal ID in fleet, unknown airport, runway too short, cash 1 cent short (Review Focus 5) — returns a non-empty
    `refusal`, and the airplane/ledger row counts are unchanged.
  - `Leasing.MonthsStillChargedAndUnpaidReturned`: today's month-charging tests, adapted to the new signature.
  - `AirlineConfig.IgnoresFleetsAndAirplanes`: a file with old `fleets:`/`airplanes:` keys parses; identity read.
- [ ] **Step 2: Run** `ctest --preset debug` → the new tests FAIL.
- [ ] **Step 3: Implement** migration 11, `leaseAirplane` / `leaseRefusal` (one shared check function), the slimmer
  `settleLeases`, the config and session changes, ledger text: delivery lines "N738SX instant delivery KBFI–KSAN ·
  Delivery fee" (like flight lines), refund "Lease refund N814SX". Rewrite `tests/test_airline.h` to lease its two
  airplanes with `leaseAirplane` from built readings (same registrations, airframes and KBFI location as today, so
  other tests keep their expectations). Remove the airplanes and fleets from `config/airline.example.yaml`.
- [ ] **Step 4: Run** the full suite → PASS.
- [ ] **Step 5: Commit** `Migration 11 and leasing from a SimBrief reading (D-242, D-244, D-245, D-246)`.

### Task 4: Return and re-read

**Files:**
- Modify: `src/leasing.h`, `src/leasing.cpp`, `tests/leasing_test.cpp`

**Interfaces:**
- Consumes: Task 2's `leaseRefundCents`, `candidateLayouts`, `splitHolds`; Task 3's schema.
- Produces:
  - `std::string returnAirplane(db::Database&, std::int64_t airplaneId, Date today);` "" on success, else why (a
    planned flight exists; already returned). One transaction: `returned_on`, reason `returned`, the refund line and
    its `lease_refund` row (no line when the refund is $0).
  - `struct RereadChange { std::string label, before, after; };`
  - `struct RereadPlan { std::vector<RereadChange> changes; std::string refusal; };`
  - `RereadPlan planReread(db::Database&, const Catalog&, std::int64_t airplaneId, const SimbriefReading&, const
    CatalogAirframe* layout);` what Save would change, or why it can't (another type, another registration, Internal
    ID used by another airplane in the fleet, seats ≠ layout, a planned flight exists). Hold changes show the new
    split.
  - `std::string applyReread(db::Database&, const Catalog&, std::int64_t airplaneId, const SimbriefReading&, const
    CatalogAirframe* layout, std::string_view readAt);` one transaction: limits, `airplane_simbrief` (insert or
    replace), holds, and when the layout changed its cabins and `airframe_name`.

- [ ] **Step 1: Write the failing tests:**
  - `Return.RefundsAndFreesTheSlot`: lease on 2026-11-22, return on 2026-11-27 → `returned_on`, reason `returned`, a
    +$120,000 (BBJ2 $150,000 rate) `lease_refund` line, fleet size down by one, and `settleLeases` on 2026-12-22
    charges nothing for it.
  - `Return.RefusedWithPlannedFlight`: a planned flight for it → non-empty reason, nothing written.
  - `Reread.ShowsOnlyChanges`: bag weight 55 → 80 and Max Cargo 18,000 → 20,000 → changes "Bag weight 55 lb → 80 lb",
    "Max cargo 18,000 lb → 20,000 lb", "Forward hold …", "Aft hold …"; nothing else.
  - `Reread.RefusesOtherTypeAndRegistration`: base type `B77W` → refusal; registration `N999XX` → refusal.
  - `Reread.LayoutChangeReplacesCabins`: Two-class → All-economy with Max Passengers 184 → cabins `{economy 184}`,
    `airframe_name` updated; flights already settled unchanged.
  - `Reread.FirstReadOfOldAirplaneInsertsSimbriefRow`: an airplane with no `airplane_simbrief` row gets one.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run the full suite** → PASS.
- [ ] **Step 5: Commit** `Return with a refund and re-reading a screenshot (D-245, D-246)`.

### Task 5: Reading the screenshot with Claude

**Files:**
- Create: `src/airframe_read.h`, `src/airframe_read.cpp` (core: request and answer, no network),
  `src/airframe_main.cpp` (`blocktime-airframe`), `tests/airframe_read_test.cpp`,
  `tests/data/airframe_read_ok.json`, `tests/data/airframe_read_partial.json`
- Modify: `CMakeLists.txt` (core source, the tool next to `blocktime-debrief`, the test), `src/review_client.h`
  (comment: also the airframe read)

**Interfaces:**
- Consumes: Task 2's `SimbriefReading`; `kReviewModel`, `kReviewUrl`, `kAnthropicVersion`, `kFallbackBeta` from
  `debrief_review.h`; `ReviewClient` / `ClaudeReviewer`.
- Produces:
  - `std::string buildAirframeReadRequest(std::string_view imageBytes, std::string_view mediaType);` the JSON body:
    `model`, `max_tokens` 2000, `output_config.format` a JSON schema with one property per field (strings for
    registration, fin_number, internal_id, base_type, weight_units; integers for the rest; each nullable, all
    required, no extras), a system prompt naming each SimBrief label as listed in `docs/fleet.md` and saying "null
    when the field isn't visible", and one user message with the base64 image block.
  - `struct AirframeReadAnswer { std::optional<SimbriefReading> reading; std::string error; };`
  - `AirframeReadAnswer parseAirframeReadResponse(int httpStatus, std::string_view body);` the debrief's error
    handling (HTTP error message, refusal, cut off, unreadable); a reading whose `weightUnits` isn't `LBS` →
    error "Set SimBrief's weight units to LBS"; weights rounded to whole pounds.
  - `blocktime-airframe <image>`: reads the key from `settings.json`, posts with `ClaudeReviewer`, prints the
    reading as `field: value` lines (or the error) and exits 0 / 1.

- [ ] **Step 1: Write the failing tests:**
  - `AirframeRead.RequestCarriesImageAndSchema`: the body parses as JSON; `model == "claude-opus-5-5"`; the user
    content has an `image` block with `media_type "image/png"` and the base64 of the bytes; the schema's `required`
    lists all 15 fields.
  - `AirframeRead.ParsesGoodAnswer`: `airframe_read_ok.json` → registration `N738SX`, internal ID
    `730097_1791702236118`, base type `B738`, max passengers 160, MLW 146300, max cargo 18000, min runway 7000.
  - `AirframeRead.NullFieldsStayUnset` (Review Focus 1): `airframe_read_partial.json` (mlw null) → `mlwLb` nullopt and
    `missingFields` = `{"Max Landing Weight"}`.
  - `AirframeRead.Errors`: HTTP 401 with an API error message → "Claude: …"; `stop_reason: refusal`; junk text;
    weight units `KGS` → the LBS message.
- [ ] **Step 2: Run** → FAIL. **Step 3: Implement.** **Step 4: Run the full suite** → PASS.
- [ ] **Step 5: Build and try the tool** on a screenshot from Dave if one is in the repo's scratch area; otherwise
  note it as unverified for Task 9.
- [ ] **Step 6: Commit** `Read a SimBrief airframe screenshot with Claude, and blocktime-airframe (D-242)`.

### Task 6: The Fleet screen

**Files:**
- Create: `src/fleet_model.h`, `src/fleet_model.cpp`, `qml/FleetCard.qml`, `qml/AirplaneGlyph.qml`
- Modify: `qml/FleetPage.qml` (rewritten), `qml/Main.qml`, `qml/Theme.qml`, `src/app_main.cpp`, `CMakeLists.txt`,
  `tests/qt_models_test.cpp`
- Delete: `src/fleet_table_model.h`, `src/fleet_table_model.cpp`

**Interfaces:**
- Consumes: `AirlineSession` (`airplanes()`, `notices()`, `hoursFlown()`, `cash()`, `airlineName()`,
  `airlineIcao()`), `slotsEarned`, `hoursForSlot`, `kMaxSlots`, `currentPlannedFlight`.
- Produces: `class FleetModel : public QAbstractListModel` (QML_ELEMENT, uncreatable):
  - roles per airplane card: `registration, title` ("737-800 Two-class"), `place` ("At KSAN · 12,400 lb"),
    `leg` ("" today; the planned flight's "XLY 12 · KLAX → RJTT" when planned), `glyph` ("ready" | "delivery" |
    "current" | "air"), `button` ("Continue Flight" | "Fly Delivery" | "Fly"), `buttonEnabled`, `group` ("" |
    "rotation" | "journey", always "" in 15b)
  - properties: `airlineName, airlineCode, callsign, slotsOpen, slotsMax, hoursFlown, nextSlotHours, nextSlotProgress
    (0..1 within the current 10 h), cashText, leasesText, emptySlots, lockedSlotHours (QVariantList of ints), notices`
  - `void reset(...)` the way `FleetTableModel::reset` is called today
  - signals used by QML: none; `Main.qml` handles `flyRequested(registration)` (→ New Flight as today),
    `continueRequested()` (→ Live), `manageRequested(registration)` (→ the aircraft page, Task 7),
    `addRequested()` (→ Add Aircraft, Task 8) from `FleetPage`.
- Theme: `glyphReady` (= `good`), `glyphDelivery` (= `warn`), `glyphCurrent` (= `accent`), `glyphAir` `#4B5563`,
  `rotation` `#2DD4BF`, `journey` `#A78BFA`, plus the card and tile sizes the page uses.

- [ ] **Step 1: Write the failing Qt tests** in `tests/qt_models_test.cpp`:
  - `FleetModel.CardsAndSlots`: the test airline at 23.4 h → `slotsOpen 4`, `nextSlotHours 30`,
    `nextSlotProgress ≈ 0.34`, two cards, `emptySlots 2`, `lockedSlotHours` starting `[30, 40, …]` with 16 entries.
  - `FleetModel.Buttons`: an airplane awaiting delivery → "Fly Delivery", glyph "delivery"; with a planned flight on
    the other airplane → that one "Continue Flight" / "current", this one disabled.
  - `FleetModel.TitleLeadsWithLayout`: `title` is the layout name, `registration` separate.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** the model, `FleetPage.qml` per `docs/mockups/fleet/2-fleet-cards.html` on the layout of
  `1-slot-layouts.html` B (left: the airline panel with notices; right: cards, dashed empty slots, the locked row with
  "+N more" after seven), `FleetCard.qml`, `AirplaneGlyph.qml` (the top-down silhouette from the mockups as a
  `Shape`, coloured by `glyph`), the wiring in `Main.qml` and `app_main.cpp` (replace every `fleet.reset(...)` call).
- [ ] **Step 4: Run** the full suite and `blocktime-app_qmllint` → PASS, nothing new. Run `BlockTime.exe --demo`
  (debug) and compare with the mockup: two cards, Fly opens New Flight, the slot numbers right.
- [ ] **Step 5: Commit** `Fleet is the slots screen (D-241)`.

### Task 7: Add Aircraft

**Files:**
- Create: `src/add_aircraft_model.h`, `src/add_aircraft_model.cpp`, `qml/AddAircraftPage.qml`,
  `qml/ScreenshotDrop.qml` (drag-and-drop, Ctrl+V and a file picker; shared with Task 8)
- Modify: `qml/Main.qml`, `src/app_main.cpp`, `CMakeLists.txt`, `tests/qt_models_test.cpp`

**Interfaces:**
- Consumes: Task 5's `buildAirframeReadRequest`, `parseAirframeReadResponse`; Task 3's `leaseAirplane`,
  `leaseRefusal`; Task 2's `candidateLayouts`, `preselectedLayout`, `splitHolds`, `instantDeliveryPrice`,
  `missingFields`; `ReviewClient`; `moneyLinesToQml`.
- Produces: `class AddAircraftModel : public QObject`:
  - `Q_INVOKABLE void readImage(const QByteArray& bytes, const QString& mediaType);` and
    `Q_INVOKABLE void readImageFile(const QUrl& file);` / `Q_INVOKABLE void readClipboard();`
  - properties: `state` ("empty" | "reading" | "read" | "failed"), `readError`, `fields` (QVariantList of
    `{key, label, value, missing}` in form order), `checks` (`{ok, text}`), `layouts` (`{name, detail, enabled}`),
    `layoutIndex` (writable), `holdsText`, `holds` (`{name, lb}`), `flightNumber` (writable), `delivery`
    ("factory" | "instant", writable), `deliveryIcao` (writable), `deliveryCheck`, `deliveryLines` (QVariantList via
    `moneyLinesToQml`), `deliveryTotalText`, `leaseLines`, `cashAfterText`, `refusal` ("" = Lease enabled),
    `factoryText` ("Fly it from Boeing Field (KBFI)")
  - `Q_INVOKABLE void setField(const QString& key, const QString& value);` (fixing a misread)
  - `Q_INVOKABLE bool lease();` → emits `leased(QString registration)`; `Q_INVOKABLE void retry();`
  - Without a key: `state "failed"`, `readError "No Claude API key in settings.json (README)"`, fields editable.

- [ ] **Step 1: Write the failing Qt tests** with a fake `ReviewClient` returning the Task 5 fixtures:
  - `AddAircraft.ReadFillsFormAndPreselects`: after `readImage`, `state "read"`, registration field `N738SX`, layout
    `737-800 Two-class` selected, `holds` 7,941 / 10,059, `refusal ""` with enough cash.
  - `AddAircraft.MissingFieldBlocksLease` (Review Focus 1): the partial fixture → MLW field `missing`, `refusal`
    names "Max Landing Weight"; `setField("mlwLb", "146300")` → `refusal ""`.
  - `AddAircraft.InstantDeliveryPrice`: delivery "instant", ICAO `KSAN` → `deliveryLines` first is "Delivery fee";
    `cashAfterText` = cash − month − total.
  - `AddAircraft.NetworkErrorAllowsRetry`: the fake answers status 0 with an error → `state "failed"`, `retry()`
    posts again.
  - `AddAircraft.LeaseEmitsAndResets`: `lease()` true, `leased("N738SX")` emitted, the fleet has the airplane.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** the model and `AddAircraftPage.qml` per `docs/mockups/fleet/4-add-aircraft.html`; a
  `ClaudeReviewer` instance with the settings key (none in a scratch run, as for reviews); on `leased`, `Main.qml`
  returns to Fleet and the fleet model resets.
- [ ] **Step 4: Run** the full suite and qmllint → PASS. In `--demo`, open an empty slot, type values by hand (no
  key in a scratch run) and lease; the new card appears; cash drops by the shown amount.
- [ ] **Step 5: Commit** `Add Aircraft from a SimBrief screenshot, with factory or instant delivery (D-242, D-245)`.

### Task 8: The aircraft screen

**Files:**
- Create: `src/aircraft_model.h`, `src/aircraft_model.cpp`, `qml/AircraftPage.qml`
- Modify: `qml/Main.qml`, `src/app_main.cpp`, `src/airline_queries.h`, `src/airline_queries.cpp` (one airplane's
  details: limits, `airplane_simbrief`, cabins, holds, flights, hours, next charge), `CMakeLists.txt`,
  `tests/qt_models_test.cpp`

**Interfaces:**
- Consumes: Task 4's `returnAirplane`, `planReread`, `applyReread`; Task 5's read functions; `ScreenshotDrop.qml`;
  `leaseRefundCents`.
- Produces: `class AircraftModel : public QObject`:
  - `Q_INVOKABLE void open(const QString& registration);`
  - properties: `registration, tailText, flightNumberText, title, statusText, leaseMonthText` ("6 of 30 days"),
    `leaseMonthProgress`, `layoutCard`, `weightsCard`, `simbriefCard`, `recordCard` (each a QVariantList of `{label,
    value}` plus a `source` string, "No screenshot yet" when there's no `airplane_simbrief` row), `holds`,
    `canReturn`, `returnText` (the confirmation's sentence), `canReread`
  - re-read: `readImage(...)` as in Task 7, then `rereadChanges` (`{label, before, after}`), `rereadRefusal`,
    `layouts`/`layoutIndex` when the seats changed, `Q_INVOKABLE bool saveReread();`
  - `Q_INVOKABLE bool returnPlane();` → emits `returned()`
- [ ] **Step 1: Write the failing Qt tests:**
  - `Aircraft.ShowsCardsAndRefund`: opened on 2026-11-27 for the BBJ2 leased 2026-11-22 → `leaseMonthText "6 of 30
    days"` (today is the period's sixth day), `returnText` contains "$120,000" and "24 days" (Task 2's refund).
  - `Aircraft.NoScreenshotYet`: an airplane without `airplane_simbrief` → `weightsCard` source "No screenshot yet".
  - `Aircraft.RereadShowsChangesThenSaves`: fake client answers with bag weight 80 → one change "Bag weight";
    `saveReread()` → limits updated.
  - `Aircraft.ReturnDisabledWithPlannedFlight`: `canReturn` false, `canReread` false.
- [ ] **Step 2: Run** → FAIL.
- [ ] **Step 3: Implement** the model and `AircraftPage.qml` per `docs/mockups/fleet/3-aircraft-screen.html` B
  (the plane on the left with New screenshot and Return, four cards on the right; the re-read changes and the return
  confirmation as in the mockup's lower half). On `returned`, `Main.qml` goes back to Fleet and resets fleet and
  ledger.
- [ ] **Step 4: Run** the full suite and qmllint → PASS. In `--demo`: Manage → the screen; Return → confirmation →
  the slot is empty and Money shows the refund.
- [ ] **Step 5: Commit** `The aircraft screen with New screenshot and Return (D-241, D-245)`.

### Task 9: Docs, the real migration, review

**Files:**
- Modify: `docs/design.md` (§3 airframes as shipped files and airplanes from SimBrief; §5 return with refund; §6 the
  slot rule and table, Add Aircraft, instant delivery, no fleets), `docs/database.md` (migration 11's tables, the
  entity diagram without `fleet`, re-leasing, the processes), `docs/dispatch.md` (step 1: Fleet → Fly),
  `README.md` (no airplanes in `airline.yaml`, `data/aircraft/`, `blocktime-airframe`, no more `catalog.yaml` copy),
  `docs/notes/` (a note on reading SimBrief screenshots if anything non-obvious turns up), `docs/progress.md`

- [ ] **Step 1:** Update the docs above so every spec agrees with the code (CLAUDE.md duty 3).
- [ ] **Step 2: Verify** (superpowers:verification-before-completion): `cmake --build --preset debug` and
  `--preset release` with no warnings; `ctest` passes in both; qmllint reports only today's info line.
- [ ] **Step 3: Migration 11 dry run:** copy `%LOCALAPPDATA%\BlockTime\blocktime.db` to a scratch folder, open it
  with the release build against the copy (as for migration 10): the backup `blocktime.before-schema-11.db` is made,
  `foreign_key_check` empty, `integrity_check` ok, both airplanes present with their charges and flights, Fleet shows
  two cards with "No screenshot yet" on Manage.
- [ ] **Step 4:** superpowers:requesting-code-review on the milestone's diff, then superpowers:receiving-code-review
  for the findings.
- [ ] **Step 5:** `docs/progress.md` entry with what's verified, and the steps for Dave printed in the reply: run the
  app, Manage each existing airplane and drop its SimBrief screenshot, add a plane with instant delivery, return one,
  check Money's lines; `blocktime-airframe` on a screenshot. No sim test (nothing here talks to the sim).
- [ ] **Step 6: Commit** `Milestone 15b: docs and verification`, then push `main` to `origin` (D-121).
