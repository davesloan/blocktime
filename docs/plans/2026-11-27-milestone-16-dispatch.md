# Milestone 16 — Dispatch on Your Flight Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (CLAUDE.md, D-033: this project doesn't use
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking. Commit to `main` after each task
> (D-038).

**Goal:** Live becomes Your Flight. A flight is set up there on a form, dispatched (loads rolled once), checked on the
dispatch report (the ring, the split-flap board, the working, load and plan), imported from SimBrief with a load
check, and accepted. Its money is locked at Accept and settles from that breakdown. Accept writes a marker in the log.

**Architecture:** Every rule and number lives in plain C++ in `blocktime-core`: the accepted breakdown and its
migration, fully loaded, the import check, the board, ring and working text, the stage of Your Flight, and settling
from the accepted lines. These come with GoogleTest tests. Two Qt models expose it: `YourFlightModel` (what Your
Flight shows, settling and the not-recording text; it replaces `PlannedFlightModel`) and `DispatchModel` (the form,
the report and its actions; it replaces `NewFlightModel`). Both sit behind the narrow `FlightSource` the dashboard
models read (review 3.1, 3.2). QML only draws.

**Tech Stack:** C++20, MSVC, CMake + Ninja (presets `x64-debug`, `x64-release`), Qt 6 QML (Basic style), SQLite
(sqlite3 C API), nlohmann::json, GoogleTest.

**Spec:** `docs/dispatch.md` (the flow, the dispatch report, "What the code needs"). Decisions: D-214, D-215, D-222,
D-223, D-233, D-234, D-236 and D-250. The look is the canvas https://claude.ai/artifact/N81DAojKWXThKyjb9itke7, board
`Final` (read `project/Final.dc.html` with the Artifact tool's read action). The summary and debrief restyle (D-235)
is milestone 17. The per-flight log files (D-237) are milestone 16b.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`. `blocktime-app_qmllint` reports nothing new (today: ReplayBar's one info
  line).
- QML only displays: the workings, deltas, checks, board text and ring geometry come from C++. Every colour and size
  goes in `qml/Theme.qml`. Every QML file starts with the GPL header and `import BlockTime`, uses
  `pragma ComponentBehavior: Bound` and `required` properties in delegates.
- **No continuous animation.** The board's flip runs once at import and only on changed rows at a re-import, like the
  gauge needles. The ring's hover lift is a short `Behavior` transition.
- Database (D-078): `STRICT`, `NOT NULL` by default, foreign keys with an explicit `ON DELETE`, an index on every
  foreign key, money in integer cents with every line a multiple of 500 (D-080), changes to several tables in one
  transaction. Ledger lines and settled flights are never updated.
- Money (D-222, D-223): what settles is what was shown. Only pilots, flight attendants and maintenance (on the real
  block time) and fuel (the fuel added) are worked out again at block in, with the rates locked at Accept.
- The import check: passengers exactly (`weights.pax_count`), cargo within `max(100 lb, 1% of the flight's cargo)`
  (`weights.freight_added`, freight only: SimBrief's `weights.cargo` includes bags), destination, airplane and flight
  number (`general.flight_number`).
- Extra fuel goes in the dispatch URL as `addedfuel=<pounds / 1000, up to 3 decimals>&addedfuel_units=wgt`, only when
  it's above 0. The unit is inferred from `cargo`'s and isn't confirmed yet: Dave checks it on his first dispatch.
- Board rows are 20 tiles: the label in capitals on the left, the value on the right, at least one space between.
- Ring colour families: fuel red; landing fee, gate, handling, GA services and cargo handling orange; pilots, flight
  attendants and maintenance purple; catering and ATC blue; profit green.
- Debug-build checks use `--demo` and `--grab` (a scratch copy, D-226). Never touch Dave's airline, except the
  migration-12 dry run on a copy in Task 11.
- Version 0.18.0.

## Review Focus

1. **A re-import after Accept that the check refuses** (Dave regenerated with the wrong passengers): the plan, the
   accepted breakdown and its rates stay exactly as they were, and the message says what to regenerate. Test in Task 8
   (`DispatchModel` with an accepted flight, then a mismatching OFP).
2. **The app restarted between Accept and block out:** Your Flight opens on Pre-Flight. View Dispatch Report shows the
   stored breakdown, not a fresh one priced from today's `rates.yaml`. The recorder's target says accepted, so block
   out writes no second marker. Test in Task 8 (a new `DispatchModel` on the same database after a `rates.yaml` edit).
3. **Fuel on board above the plan's ramp fuel** (nothing to add): the fuel line is $0 with "0 lb added", the ring has
   no fuel slice and no gap, and settlement charges the fuel really added. Test in Task 4 (`ring` with a $0 line) and
   Task 6.
4. **A losing flight** (a delivery, or costs above the fares): the ring's whole circle is the costs, with no green arc;
   the centre reads "Loss" in red; a delivery's board shows no fares row. Test in Task 4.
5. **Block out with a flight dispatched but no plan imported:** nothing is recorded, and the banner says to import
   the plan. Block out with a plan imported but not accepted: accepted then (D-250), with one marker before
   `block_out`. Tests in Task 5.

---

### Task 1: Migration 12: the accepted breakdown and extra fuel

**Files:**
- Create: `src/accepted_breakdown.h`, `src/accepted_breakdown.cpp`, `tests/accepted_breakdown_test.cpp`
- Modify: `src/airline_db.h` (`kAirlineSchemaVersion = 12`), `src/airline_db.cpp` (migration 12, additive: no
  rebuild), `src/planned_flight.h/.cpp`, `tests/airline_db_test.cpp` (query plans), `CMakeLists.txt`,
  `docs/database.md`

Schema (migration 12):

```sql
ALTER TABLE planned_flight ADD COLUMN additional_fuel_lb INTEGER NOT NULL DEFAULT 0 CHECK (additional_fuel_lb >= 0);
ALTER TABLE flight ADD COLUMN additional_fuel_lb INTEGER NOT NULL DEFAULT 0 CHECK (additional_fuel_lb >= 0);
CREATE TABLE accepted_breakdown (
  accepted_breakdown_id INTEGER PRIMARY KEY,
  accepted_at TEXT NOT NULL CHECK (accepted_at GLOB <BT_INSTANT>),
  at_block_out INTEGER NOT NULL CHECK (at_block_out IN (0, 1))   -- D-250: accepted by block out
) STRICT;
CREATE TABLE accepted_line (
  accepted_breakdown_id INTEGER NOT NULL REFERENCES accepted_breakdown ON DELETE CASCADE,
  position INTEGER NOT NULL CHECK (position >= 0),                 -- income first, then costs, in preview order
  category TEXT NOT NULL REFERENCES ledger_category (code) ON DELETE RESTRICT,
  label TEXT NOT NULL CHECK (label <> ''),
  cents INTEGER NOT NULL CHECK (cents % 500 = 0),
  rate_micro_dollars INTEGER NOT NULL,                           -- the rate in millionths of a dollar (D-251)
  rate_basis INTEGER NOT NULL CHECK (rate_basis >= 1),
  rate_unit TEXT NOT NULL,
  PRIMARY KEY (accepted_breakdown_id, position)
) STRICT;
CREATE TABLE accepted_line_quantity (
  accepted_breakdown_id INTEGER NOT NULL,
  position INTEGER NOT NULL,
  quantity_position INTEGER NOT NULL CHECK (quantity_position >= 0),
  value REAL NOT NULL CHECK (value >= 0),                          -- hours, passengers, lb, nm: not money
  unit TEXT NOT NULL CHECK (unit <> ''),
  PRIMARY KEY (accepted_breakdown_id, position, quantity_position),
  FOREIGN KEY (accepted_breakdown_id, position) REFERENCES accepted_line ON DELETE CASCADE
) STRICT;
CREATE TABLE planned_flight_acceptance (
  planned_flight_id INTEGER PRIMARY KEY REFERENCES planned_flight ON DELETE CASCADE,
  accepted_breakdown_id INTEGER NOT NULL UNIQUE REFERENCES accepted_breakdown ON DELETE RESTRICT
) STRICT;
CREATE TABLE flight_acceptance (
  flight_id INTEGER PRIMARY KEY REFERENCES flight ON DELETE RESTRICT,
  accepted_breakdown_id INTEGER NOT NULL UNIQUE REFERENCES accepted_breakdown ON DELETE RESTRICT
) STRICT;
```

Index `accepted_line_category` on `accepted_line (category)`. The `UNIQUE` columns already have indexes. Income vs
cost follows the sign of `cents`. A flight settled before milestone 16 has no `flight_acceptance` row (dispatch.md:
no "moved since" tags).

**Interfaces:**
- Consumes: `Preview`, `MoneyLine`, `Quantity` (`pricing.h`); `db::Database`, `db::Statement`.
- Produces (namespace `blocktime`):
  - `struct AcceptedBreakdown { std::string acceptedAt; bool atBlockOut = false; Preview money; };`
  - `bool acceptPlannedFlight(db::Database&, std::int64_t plannedFlightId, const Preview& money, std::string_view acceptedAt, bool atBlockOut, std::string* error);`
    in one transaction: deletes the flight's old breakdown if any, then inserts the new one and the link. A re-import
    after Accept calls it again (D-233).
  - `std::optional<AcceptedBreakdown> plannedAcceptance(db::Database&, std::int64_t plannedFlightId);`
  - `std::optional<AcceptedBreakdown> flightAcceptance(db::Database&, std::int64_t flightId);` (for milestone 17)
  - `bool moveAcceptanceToFlight(db::Database&, std::int64_t plannedFlightId, std::int64_t flightId, std::string* error);`
    runs inside the caller's transaction (settlement, Task 6)
  - `NewPlannedFlight::additionalFuelLb`, `PlannedFlightView::additionalFuelLb` (int, 0 = none) and
    `PlannedFlightView::acceptedAt` (`std::optional<std::string>`).
  - `cancelPlannedFlight` also deletes the flight's `accepted_breakdown` (the link cascades, the breakdown doesn't).

- [ ] **Step 1: Write the failing tests** (`tests/accepted_breakdown_test.cpp`, `tests/airline_db_test.cpp`):
  - `AcceptedBreakdown.RoundTripsLinesAndWorkings`: accept a preview with an economy-fares line (`140 passengers ×
    $157.50`), a fuel line (`5,900 lb × $41 / 100 lb`) and a pilots line (`2.96 h × 2 pilots × $250`). Read it back:
    every field is equal, including `rateDollars` to 1e-6 and the quantities in order.
  - `AcceptedBreakdown.ReacceptReplaces`: accepting twice leaves one breakdown, one link and only the second lines
    (`SELECT count(*) FROM accepted_breakdown` = 1).
  - `AcceptedBreakdown.CancelLeavesNothing`: accept, then `cancelPlannedFlight`. All four tables are empty and
    `foreign_key_check` is empty.
  - `AcceptedBreakdown.MovesToTheFlight`: after `moveAcceptanceToFlight`, `flightAcceptance(flightId)` equals what was
    accepted, and deleting the planned flight leaves it in place.
  - `AirlineDb.EmptyDatabaseMigratesTo12` (the existing migration test, extended) and an `EXPLAIN QUERY PLAN` check:
    `plannedAcceptance`'s query uses the `planned_flight_acceptance` primary key and `accepted_line`'s.
  - `PlannedFlight.ExtraFuelStored`: `createPlannedFlight` with `additionalFuelLb = 1000` reads back 1000.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "AcceptedBreakdown|AirlineDb|PlannedFlight"`: FAIL.
- [ ] **Step 3: Implement** migration 12 and the functions above. `rate_micro_dollars = llround(rateDollars * 1e6)`.
- [ ] **Step 4: Run** the full suite: PASS. Update `docs/database.md` (the tables, the diagram, "Accept" and
  "Re-import after Accept" under the processes).
- [ ] **Step 5: Commit** `Migration 12: the accepted breakdown and extra fuel (D-222)`.

### Task 2: Fully loaded, a draft from a stored flight, and changing a flight

**Files:**
- Modify: `src/loads.h/.cpp`, `src/flight_draft.h/.cpp`, `src/planned_flight.h/.cpp`, `tests/loads_test.cpp`,
  `tests/flight_draft_test.cpp`, `tests/planned_flight_test.cpp`

**Interfaces:**
- Consumes: `rollLoads`, `bagsLb`, `splitHolds`, `AirplaneLimits::maxPayloadLb` (`loads.h`, `catalog.h`).
- Produces:
  - `LoadRoll fullLoads(const std::vector<Cabin>&, const std::vector<Hold>&, const AirplaneLimits&, FlightKind);`
    (D-215; design.md §9, step 5). A passenger flight fills every seat, then adds cargo up to the hold space left after
    bags or max payload less passengers and bags, whichever is less. A freighter carries cargo up to the holds or max
    payload. Other kinds are never fully loaded.
  - `FlightDraft::fullyLoadedAvailable()`: true for `Passenger` and `Cargo` only. `fullyLoaded()` and
    `setFullyLoaded(bool)` work like `setKind`: a kind's random roll is kept for switching back.
  - `FlightDraft::additionalFuelLb()` and `setAdditionalFuelLb(int)`, carried into `toNew()`.
  - `static FlightDraft FlightDraft::fromPlanned(FlightAirplane, Rates, const PlannedFlightView&);` takes the stored
    kind, loads, destination, flight number and extra fuel, and doesn't roll (review 4.5, D-089).
  - `bool updatePlannedFlight(db::Database&, std::int64_t id, const NewPlannedFlight&, std::string* error);` in one
    transaction: rewrites the row and its cabins, and deletes its `simbrief_plan` and its acceptance (any change
    makes the imported plan stale: Task 8's "Change flight" offers it only before Accept).

- [ ] **Step 1: Write the failing tests:**
  - `Loads.FullyLoadedPassengerFillsSeatsThenCargo`: the test 737-800 (160 seats, holds 7,500 / 9,500, max payload
    from its limits). Every cabin is at its seat count, and `cargoLb` is the smaller of the hold space after bags and
    the payload left.
  - `Loads.FullyLoadedTrimsCargoNeverBags`: limits where seats + bags + full holds exceed max payload. The cargo is
    trimmed to exactly max payload, the passengers stay full, and cargo is never negative.
  - `Loads.FullyLoadedFreighter`: the BCF carries `min(sum of holds, maxPayloadLb)`.
  - `FlightDraft.FullyLoadedOnlyForPassengerAndCargo`: available for the 737-800 airliner, not for the BBJ2's VIP or
    charter, nor for delivery.
  - `FlightDraft.FromPlannedDoesNotRoll`: a `Roll` that fails the test if called. `fromPlanned` keeps the view's
    cabins, cargo, kind, flight number and extra fuel.
  - `PlannedFlight.UpdateDropsPlanAndAcceptance`: plan stored and accepted, then `updatePlannedFlight` with a new
    destination. The cabins are rewritten, no `simbrief_plan` row, `plannedAcceptance` nullopt, `foreign_key_check`
    empty.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "Loads|FlightDraft|PlannedFlight"`: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite: PASS.
- [ ] **Step 5: Commit** `Fully loaded, drafts from a stored flight, changing a flight (D-215, review 4.5)`.

### Task 3: SimBrief: extra fuel in the URL, and the import check

**Files:**
- Modify: `src/simbrief.h/.cpp`, `tests/simbrief_test.cpp`, `tests/data/ofp-ksan-ksjc-brief.json` (add the fields
  below from Dave's stored KSAN–KSJC OFP), `docs/notes/simbrief-api.md`

**Interfaces:**
- Produces:
  - `DispatchRequest::additionalFuelLb` (int). `dispatchUrl` adds `&addedfuel=1.5&addedfuel_units=wgt` for 1,500 lb,
    and nothing at 0.
  - `SimbriefPlan` gains `int passengers` (`weights.pax_count`), `int freightLb` (`weights.freight_added`),
    `std::string flightNumber` (`general.flight_number`) and `int extraFuelLb`: the `fuel_extra.bucket` entry with
    `label == "EXTRA"`, 0 when missing.
  - `struct ExpectedLoad { int passengers = 0; int cargoLb = 0; std::string flightNumber; };`
  - `std::optional<std::string> loadMismatch(const SimbriefPlan&, const ExpectedLoad&);` returns the refusal, or
    nullopt. The texts:
    - loads: `"SimBrief has 40 passengers and 2,100 lb cargo; this flight has 44 and 2,300 lb. Open SimBrief to generate it again with this flight's loads."`
    - flight number: `"SimBrief has flight 815; this flight is 814. Open SimBrief to generate it again."`
    `planMismatch` (airports, airframe, time) still runs first.

- [ ] **Step 1: Write the failing tests:**
  - `Simbrief.DispatchUrlSendsExtraFuelInThousands`: 1,500 gives `addedfuel=1.5&addedfuel_units=wgt`, 1,234 gives
    `addedfuel=1.234`, 0 has no `addedfuel`.
  - `Simbrief.ParsesLoadsAndExtraFuel`: the fixture gives passengers 44, freight 0, flight number `"8"` and extra
    fuel 0.
  - `Simbrief.LoadCheck`: 44 / 2,300 against 44 / 2,300 passes; 2,330 passes (1% is 23, 100 wins); 2,401 refuses;
    cargo 20,000 against 20,190 passes (1% = 200); 43 passengers refuses with exactly the text above (numbers
    formatted with `formatNumber`); flight `"815"` against `"814"` refuses.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R Simbrief`: FAIL.
- [ ] **Step 3: Implement.** In `docs/notes/simbrief-api.md`, add `api_params` (the dispatch form as submitted,
  including `route` filled in by SimBrief when none is sent), `fuel_extra.bucket`, `weights.freight_added` vs
  `weights.cargo`, `general.flight_number`, and `addedfuel`/`addedfuel_units` (the unit not confirmed yet).
- [ ] **Step 4: Run** the full suite: PASS.
- [ ] **Step 5: Commit** `SimBrief: extra fuel in the dispatch URL and the import check (review 4.7)`.

### Task 4: The report's numbers: board, ring, working, changes, settling the accepted lines

**Files:**
- Create: `src/dispatch_report.h`, `src/dispatch_report.cpp`, `tests/dispatch_report_test.cpp`
- Modify: `src/pricing.h/.cpp` (`settleAccepted`, hours to two decimals in `workingText`), `tests/pricing_test.cpp`,
  `CMakeLists.txt`

**Interfaces:**
- Consumes: `Preview`, `MoneyLine`, `workingText`, `pilotsFor`, `formatMoneyFull`, `formatNumber`.
- Produces (namespace `blocktime`):
  - `Preview settleAccepted(const Preview& accepted, double actualBlockHours, int fuelAddedLb);` (D-222). It copies
    the accepted lines and replaces the quantities of only these lines, each recomputed with its own rate and rounded
    to $5:
    - `pilots`: the hours and `pilotsFor(actualBlockHours)`;
    - `flight_attendants` and `maintenance`: the hours;
    - `fuel`: the pounds. The fuel line is added if it was missing (a flight accepted before the fuel was known never
      happens, since Accept needs a plan, but don't drop the fuel charge).
    `totalCents` is recomputed.
  - `workingText` shows an `"h"` quantity with two decimals when the hundredths aren't 0 (`"2.96 h"`), else as today
    (D-238's open point).
  - `struct BoardRow { std::string id; std::string label; std::string value; enum class Tone { Income, Cost, Profit, Loss } tone; };`
  - `std::vector<BoardRow> boardRows(const Preview&);`: the income lines, the cost lines, then `{"profit", "Profit"}`.
    Values look like `+$15,630`, `-$2,420`, `$8,760` (a loss `-$1,240` with `Tone::Loss`). A preview with no income
    line (a delivery) has no fares row.
  - `std::string boardLabel(std::string_view category);`: short labels for the 20 tiles. `flight_attendants` →
    `Cabin crew`, `cargo_handling` → `Cargo hdlg`, `atc` → `ATC / nav`, `landing_fee` → `Landing`, `gate` → `Gate`,
    `handling` → `Handling`, `ga_services` → `GA services`; the rest use the ledger label.
  - `std::string boardText(const BoardRow&, int tiles = 20);`: exactly `tiles` characters. The label is in capitals,
    cut so that one space is left before the value, which is right-aligned.
  - `struct RingSlice { std::string id, family; Cents cents; double start, sweep; };` where `start` and `sweep` are
    fractions of the circle and `family` is one of `fuel`, `airport`, `crew`, `service`, `profit`.
  - `struct Ring { bool loss = false; std::vector<RingSlice> slices; };` `Ring ring(const Preview&);` With a profit,
    the circle is the income: each cost is a slice in line order, then a `profit` slice for what's left. With a loss,
    the circle is the costs and there's no profit slice. $0 lines get no slice.
  - `struct WorkingFacts { int rampFuelLb = 0, fuelOnRecordLb = 0, extraFuelLb = 0; bool locked = false; };`
  - `std::vector<std::string> workingLines(const MoneyLine&, const WorkingFacts&);`. The fuel line gives
    `{"9,200 lb block fuel", "− 3,300 lb on board", "= 5,900 lb added × $41 / 100 lb", "Price locked when you dispatched"}`;
    every other line gives `{workingText(line)}`. `pilots`, `flight_attendants` and `maintenance` add
    `"Charged on the actual block time"` until `locked`.
  - `struct ChangeFacts { int blockS = 0, fuelAddedLb = 0, extraFuelLb = 0; };`
    `struct LineChange { std::string id; Cents was = 0, now = 0; std::string why; };`
    `std::vector<LineChange> changesSince(const Preview& before, const ChangeFacts& b, const Preview& after, const ChangeFacts& a);`
    gives one entry per board id whose amount differs, the profit included. The texts:
    - fuel: `"Was -$2,180 before the re-import: 1,000 lb more fuel to add (extra fuel +1,000 lb)"`; the bracket only
      when the extra fuel changed;
    - crew and maintenance: `"Was -$840 before the re-import: planned block 1:44 (was 1:40)"`;
    - profit: `"-$240 since the last import"`.
    The baseline is in memory only (D-251).

- [ ] **Step 1: Write the failing tests** (`tests/dispatch_report_test.cpp`, `tests/pricing_test.cpp`):
  - `SettleAccepted.OnlyCrewMaintenanceAndFuelMove`: accept a passenger preview at a planned block of 1:44, then
    settle at 2.1 h and 6,400 lb. Fares, landing, gate, handling, ATC, catering and cargo handling are identical; pilots
    = 2.1 × 2 × the line's rate rounded to $5; fuel = 6,400 × the line's rate / 100; total = the sum.
  - `SettleAccepted.ThirdPilotOverEightHours`: accepted at 7.9 h, settled at 8.2 h, so the pilots quantity is 3.
  - `Working.HoursTwoDecimals`: `2.96 h × 2 pilots × $250`; `2.5 h` stays `2.5 h`.
  - `Board.RowsAndText`: VIP preview. The first row is `VIP fares` (Income), the last `Profit`;
    `boardText({.., "Fuel", "-$2,420", Cost})` is `"FUEL         -$2,420"` (20 characters); a long label gets cut to
    leave one space.
  - `Board.DeliveryHasNoFaresAndALoss` (Review Focus 4): a delivery preview has no Income row; profit has
    `Tone::Loss`.
  - `Ring.ProfitCircleIsTheFares`: the slices' sweeps add up to 1 (to 1e-9). Fuel's `family == "fuel"`. The last
    slice is `profit` with sweep = profit / fares.
  - `Ring.LossCircleIsTheCosts` (Review Focus 4): `loss == true`, no `profit` slice, sweeps add up to 1.
  - `Ring.ZeroLineHasNoSlice` (Review Focus 3): a $0 fuel line gives no fuel slice, and the starts stay contiguous.
  - `Working.FuelDerivation`: ramp 9,200 and on record 3,300 give the four lines above (the pound figures with
    `formatNumber`). Ramp 3,000 and on record 3,300 give `"= 0 lb added × $41 / 100 lb"`.
  - `Changes.FuelAndProfitAfterReimport`: two previews with fuel 4,900 and 5,900 lb and extra fuel 0 and 1,000 give
    entries for `fuel` and `profit` only, with the texts above.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "SettleAccepted|Working|Board|Ring|Changes"`: FAIL.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** the full suite: PASS.
- [ ] **Step 5: Commit** `The dispatch report's numbers: board, ring, working, changes; settling accepted lines (D-222, D-234)`.

### Task 5: Accept in the log, and block out accepting (D-236, D-250)

**Files:**
- Modify: `src/flight_match.h/.cpp` (`FlightTarget::accepted`), `src/planned_flight.cpp` (`flightTarget`),
  `src/flight_tracker.h/.cpp` (`onBlockOut`), `src/recorder.cpp` (the app event), `src/feed_text.cpp` (a feed line),
  `tests/flight_tracker_test.cpp`, `tests/planned_flight_test.cpp`, `tests/flight_settlement_test.cpp`,
  `docs/recorder.md` (the event)

**Interfaces:**
- Produces:
  - `FlightTarget::accepted` (bool).
  - `flightTarget(db, view)` returns nullopt while no plan is imported. A dispatched flight without a plan isn't
    recorded (D-250).
  - The log line `flight_accepted` with data `{planned_flight_id, planned_at, registration, origin, destination,
    at_block_out}`, then a `cockpit` snapshot (`CockpitMonitor::snapshot`) and a `telemetry` line of the latest sample.
    None of them carries a `flight_id`: the flight doesn't exist until block out.
  - The app sends `AppEventQueue::push("flight_accepted", {planned_flight_id, planned_at, registration, origin,
    destination, at_block_out: false})` at Accept. The recorder writes it with `logHistory` (held on a failed write,
    like the rest), then the two snapshots.
  - `FlightTracker::onBlockOut` puts `flight_accepted` with `at_block_out: true` first in its returned events when
    `target && !target->accepted`.
  - Readers ignore the type: `loadFlightHistory`, `findOpenFlight`, `findCurrentFlown`, `loadFlightEvents` and the
    debrief measure don't change (milestone 16b moves them to the marker).

- [ ] **Step 1: Write the failing tests:**
  - `FlightTracker.BlockOutAcceptsAnUnacceptedTarget` (Review Focus 5): a target with `accepted = false` gives
    events `flight_accepted` (with `at_block_out == true` and the planned id) then `block_out`. With
    `accepted = true`, no `flight_accepted`.
  - `PlannedFlight.NoTargetWithoutAPlan` (Review Focus 5): dispatched, no plan, so nullopt. Plan stored, so a target
    with `accepted == false`. Accepted, so `accepted == true`.
  - `FlightSettlement.IgnoresAcceptedLines`: an existing settlement fixture with `flight_accepted`, `cockpit` and
    `telemetry` lines inserted before `block_out` settles to the same result as without them.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "FlightTracker|PlannedFlight|FlightSettlement"`: FAIL.
- [ ] **Step 3: Implement.** The `--demo` feed shows `flight_accepted` as "Flight accepted" (`feed_text.cpp`).
  `docs/recorder.md` gets the event and its fields. `PlannedFlightModel::notRecording` (still there until Task 7)
  reads `NoPlannedFlight` with a planned flight as "Not recording: import the SimBrief plan on Your Flight first."
- [ ] **Step 4: Run** the full suite: PASS.
- [ ] **Step 5: Commit** `Accept writes a flight_accepted marker; block out accepts an unaccepted flight (D-236, D-250)`.

### Task 6: Settling from the accepted breakdown, and the settlement coordinator

**Files:**
- Create: `src/settlement_coordinator.h`, `src/settlement_coordinator.cpp`, `tests/settlement_coordinator_test.cpp`
- Modify: `src/flight_settlement.h/.cpp`, `tests/flight_settlement_test.cpp`, `CMakeLists.txt`

**Interfaces:**
- Consumes: `plannedAcceptance`, `moveAcceptanceToFlight` (Task 1); `settleAccepted` (Task 4).
- Produces:
  - `settleFlight` (same signature). With a planned acceptance, the money is `settleAccepted(accepted.money,
    actualBlockHours, fuelAddedLb)`, the acceptance moves to the flight and `additional_fuel_lb` is copied, all in the
    settlement transaction. Without one (a flight recorded by 0.17), it prices as today from `rates`.
  - `class SettlementCoordinator` (plain C++, review 3.2): it takes over `PlannedFlightModel::checkFlown` and
    `retryPendingSettle`.
    ```cpp
    SettlementCoordinator(db::Database&, std::filesystem::path logDir, std::filesystem::path ratesFile);
    struct Outcome { enum class Kind { Nothing, Waiting, Settled } kind = Kind::Nothing; std::string waitingWhy;
                     std::optional<SettledFlight> settled; std::vector<LogLine> lines; };
    Outcome check(bool blockInSeen);  // what checkFlown did, the waiting texts unchanged
    bool pending() const;
    ```
    The deboarding recording (`recordDeboardings`) stays with the caller.

- [ ] **Step 1: Write the failing tests:**
  - `FlightSettlement.AcceptedLinesSettleAsAccepted`: plan, accept, then edit the test rates (pilot rate doubled),
    then settle. The fares and fixed lines equal the accepted ones; the pilots line uses the accepted rate;
    `flightAcceptance(flightId)` exists and `planned_flight_acceptance` is empty; `flight.additional_fuel_lb` was
    copied.
  - `FlightSettlement.WithoutAcceptancePricesAsBefore`: the existing settlement tests pass unchanged.
  - `FlightSettlement.ZeroFuelToAdd` (Review Focus 3): fuel on record above ramp fuel at Accept, and a settled fuel
    added of 800 lb, so the settled fuel line is 800 lb at the accepted rate.
  - `SettlementCoordinator.WaitsForBlockIn` / `.WaitsForRates` / `.Settles`: the three outcomes, with the same
    waiting texts `checkFlown` used.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "FlightSettlement|SettlementCoordinator"`: FAIL.
- [ ] **Step 3: Implement.** Make `PlannedFlightModel::checkFlown` a thin call to the coordinator, so the app keeps
  working until Task 7 replaces the model.
- [ ] **Step 4: Run** the full suite: PASS.
- [ ] **Step 5: Commit** `Settle from the accepted breakdown with the locked rates (D-222); a plain-C++ settlement coordinator (review 3.2)`.

### Task 7: One stage for Your Flight, and the FlightSource seam (review 3.1, 3.2)

**Files:**
- Create: `src/your_flight_stage.h`, `src/your_flight_stage.cpp`, `tests/your_flight_stage_test.cpp`,
  `src/flight_source.h`, `src/your_flight_model.h`, `src/your_flight_model.cpp`
- Modify: `src/dashboard_header.*`, `src/preflight_model.*`, `src/inflight_model.*`, `src/replay_planned_flight.*`
  (becomes a `FlightSource`), `src/app_main.cpp` (drop the `preFlightOrigin` lambda's string compare),
  `tests/qt_models_test.cpp`, `CMakeLists.txt`
- Delete: `src/planned_flight_model.*`, `src/planned_flight_display.*` (their reference text goes to Task 8's report)

**Interfaces:**
- Produces:
  - In core:
    ```cpp
    enum class YourFlightStage { Empty, Form, Dispatched, Imported, Accepted, Flying, Arrived, Summary };
    struct StageInputs { bool formOpen = false, planned = false, planImported = false, accepted = false,
                         flightOpen = false, settlePending = false, summaryShowing = false, finishable = false; };
    YourFlightStage yourFlightStage(const StageInputs&);
    ```
    The first that applies wins: `flightOpen` → Flying; `settlePending` or (`summaryShowing && finishable`) → Arrived;
    `summaryShowing` → Summary; `formOpen && !accepted` → Form; `accepted` → Accepted; `planImported` → Imported;
    `planned` → Dispatched; `formOpen` → Form; else Empty.
  - `class FlightSource : public QObject` (abstract) with `virtual const std::optional<blocktime::PlannedFlightView>& view() const = 0;`,
    `virtual blocktime::YourFlightStage stage() const = 0;` and `signal changed()`. `DashboardHeader`, `PreFlightModel`
    and `InFlightModel` take a `FlightSource&`, and their `visible` becomes a one-line read of `stage()`: the header
    from Accepted on; Pre-Flight in Accepted; in-flight in Flying and Arrived.
  - `class YourFlightModel : public FlightSource` (`QML_ELEMENT`), with `Q_PROPERTY(QString stage)` (`"empty"`,
    `"form"`, `"dispatched"`, `"imported"`, `"accepted"`, `"flying"`, `"arrived"`, `"summary"`), `notRecording`,
    `notice` and `summaryFlightId`. Invokables: `reload()`, `doneForNow()`, `turnaround()` and
    `setFormOpen(bool)`. Signals: `flightChanged()`, `settled(QString)`, `turnaroundRequested(QString)`. It owns a
    `SettlementCoordinator`, sets the recorder's target on `reload()`, and keeps the deboarding hook.
  - `ReplayPlannedFlight : FlightSource` drops its no-op invokables and the `readOnly` flag.

- [ ] **Step 1: Write the failing tests:**
  - `YourFlightStage.Table`: one `EXPECT_EQ` per row of the precedence above, plus "accepted and the form open" →
    Accepted (Change flight isn't offered after Accept) and "settle pending while flight open" → Flying.
  - `QtModels.HeaderVisibleFromAccepted`: a fake `FlightSource` steps through every stage, and the three models'
    `visible` follow the rule above.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "YourFlightStage|QtModels"`: FAIL.
- [ ] **Step 3: Implement.** Until Task 10, `LivePage.qml` keeps its panels, bound to the new names: `showSummary`
  becomes `stage === "summary"`, and `PlannedFlightPanel` is shown for `"dispatched"`/`"imported"`.
- [ ] **Step 4: Run** the full suite, build, and `--demo`: the dashboard and replay behave as before.
- [ ] **Step 5: Commit** `One stage for Your Flight and a FlightSource for the panels (review 3.1, 3.2)`.

### Task 8: DispatchModel: the form, the report and their actions

**Files:**
- Create: `src/dispatch_model.h`, `src/dispatch_model.cpp`
- Modify: `src/app_main.cpp` (build it, connect Fleet's Fly, block events and turnaround), `tests/qt_models_test.cpp`,
  `CMakeLists.txt`
- Delete: `src/new_flight_model.*` (its logic moves here)

**Interfaces:**
- Consumes: `FlightDraft` (Task 2), `loadMismatch` (Task 3), `boardRows`, `boardText`, `ring`, `workingLines` and
  `changesSince` (Task 4), `acceptPlannedFlight` (Task 1), `YourFlightModel` (Task 7), `Planner`, `AppEventQueue`,
  `RecorderBridge`.
- Produces `class DispatchModel : public QObject` (`QML_ELEMENT`, uncreatable), one `changed()` signal:
  - Form properties: `registration`, `airplaneText`, `origin`, `kinds`, `kindLabels`, `kind` (rw),
    `fullyLoadedAvailable`, `fullyLoaded` (rw), `bellyCargoAvailable`, `bellyCargo` (rw), `destination` (rw),
    `destinationMessage`, `flightNumber` (rw), `additionalFuel` (rw, text, digits only, "" = none), `loadLines`,
    `canDispatch`.
  - Report properties: `reportTitle` (`"XLY 8 · KSAN → KSJC"`), `extraFuelBadge` (`"+1,000 lb extra fuel"` from the
    OFP's `extraFuelLb`, else from the form before import; `""` at 0), `warnings` (QStringList, the amber badges:
    `"Landing weight near MLW"` from `nearMlw`, and `"More fuel on board than the plan's block fuel"` when the plan's
    ramp fuel is below the fuel on record; empty otherwise), `imported`, `accepted`, `locked` (after block out), `boardRows` (QVariantList of
    `{id, text, tone, changed}`, where `text` is `boardText`'s 20 characters, blank value tiles before import),
    `ring` (`{loss, slices:[{id, family, label, amountText, start, sweep}]}`), `centerLabel`, `centerValue`,
    `centerSub`, `centerTone`, `selectedId`, `selection` (`{label, amountText, tone, lines, note}`), `hint`,
    `loadTitle`, `seatsFilled`, `seatsTotal`, `loadText`, `weightBars` (`[{label, valueText, limitText, share,
    limitShare}]` for ZFW, TOW, LW; TOW and LW say "after import" before it), `planTitle`, `planCells`
    (`[{label, value}]`: cruise, distance, block, block fuel, burn, arrive in Dave's local time), `runwaysText`,
    `routeForCharts` (`routeWithRunways`), `openSimbriefLabel`, `importLabel`, `busy`, `message`.
  - Invokables: `openForAirplane(registration)` (from Fleet's Fly and from turnaround), `openEmpty()`, `dispatch()`
    (fetches the runways, creates the planned flight, shows the report), `changeFlight()` (back to the form from
    `fromPlanned`, before Accept only), `saveChange()` (`updatePlannedFlight`), `openSimbrief()`, `importPlan()`,
    `accept()`, `cancelFlight()`, `select(id)` (`""` clears it), `copyRoute()`.
  - Rules:
    - `importPlan` runs `planMismatch`, then `loadMismatch`. A refusal sets `message` and keeps everything else.
    - After block out, `importPlan` does nothing and `locked` is true (review 4.6).
    - A successful re-import after Accept calls `acceptPlannedFlight` again with the new preview, without asking
      (D-233), and keeps the previous preview in memory for `changesSince`.
    - `accept()` stores the breakdown, pushes the `flight_accepted` app event, updates the recorder's target
      (`YourFlightModel::reload()`) and switches the stage.
    - On the bridge's `block_out` with an imported, unaccepted flight, it stores the breakdown with
      `atBlockOut = true`; the recorder already wrote the marker (Task 5).
    - `cancelFlight` keeps today's guards (flight open; flown but not settled).

- [ ] **Step 1: Write the failing tests** (`tests/qt_models_test.cpp`, with a fake `Planner` returning fixture OFPs):
  - `Dispatch.FormToReportToAccept`: open N814SX, set the destination to KSJC, dispatch. The stage is `dispatched`,
    the board's value tiles are blank and Accept is off. Import the matching OFP: `imported`, a profit ring, and the
    first row is fares. Accept: `accepted`, `plannedAcceptance` exists, one `flight_accepted` in the app queue.
  - `Dispatch.RefusedImportKeepsEverything` (Review Focus 1): after Accept, a re-import with 43 passengers refuses with
    `loadMismatch`'s text, and the plan's `requestId` and the accepted lines are unchanged.
  - `Dispatch.ReimportAfterAcceptReplacesQuietly`: an OFP with more ramp fuel. The accepted fuel line changes, and
    `boardRows` marks only fuel and profit `changed`.
  - `Dispatch.RestartShowsStoredBreakdown` (Review Focus 2): accept, double the pilot rate in `rates.yaml`, build a new
    `YourFlightModel` and `DispatchModel` on the same database. The stage is `accepted`, the pilots row is the stored
    amount, and the recorder target has `accepted == true`.
  - `Dispatch.LockedAfterBlockOut`: a `block_out` event, then `importPlan()`, and the fake planner saw no fetch.
  - `Dispatch.BlockOutAccepts` (Review Focus 5): imported and not accepted, then `block_out`, so `plannedAcceptance`
    has `atBlockOut == true`.
  - `Dispatch.ChangeFlightKeepsLoads`: `changeFlight()`, change the flight number, `saveChange()`. The cabins are the
    same and the plan is dropped (`imported == false`).
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "Dispatch"`: FAIL.
- [ ] **Step 3: Implement.** Fleet's Fly / Fly Delivery calls `openForAirplane` and switches to Your Flight in place
  of opening New Flight (`FleetPage.qml`'s signal, wired in `Main.qml` in Task 10).
- [ ] **Step 4: Run** the full suite: PASS.
- [ ] **Step 5: Commit** `DispatchModel: form, dispatch report, import check, Accept, locked at block out (D-233)`.

### Task 9: The dispatch form and report in QML

Build to the board `Final` (read `project/Final.dc.html`), in the dark theme's tokens. Each component draws what
`DispatchModel` gives and decides nothing.

**Files:**
- Create: `qml/MoneyRing.qml` (a `Shape` with a `PathAngleArc` per slice; hover lifts a slice outward along its middle
  angle with a `Behavior` on its offset and shows a tag with label and amount just outside the ring; click selects),
  `qml/FlipBoard.qml` (rows of `FlipCard` tiles, 20 a row; each row a button that calls `select`; flips run once per
  import, row by row, and on re-import only for rows with `changed`), `qml/WorkingCard.qml`, `qml/LoadCard.qml`,
  `qml/PlanCard.qml` (before import: the three steps; after: the airports with an altitude-coloured line, runways,
  SID/STAR, the six cells, Copy route beside the route, Open SimBrief again ↗ and Re-import centred),
  `qml/DispatchReport.qml` (header with badges, the money card in three columns, load and plan cards in two),
  `qml/DispatchForm.qml` (the form's fields in the report's style: airplane, destination with its message, flight
  number, kind, fully loaded or random, belly cargo, extra fuel in lb, the rolled loads, Dispatch)
- Modify: `qml/Theme.qml` (ring families `ringFuel`, `ringAirport`, `ringCrew`, `ringService`, `ringProfit`, each with
  the mockup's shades; board tile colours and sizes; the report's sizes), `qml/FlipCard.qml` if it needs a mode
  without its own timer, `CMakeLists.txt` (QML files)

- [ ] **Step 1:** Write the components.
- [ ] **Step 2: Check** with the debug build, hot reload and `--demo` at a maximized 1920×1080 (D-248): before
  import, after import (the flip runs once), a re-import (only changed rows flip), a selected slice and row, and a
  delivery flight's loss ring. Then `--grab` pictures of each, compared side by side with the board's mockup.
- [ ] **Step 3:** qmllint: nothing new.
- [ ] **Step 4: Commit** `The dispatch form and report in QML: ring, split-flap board, working, load and plan (D-234)`.

### Task 10: The Your Flight page

**Files:**
- Create: `qml/YourFlightPage.qml` (from `LivePage.qml`), `qml/DispatchReportPopup.qml` (an overlay `Popup`, modal,
  the accepted report with Re-import until block out and read-only after)
- Modify: `qml/Main.qml`, `qml/Sidebar.qml` ("Your Flight"), `qml/FlightHeader.qml` (a View Dispatch Report button),
  `qml/InFlightPanel.qml` ("Finish Flight" becomes "Settle Up"), `qml/FleetPage.qml` (Fly goes to Your Flight),
  `qml/ReplayWindow.qml` (the renamed page and models), `CMakeLists.txt`
- Delete: `qml/LivePage.qml`, `qml/NewFlightDialog.qml`, `qml/PlannedFlightPanel.qml`

The page shows one thing per stage: `empty` and `form` the form (empty, or pre-filled); `dispatched` and `imported`
the report; `accepted` the header and Pre-Flight; `flying` and `arrived` the header and the in-flight panel; `summary`
today's `FlightSummary` (restyled in milestone 17), with At Gate Turnaround → `openForAirplane` for the same airplane.
The banner and notice stay on top.

- [ ] **Step 1:** Build the page and the popup, and do the renames and removals.
- [ ] **Step 2: Check** with `--demo`: Fleet → Fly → the form pre-filled → Dispatch → the report → Import (the demo
  planner's OFP) → Accept → Pre-Flight with View Dispatch Report → the popup → Cancel flight → back to the empty
  form. Then the replay window (`--replay last`) still plays. Then `--grab` of the form, the report, the popup and
  Pre-Flight.
- [ ] **Step 3:** qmllint: nothing new. The full suite passes.
- [ ] **Step 4: Commit** `Your Flight: dispatch until Accept, then the dashboard; View Dispatch Report; Settle Up (D-233)`.

### Task 11: Docs, the real migration, review

**Files:**
- Modify: `docs/design.md` (§13: Live → Your Flight, the flow, the planned flight panel gone), `docs/dispatch.md`
  (anything the build settled: the popup, the board labels), `docs/database.md`, `docs/recorder.md`,
  `docs/decisions.md` (D-252, below), `docs/roadmap.md` (16 built), `README.md` (Live → Your Flight), `CMakeLists.txt`
  (0.18.0), `docs/progress.md`

**D-251** (the plan's own calls) is already recorded and confirmed by Dave. Calls made while building go in a new
**D-252** (Proposed), listed in the end-of-milestone summary.

- [ ] **Step 1:** Update the docs so every spec agrees with the code (CLAUDE.md duty 3).
- [ ] **Step 2: Verify** (superpowers:verification-before-completion): `cmake --build --preset x64-debug` and
  `--preset x64-release` with no warnings; `ctest` passes in both; qmllint shows only today's info line.
- [ ] **Step 3: Migration 12 dry run.** Copy `%LOCALAPPDATA%\BlockTime\blocktime.db` to a scratch folder and start the
  release build with `--data-dir` on the copy. Check: the backup `blocktime.before-schema-12.db` is made;
  `foreign_key_check` is empty; `integrity_check` is ok; every old table's rows are unchanged; the ledger total is
  unchanged. If an unaccepted planned flight is in the copy, Your Flight opens on its report.
- [ ] **Step 4:** superpowers:requesting-code-review on the milestone's diff, then superpowers:receiving-code-review
  for the findings.
- [ ] **Step 5:** A `docs/progress.md` entry. Print the steps for Dave in the reply. No new sim probe is needed: on his
  next flight he should see the form, the report, Import with the check, Accept, the marker in the feed, and the
  money settled from the accepted lines. His first dispatch with extra fuel confirms `addedfuel`'s unit: SimBrief's
  OFP shows EXTRA as the pounds typed, and the report's badge matches.
- [ ] **Step 6: Commit** `Milestone 16: docs and verification`, then push `main` to `origin` (D-121).
