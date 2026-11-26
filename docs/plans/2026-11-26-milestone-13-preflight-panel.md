# Milestone 13: The Pre-Flight Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, D-033) to implement this plan
> task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** At the gate, Live shows everything Dave needs to set up the FMC and the EFB: a load sheet, the departure
conditions and the clearance, in the instrument panel look. The dashboard header collapses to the pill bar with a click.
The real app and the replay window both get it.

**Architecture:** The logic is plain C++ in `blocktime-core`, written tests first:
- D-168's plain form for an ATC exchange;
- reading a clearance and a taxi instruction from ATC's words;
- reading wind, temperature, precipitation and the altimeter from an ATIS and its METAR;
- the plan's extra fields from the stored OFP;
- the load sheet;
- a `PreFlightLog` that keeps what the origin's ATC and ATIS said since the flight was planned.

`FlightWatcher` starts polling ATC and the origin's ATIS at the gate instead of at block out, so the clearance reaches
the log, and Live, before pushback. A new Qt object, `PreFlightModel`, turns the log, the planned flight and the
bridge's clock into QML properties. A new `PreFlightPanel.qml` draws them in the instrument panel look. The header's
collapse is QML only, remembered in an ini file.

**Tech Stack:** C++20, MSVC, Qt 6 / QML (QtCore `Settings`), nlohmann::json, sqlite3, GoogleTest.

**Spec:** `docs/dashboard.md`, "Pre-Flight panel (D-153)" and "The instrument panel look (D-179)". Also D-187 (Dave's
scope for this milestone), D-188 (Claude's calls) and D-189 (SimBrief's SID and runway until ATC changes them), all
written with this plan.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`. `blocktime-app_qmllint` reports nothing new (the one Info in
  `ReplayBar.qml` is from milestone 11).
- Every new file starts with the GPL header. Every QML file has `import BlockTime`, `pragma ComponentBehavior: Bound`
  and `required` properties in delegates.
- Every colour, size and font goes in `qml/Theme.qml` (D-054). That includes the readouts' amber, the inset panel's
  shades and the screw heads.
- No continuous animations. The panel's one timer ticks once a second, and only while the clearance countdown shows.
- Nothing on Live reads the wall clock (D-166). Use only `RecorderBridge::now()`.
- All weights in pounds, written `13,439 lb`. No centre of gravity. BlockTime calculates no takeoff performance (D-118).
- **No frequency readout** beyond the clearance's departure frequency (D-187). "Now and next" waits for the in-flight
  panels' brainstorm.
- **The FMC line has only the cruise level and the transition altitude** (D-187): `FL400 · Trans alt 18,000`.
- Core keeps ISO UTC strings and `TimePoint`s. The Qt layer formats local times.
- Version 0.14.0.
- Test commands: `cmake --build --preset x64-debug`, then `ctest --preset x64-debug`, and the same with `x64-release`
  (`README.md`). 741 tests pass at the start.

## Review Focus

1. **A turnaround at the gate.** After block in, the previous leg's clearance and ATIS must not show on the next leg's
   panel. The same applies after BlockTime restarts and re-logs the whole SayIntentions history. Messages count only
   from the origin airport and only from when the flight was planned. Tests: `PreFlightLog.IgnoresMessagesBeforePlanning`
   and `PreFlightLog.BlockInStartsOver` (Task 5).
2. **ATC changes the runway or the SID.** The plan's runway and SID show from the start (D-189). If the clearance
   says `Expect runway 8L` or names a SID the plan doesn't have, the panel shows ATC's value marked as changed and
   never silently keeps the plan's. If ATC changes nothing, the plan's stay. Tests:
   `DepartureView.AtcRunwayWins` and `ClearanceView.UnknownSidIsMarkedChanged` (Task 5).
3. **No SimBrief plan yet.** The panel shows the passengers, and the clearance once it comes, with `—` for block fuel,
   ZFW, cargo and heading. It never shows 0 lb. Tests: `LoadSheet.NoPlan` (Task 4) and
   `PreFlightModel.WaitingForPlan` (Task 7).
4. **The sim isn't running.** No SayIntentions calls while a flight is merely planned. Test:
   `FlightWatcher.PreFlightWaitsForTheSim` (Task 6).
5. **A clearance read back late, or repeated.** SayIntentions logs an exchange again when its content changes, and
   BlockTime re-logs the history at block out. The countdown runs from ATC's own time (`stamp_zulu`), not the log
   line's, and the latest clearance by time wins. Test: `PreFlightLog.CountdownFromAtcTime` (Task 5).

---

### Task 1: The mockup, for Dave's approval

Milestone 12's header went through three rounds of mockups (D-184). This panel starts the same way (D-187). No code
is built until Dave approves.

**Files:**
- Create: `docs/images/dashboard-preflight-v1.html` (self-contained, like `dashboard-header-v2.html`)

- [ ] **Step 1: Draw the gate view** in the instrument panel look (`dashboard-instrument-panel.html`'s inset, screws,
  amber radio-stack readouts). Use the real numbers from Dave's KSAN → KSJC leg (XLY 8, N814SX, 2026-11-25), which
  are the values the tests use:
  - **Load sheet:** block fuel 13,439 lb · ZFW 112,967 lb · passengers 44 (the BBJ2 has one cabin, so no class
    split) · cargo 2,420 lb, split forward 1,075 / aft 1,345 · `FL400 · Trans alt 18,000`.
  - **Departure:** runway 27 (the plan's 27) · heading 275° · Dry · wind Calm · OAT 19 °C · altimeter 30.09 inHg ·
    ATIS L.
  - **Clearance:** SID `PADRZ2 IKAYE` · `Climb via SID` · squawk 3166 (transponder style) · departure 119.6 · the
    countdown `26:40`, amber, red in the last 5 minutes · the taxi route `A, A7, B, B1` once ground gives it.
- [ ] **Step 2: Add the collapsed header** above the panel, with a click on the pill toggling it. Collapsed, it shows
  `KSAN` left of the pill, `KSJC` right of it, the marker with its sub-phase label, and only the current status line.
  Expanded, it is today's header.
- [ ] **Step 3: Add a Next button** that steps through the states Dave will see, like `dashboard-header-v2.html`:
  1. waiting for the plan;
  2. plan in, no ATIS yet (the plan's runway 27 and SID `PADRZ2 IKAYE` already show, not yet cleared);
  3. ATIS in;
  4. cleared;
  5. the runway changed by ATC (`Expect runway 09`, shown as changed from the plan's 27);
  6. taxiing (block out: the countdown is gone, the taxi route shows).
- [ ] **Step 4: Show Dave** (open the file in the browser pane). Rework it until he approves, then record anything he
  changed in D-187 or the spec.
- [ ] **Step 5: Commit** "Pre-Flight panel mockup approved (D-187)", with a still image
  `docs/images/dashboard-preflight-v1.png` grabbed from the browser pane.

### Task 2: ATC's words: the plain form, the clearance and the taxi route

**Files:**
- Create: `src/atc_message.h/.cpp`, `src/clearance.h/.cpp`; add them to `blocktime-core` in `CMakeLists.txt`
- Test: `tests/atc_message_test.cpp`, `tests/clearance_test.cpp`; add them to `blocktime-tests`

**Interfaces:**
- Produces (`atc_message.h`, D-168: dashboard code reads ATC only through this):
  ```cpp
  struct AtcMessage {
      long long id = -1;
      std::string time;       // ATC's own time, stamp_zulu ("2026-11-25T18:39:45Z"); "" if none
      std::string airport;    // ident: "KSAN"
      std::string position;   // "CLEARANCE", "GROUND", "TOWER", ...; "" if none
      std::string station;    // "Lindbergh Clearance"
      std::string frequency;  // "125.9"
      std::string atc;        // outgoing_message: ATC's words; "" when only the pilot spoke
      std::string pilot;      // incoming_message
  };
  // From an atc_exchange line's data ({"entry": {...}, ...}). nullopt without an entry, or for a channel that isn't
  // COM (cabin crew and PA are on INTERCOM). SayIntentions' nulls read as "".
  std::optional<AtcMessage> atcMessage(const nlohmann::json& atcExchangeData);
  ```
- Produces (`clearance.h`):
  ```cpp
  struct Clearance {
      std::string sid;                 // "PADRZ2"; "" when none is named
      std::string initialAltitude;     // "Climb via SID", "5,000 ft", "FL190"; "" when none is heard
      std::string departureFrequency;  // "119.6", as spoken
      std::string squawk;              // "3166"
      std::string runway;              // "Expect runway 8L" → "08L" (normalizeRunway); ""
  };
  // ATC's words. nullopt unless they contain "cleared to" and a squawk.
  std::optional<Clearance> parseClearance(std::string_view atcWords);

  struct TaxiInstruction {
      std::string runway;  // normalized, "27"
      std::string route;   // "A, A7, B, B1": everything after "via", up to the end, without the final full stop
  };
  // nullopt unless the words have "taxi" and then "via".
  std::optional<TaxiInstruction> parseTaxi(std::string_view atcWords);

  inline constexpr std::chrono::minutes kClearanceValidity{30};  // D-153
  struct Countdown {
      std::string text;  // "26:40", "0:00" once expired
      bool urgent = false;   // 5 minutes or less left
      bool expired = false;
  };
  Countdown clearanceCountdown(TimePoint clearedAt, TimePoint now);
  ```

- [ ] **Step 1: Write the failing tests:**
  - `AtcMessage.ReadsTheClearanceEntry`: the first `atc_exchange` line of `tests/data/flight-ksan-ksjc` gives id
    110637058, time `2026-11-25T18:39:40Z`, airport `KSAN`, position
    `CLEARANCE`, frequency `125.9`, and `atc` starting with `Cleared to Norman Y. Mineta San Jose`.
  - `AtcMessage.NullWordsAreEmpty`: entry 110637996 (`outgoing_message` null) has `atc` "" and `pilot` `Push and start
    approved. Tail west.`
  - `AtcMessage.IntercomIsNotAtc`: an entry with `channel` `INTERCOM` → nullopt. So is data with no `entry` object.
  - `Clearance.KsanPadrz2`: the KSAN words → sid `PADRZ2`, initialAltitude `Climb via SID`, departureFrequency
    `119.6`, squawk `3166`, runway `27`.
  - `Clearance.KlasRadyr2`: `Cleared to San Diego via the RADYR2 departure. Then as filed. Climb via the SID. Expect
    FL360 one-zero minutes after departure. Departure on 125.9. Squawk 2546. Expect runway 8L` → `RADYR2`,
    `Climb via SID`, `125.9`, `2546`, `08L`.
  - `Clearance.ClimbAndMaintain`: `... Climb and maintain 5000. Expect FL350 ...` gives `5,000 ft`; `maintain flight
    level 190` gives `FL190`. The `Expect FL…` is never the initial altitude.
  - `Clearance.NotAClearance`: `Readback correct. ground will be on 123.9.` → nullopt.
  - `Taxi.KsanRoute`: `Runway 27 taxi via A, A7, B, B1.` → runway `27`, route `A, A7, B, B1`.
  - `Taxi.NotTaxi`: `Push and start approved. Tail West.` → nullopt.
  - `Countdown.Runs`: cleared at T. At T the text is `30:00`, not urgent. At T+3:20 it is `26:40`. At T+25:00 it is
    `5:00` and urgent. At T+31:00 it is `0:00`, urgent and expired.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "AtcMessage|Clearance|Taxi|Countdown"` → FAIL.
- [ ] **Step 3: Implement.** Match case-insensitively with `std::regex` (the strings are short). Patterns:
  - SID: `via the (\w+) departure`;
  - initial altitude: `climb via the sid`, else `maintain (flight level |FL)?([\d,]+)`, taken only before any
    `expect`;
  - departure frequency: `departure on (\d{3}\.\d{1,3})`;
  - squawk: `squawk (\d{4})`;
  - runway: `expect runway (\w+)`;
  - taxi: `runway (\w+).*?taxi.*?via (.+)`.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "Read the clearance and the taxi route from ATC's words (D-168, D-153)".

### Task 3: The departure conditions from the ATIS

**Files:**
- Create: `src/departure_conditions.h/.cpp` (core)
- Test: `tests/departure_conditions_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct Wind {
      bool calm = false, variable = false;
      int directionDeg = 0, speedKt = 0, gustKt = 0;  // gust 0 = none
  };
  // The spoken ATIS first (SayIntentions' text: magnetic in the US), else the METAR (true).
  std::optional<Wind> atisWind(std::string_view atisText);  // "WIND CALM", "WIND VRB AT 4", "WIND 330 AT 9 GUST 15"
  std::optional<Wind> metarWind(std::string_view metar);    // "00000KT", "VRB04KT", "33009G15KT"
  std::string windText(const Wind& w);  // "Calm", "Variable / 4 kt", "330° / 9 kt", "330° / 9G15 kt"
  std::optional<int> metarTemperatureC(std::string_view metar);  // "19/05" → 19, "M02/M05" → -2
  bool metarPrecipitation(std::string_view metar);  // a weather group with DZ RA SN SG PL GR GS or UP
  std::string altimeterText(std::string_view altimeter);  // "30.09" → "30.09 inHg", "1013" → "1013 hPa", "" → ""
  ```

- [ ] **Step 1: Write the failing tests**, using the ATIS texts and METARs logged in the two flight fixtures:
  - `Conditions.KsanLima`: the text gives a calm wind, and `windText` is `Calm`. The METAR's temperature is 19 and it
    has no precipitation. `altimeterText("30.09")` is `30.09 inHg`.
  - `Conditions.KsjcJuliet`: `WIND VRB AT 4` → variable, 4 kt.
  - `Conditions.SpokenWindWithGust`: `WIND 330 AT 9 GUST 15` → `330° / 9G15 kt`.
  - `Conditions.MetarWindFallback`: `33009G15KT` → 330 / 9 / 15. `VRB04KT` → variable 4. `00000KT` → calm.
  - `Conditions.Precipitation`: `-RA`, `+TSRA`, `SHSN` and `FZDZ` are precipitation. `VCSH`, `BR` and `FEW200` are
    not.
  - `Conditions.HpaAltimeter`: `1013` → `1013 hPa`.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R Conditions` → FAIL.
- [ ] **Step 3: Implement.** The METAR's weather groups are space-separated tokens between the visibility and the
  clouds. Test each token against `^[-+]?(VC)?(MI|BC|PR|DR|BL|SH|TS|FZ)?(DZ|RA|SN|SG|PL|GR|GS|UP)+$`. A `VC` group
  doesn't count, since it's near the airport, not on it.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "Read wind, temperature, precipitation and the altimeter for the Pre-Flight panel".

### Task 4: The plan's extras and the load sheet

**Files:**
- Create: `src/plan_brief.h/.cpp` (core), `tests/data/ofp-ksan-ksjc-brief.json`
- Modify: `src/planned_flight.h/.cpp` (`PlannedFlightView::brief` and `holds`, filled by `currentPlannedFlight` and
  `flownFlightView`)
- Modify: `config/catalog.yaml` (the 737-800 BCF's holds, from Dave's `simbrief.ps1`)
- Modify: `docs/notes/simbrief-api.md` (the new fields)
- Test: `tests/plan_brief_test.cpp`, `tests/planned_flight_test.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct PlanRunway {
      std::string ident;  // normalized, "27"
      int magneticCourseDeg = 0;
  };
  struct PlanBrief {
      std::string sid, sidTransition;  // general.sid_ident "PADRZ2", general.sid_trans "IKAYE"
      int zfwLb = 0;                   // weights.est_zfw
      int cargoLb = 0;                 // weights.cargo: bags and freight together (2,420 = 44 bags x 55 lb)
      std::vector<PlanRunway> runways; // tlr.takeoff.runway[]: identifier, magnetic_course
      std::string surface;             // tlr.takeoff.conditions.surface_condition, "dry"; "" without a TLR
  };
  // Read from the stored OFP when the view loads: no migration (D-188). Missing fields stay empty or 0.
  PlanBrief parsePlanBrief(std::string_view ofpJson);

  // On PlannedFlightView: std::optional<PlanBrief> brief (set with plan); std::vector<Hold> holds (the airplane's).

  struct LoadSheet {
      bool hasPlan = false;
      int blockFuelLb = 0, zfwLb = 0;  // block fuel is the plan's ramp fuel: the total on board, not what to add
      std::vector<CabinLoad> passengers;  // the planned cabins; empty for a cargo or delivery flight
      int cargoLb = 0;                    // SimBrief's cargo
      std::vector<HoldLoad> holds;        // splitHolds(view.holds, cargoLb); empty when it doesn't fit (below)
      int cruiseAltitudeFt = 0, transitionAltitudeFt = 0;  // the FMC line (D-187)
  };
  LoadSheet loadSheet(const PlannedFlightView& flight);
  ```

- [ ] **Step 1: Make the fixture.** Copy Dave's latest flown OFP (`flight_plan.ofp_json` in
  `%LOCALAPPDATA%\BlockTime\blocktime.db`, KSAN → KSJC, request 194762580) into `tests/data/ofp-ksan-ksjc-brief.json`.
  Cut it down to these fields:
  - `general.sid_ident`, `sid_trans`, `initial_altitude`;
  - `origin.icao_code`, `trans_alt`;
  - `weights.est_zfw`, `cargo`, `pax_count`;
  - `fuel.plan_ramp`;
  - `tlr.takeoff.conditions.surface_condition`;
  - `tlr.takeoff.runway[].identifier`, `magnetic_course` (09 → 95, 27 → 275).

  Keep no user ids. The sqlite3 CLI isn't installed; Windows' own `winsqlite3.dll` read-only works from PowerShell.
- [ ] **Step 2: Write the failing tests:**
  - `PlanBrief.KsanKsjc`: sid `PADRZ2`, transition `IKAYE`, ZFW 112,967, cargo 2,420, runways {09, 95} and {27, 275},
    surface `dry`.
  - `PlanBrief.OldFixtureHasNone`: `ofp_v2_min.json` has no SID fields and no TLR. Sid is "", there are no runways,
    and ZFW is 102,187.
  - `LoadSheet.Bbj2`: a view with one economy cabin of 44, holds forward 4 / aft 5 and the brief above gives block fuel
    13,439, ZFW 112,967, passengers [economy 44], holds forward 1,075 / aft 1,345, FL 40,000 and transition 18,000.
  - `LoadSheet.Freighter`: kind cargo, no cabins, holds main deck / forward / aft → no passengers, and the holds add up
    to the cargo.
  - `LoadSheet.NoPlan`: no plan → `hasPlan` false, the passengers still there, no holds.
  - `LoadSheet.CargoOverCapacity`: SimBrief's cargo is more than the holds' max in total (many holds are still
    catalog estimates) → `cargoLb` is SimBrief's figure and `holds` is empty. `splitHolds` asserts that the cargo
    fits, so it must not be called.
  - `PlannedFlight.ViewHasBriefAndHolds`: the view of a planned flight with a stored plan has `brief` and the airplane's
    holds. `flownFlightView` gives the same.
- [ ] **Step 3: Run** `ctest --preset x64-debug -R "PlanBrief|LoadSheet|ViewHasBrief"` → FAIL.
- [ ] **Step 4: Implement.**
  - The view's existing plan query already selects `ofp_json`, so parse the brief from it next to `parseNavlog`.
  - Holds come from `holdsOf`. The SimBrief values are strings, read like `parseOfp` reads them.
- [ ] **Step 5: The BCF holds.** In `config/catalog.yaml`, replace the 737-800 BCF Freighter's estimated holds with
  the max-load distribution from Dave's `simbrief.ps1`:
  ```yaml
  - { hold: main_deck, max_lb: 34960, ratio: 34960 }   # from Dave's simbrief.ps1 (737-800BCF max-load distribution)
  - { hold: forward, max_lb: 4892, ratio: 4892 }
  - { hold: aft, max_lb: 6108, ratio: 6108 }
  ```
  This changes only airplanes leased later (D-082). Dave has no BCF today. Tell him that his own
  `%LOCALAPPDATA%\BlockTime\catalog.yaml` needs the same edit if he wants it.
- [ ] **Step 6: Run** all tests → PASS.
- [ ] **Step 7: Commit** "The load sheet from the stored plan; the BCF's holds from Dave's script (D-188)".

### Task 5: What the origin's ATC and ATIS said

**Files:**
- Create: `src/preflight_log.h/.cpp` (core)
- Test: `tests/preflight_log_test.cpp`

**Interfaces:**
- Consumes: `atcMessage`, `parseClearance`, `parseTaxi` (Task 2); `Atis`'s fields as logged (`atis` events);
  `PlanBrief`, `PlannedFlightView` (Task 4); the condition readers (Task 3).
- Produces:
  ```cpp
  // Fed every flight and block event in log order, like FlightStages. Keeps what the origin said since planning.
  class PreFlightLog {
  public:
      // A different origin or planning time starts over.
      void setFlight(std::string originIcao, std::string plannedAt /* UTC instant */);
      void onEvent(std::string_view type, const nlohmann::json& data);

      const std::optional<Clearance>& clearance() const;  // the latest by ATC's time
      std::optional<TimePoint> clearedAt() const;
      const std::optional<TaxiInstruction>& taxi() const; // the latest before liftoff
      std::string atcRunway() const;  // the taxi's runway, else the clearance's; ""
      const nlohmann::json* atis() const;  // the origin's latest `atis` data; nullptr before one
      bool blockedOut() const;  // block_out seen
      bool airborne() const;    // liftoff seen
  };

  struct DepartureView {
      std::string runway, planRunway;  // runway: ATC's, else the ATIS's first departing, else the plan's
      bool runwayChanged = false;      // both set and different
      std::optional<int> headingDeg;   // the brief's magnetic course for `runway`
      std::string condition;           // "Wet" when the METAR has precipitation, else the plan's surface ("Dry")
      std::string wind, temperature, altimeter, atisLetter;  // "" before an ATIS; temperature "19 °C"
  };
  DepartureView departureView(const PlannedFlightView& flight, const PreFlightLog& log);

  struct ClearanceView {
      bool received = false;
      // The plan's full form "PADRZ2 IKAYE" from the start (Dave, D-189). Once cleared: the plan's form when ATC's SID
      // has the plan's name (or ATC named none), else ATC's own, marked changed.
      std::string sid;
      bool sidChanged = false;  // ATC gave a SID the plan doesn't have
      std::string initialAltitude, squawk, departureFrequency, taxiRoute;  // "" until ATC gives them
  };
  ClearanceView clearanceView(const PlannedFlightView& flight, const PreFlightLog& log);
  ```
- Rules:
  - An `atc_exchange` counts only if its airport is the origin and ATC's time is at or after `plannedAt`.
  - `block_in` and `flight_interrupted` start over.
  - `block_out` and `liftoff` only set their flags.
  - A taxi instruction after liftoff is ignored.
  - The plan's runway is `plan.originRunway`, else the planned flight's `originRunway`.

- [ ] **Step 1: Write the failing tests:**
  - `PreFlightLog.KsanKsjcFixture`: feed every line of `tests/data/flight-ksan-ksjc` with `setFlight("KSAN",
    "2026-11-25T18:00:00Z")`.
    - At block out the clearance is PADRZ2 and clearedAt is `18:39:40Z` (ATC's words, not the readback's
      `18:39:45Z`), the ATIS is letter L, and `blockedOut` is true.
    - At liftoff the taxi route is `A, A7, B, B1` and `airborne` is true.
  - `PreFlightLog.IgnoresMessagesBeforePlanning`: the same clearance with plannedAt `18:45:00Z` → no clearance.
  - `PreFlightLog.OtherAirportIgnored`: the clearance with origin `KLAS` → no clearance.
  - `PreFlightLog.BlockInStartsOver`: clearance, then `block_in` → no clearance, no ATIS, flags false.
  - `PreFlightLog.CountdownFromAtcTime`: the clearance logged again 3 minutes later, with the same `stamp_zulu` →
    clearedAt unchanged. A newer clearance (a later stamp, a new squawk) replaces it.
  - `DepartureView.PlanThenAtis`: no ATIS → runway `27` from the plan, heading 275, condition `Dry`, empty wind. With
    an ATIS departing `30L,30R` → runway `30L`, changed from the plan, and no heading (not in the brief).
  - `DepartureView.AtcRunwayWins`: the ATIS says 27, the clearance says `Expect runway 09` → runway `09`, changed,
    heading 95.
  - `DepartureView.WetWithRain`: METAR `-RA` → `Wet`.
  - `ClearanceView.PlanSidBeforeClearance`: plan PADRZ2 / IKAYE, no clearance → not received, sid `PADRZ2 IKAYE`,
    not changed. No plan and no clearance → sid "".
  - `ClearanceView.FullSid`: plan PADRZ2 / IKAYE with clearance PADRZ2 → `PADRZ2 IKAYE`, not changed.
  - `ClearanceView.NoSidInClearanceKeepsPlan`: a clearance naming no SID (`via radar vectors`) → `PADRZ2 IKAYE`, not
    changed.
  - `ClearanceView.UnknownSidIsMarkedChanged`: clearance `ZOOO4`, plan PADRZ2 → `ZOOO4`, changed.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R "PreFlightLog|DepartureView|ClearanceView"` → FAIL.
- [ ] **Step 3: Implement.** Compare runways after `normalizeRunway`.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "PreFlightLog: the origin's clearance, taxi route and ATIS since planning".

### Task 6: Poll SayIntentions at the gate

Today `FlightWatcher` starts at block out, so the clearance (given about 3 minutes before pushback on KSAN → KSJC)
reaches the log only at block out.

**Files:**
- Modify: `src/flight_watcher.h/.cpp`, `src/app_main.cpp`
- Modify: `docs/recorder.md` ("ATC log, ATIS and altimeter"), the header comment in `flight_watcher.h`
- Test: `tests/qt_models_test.cpp` (`WatcherRig`)

**Interfaces:**
- Produces:
  ```cpp
  // The planned flight's origin ("" = nothing planned). While one is planned, the sim is connected and no flight is
  // open, ATC is polled at the ATC interval and the origin's ATIS at the ATIS interval, logged with flight_id "".
  void FlightWatcher::setPreFlightOrigin(const QString& originIcao);
  ```
- The app calls it from `PlannedFlightModel::changed` with the planned flight's origin, or "" when none. The watcher
  listens to `RecorderBridge::connectionChanged` itself (`"connected"`).
- The ATIS interval in the app becomes 5 minutes for both airports (D-118, D-188), instead of 10.
- Block out keeps today's behaviour: it resets the dedupes, so the whole history is logged again with the flight id,
  and the debrief keeps everything even when the clearance came more than 30 minutes before block out.

- [ ] **Step 1: Write the failing tests:**
  - `FlightWatcher.PreFlightPollsWhileConnected`: the bridge is connected, then `setPreFlightOrigin("KBFI")`. The
    first wx call is `KBFI`, comms are fetched, and the pushed `atis` has role `origin` and flight_id "".
  - `FlightWatcher.PreFlightWaitsForTheSim`: not connected → no calls after a wait. Connecting starts them.
  - `FlightWatcher.PreFlightStopsWhenThePlanGoes`: `setPreFlightOrigin("")` → no more calls.
  - `FlightWatcher.PreFlightHandsOverAtBlockOut`: the pre-flight polls run, then block out → the lines carry the flight
    id, and the existing tests still pass.
  - `FlightWatcher.NoPreFlightAfterBlockIn`: after block in, no pre-flight polls until `setPreFlightOrigin` is called
    again with the next leg's origin.

  If the rig's fake planner doesn't count comms calls, add a counter.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R FlightWatcher` → FAIL.
- [ ] **Step 3: Implement.**
  - Keep an `open_` flag (block out → true; block in or interrupted → false), separate from `flightId_`, which block
    in still needs for its last fetch.
  - One `updatePreFlight()` starts or stops the ATC and origin timers.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "Poll ATC and the origin's ATIS at the gate, so the clearance shows before pushback (D-188)".

### Task 7: PreFlightModel

**Files:**
- Create: `src/preflight_model.h/.cpp` (the app library, next to `dashboard_header`)
- Modify: `src/app_main.cpp` (one for Live, as the `preflight` initial property), `src/replay_session.h/.cpp` (one
  per replay bridge, `Q_PROPERTY(PreFlightModel* preflight READ preflight NOTIFY restarted)`)
- Test: `tests/qt_models_test.cpp`

**Interfaces:**
- Consumes: `RecorderBridge` (`flightEvent`, `blockEvent`, `now()`), `PlannedFlightDisplay` (`view()`, `changed`),
  `PreFlightLog`, `loadSheet`, `departureView`, `clearanceView`, `clearanceCountdown`.
- Produces, a `QML_ELEMENT`, `QML_UNCREATABLE` `PreFlightModel` (QML can't have two `PreFlightPanel` types):
  ```cpp
  PreFlightModel(RecorderBridge& bridge, PlannedFlightDisplay& planned, QObject* parent = nullptr);
  ```
  It has these `Q_PROPERTY`s, all `NOTIFY changed`. Texts are ready to show, and `—` means not known yet:
  - `bool visible`: a planned flight is on show and it hasn't lifted off;
  - `bool hasPlan`;
  - `QString blockFuelText`, `zfwText` (`13,439 lb`);
  - `QVariantList passengers` (`[{label: "Economy", value: "44"}]`, the class omitted with one cabin);
  - `QString cargoText`;
  - `QVariantList holds` (`[{label: "Forward", value: "1,075 lb"}]`);
  - `QString fmcText` (`FL400 · Trans alt 18,000`);
  - `QString runway`, `planRunway`, `bool runwayChanged`;
  - `QString headingText` (`275°`), `conditionText`, `windText`, `temperatureText`, `altimeterText`, `atisLetter`;
  - `bool cleared`, `QString sid` (the plan's before the clearance, D-189), `bool sidChanged`, `initialAltitude`,
    `squawk`, `departureFrequency`, `taxiRoute`;
  - `QString countdownText`, `bool countdownUrgent`, `bool countdownVisible` (cleared and not blocked out).
- Each `flightEvent` is converted to JSON the way `RecorderBridge::applyFlightEvent` does, and fed to the log.
  `blockEvent` feeds its type with `{}`. `planned.changed` calls `setFlight(origin, plannedAt)`.
- A `QTimer` ticks once a second only while `countdownVisible`.

- [ ] **Step 1: Write the failing tests:**
  - `PreFlightModel.WaitingForPlan`: a planned flight with no plan → visible, `blockFuelText` `—`, passengers there.
  - `PreFlightModel.ClearanceAndCountdown`: feed the KSAN clearance and set the bridge's clock 3:20 after it →
    `cleared`, squawk `3166`, `countdownText` `26:40`, not urgent. Block out → `countdownVisible` false, the timer
    stopped.
  - `PreFlightModel.HiddenFromLiftoff`: `liftoff` → `visible` false.
  - `PreFlightModel.ReplayHasOne`: a `ReplaySession` on the KSAN → KSJC fixture has a non-null `preflight` that is
    visible at the start.
- [ ] **Step 2: Run** `ctest --preset x64-debug -R PreFlightModel` → FAIL.
- [ ] **Step 3: Implement**, and wire it in `app_main.cpp` and `ReplaySession::makeBridge`.
- [ ] **Step 4: Run** all tests → PASS.
- [ ] **Step 5: Commit** "PreFlightModel: the Pre-Flight panel's values for Live and the replay".

### Task 8: The Pre-Flight panel on Live

**Files:**
- Create: `qml/InstrumentPanel.qml` (the inset with its screw heads, a container), `qml/Readout.qml` (amber digits in
  a dark window with an engraved placard), `qml/PreFlightPanel.qml`; add them to `CMakeLists.txt`
- Modify: `qml/LivePage.qml` (`required property PreFlightModel preflight`; the panel right under the header),
  `qml/Main.qml`, `qml/ReplayWindow.qml`, `qml/Theme.qml`

**Interfaces:**
- Consumes: `PreFlightModel` (Task 7). Drawn as the approved mockup (Task 1).

- [ ] **Step 1: Build the three QML files** to the mockup.
  - The load sheet, departure and clearance cards sit inside one `InstrumentPanel`. Their numbers use `Readout`, and
    the squawk is transponder style.
  - A changed runway or SID is flagged as the mockup shows.
  - The countdown is amber, and red when `countdownUrgent`.
  - `InstrumentPanel` and `Readout` are general, ready for the in-flight panels.
- [ ] **Step 2: Put it on `LivePage.qml`** below `FlightHeader`, above the summary loader and `PlannedFlightPanel`,
  with `visible: page.preflight.visible`. Pass `preflight` from `Main.qml` and `ReplayWindow.qml` (`replay.preflight`).
- [ ] **Step 3: Run** qmllint, the build and all tests.
- [ ] **Step 4: Look at it** (`set QT_QPA_PLATFORM=offscreen`):
  - `BlockTime.exe --replay last --grab gate.png` (the replay's opening, before block out);
  - `--at block_out` (cleared in Dave's older logs, where the clearance arrives at block out);
  - `--at taxi_out` (the taxi route);
  - `--at climb` (the panel gone).

  Compare with the mockup, fix what looks wrong and grab again. Check the gate view at the laptop's window size: with
  the header collapsed (Task 9), the panel should be above the fold.
- [ ] **Step 5: Commit** "The Pre-Flight panel on Live and in the replay (milestone 13)".

### Task 9: The header collapses

**Files:**
- Modify: `qml/FlightHeader.qml`, `qml/Theme.qml`, `src/app_main.cpp`

**Interfaces:**
- The collapsed state is remembered across restarts in `%LOCALAPPDATA%\BlockTime\BlockTime.ini` (D-188):
  - `app_main.cpp` sets `QCoreApplication::setOrganizationName("BlockTime")`, `QSettings::setDefaultFormat(IniFormat)`
    and `QSettings::setPath(IniFormat, UserScope, <%LOCALAPPDATA%>)` before the QML engine loads;
  - `FlightHeader.qml` has `import QtCore` and `Settings { category: "dashboard"; property alias headerCollapsed:
    card.collapsed }`.

  The replay window shares the setting.

- [ ] **Step 1: Add `property bool collapsed`** and a `MouseArea` (pointing-hand cursor) over the pill and the marker
  row that toggles it.
  - **Collapsed:** one row with the origin's ICAO code, the pill and the destination's ICAO code. Under it, the marker
    with its sub-phase label (D-187), then only the current status line.
  - **Hidden when collapsed:** the airline line, the clock, the arc, the airport blocks, the older status lines and the
    checklist.
  - The change is instant (no animation). The marker doesn't slide when the pill's width changes, as on a resize
    today.
- [ ] **Step 2: Run** qmllint and the build. Then grab collapsed and expanded, at the gate and in cruise
  (`--replay last --at cruise`). For the collapsed grab, set `headerCollapsed=true` in a scratch copy's ini. Check
  that clicking in the hot-reload debug build toggles it and that the setting survives a restart.
- [ ] **Step 3: Commit** "The dashboard header collapses to the pill bar with a click (D-187)".

### Task 10: Docs, version, review and close

- [ ] **Step 1: Docs.**
  - `docs/dashboard.md`: the Pre-Flight panel and the collapse as built, with the frequencies moved to the in-flight
    brainstorm (D-187) and the open item about fitting on the screen closed by the collapse.
  - `design.md` section 13.5 (Pre-Flight shows the panel).
  - `docs/recorder.md`: the pre-flight polling.
  - `roadmap.md`: milestone 13 built; the frequencies line added to "Then".
  - Add a decisions entry for the smaller calls made while building (Proposed).
  - `docs/README.md` if a note was added.
- [ ] **Step 2: Version** 0.14.0 in `CMakeLists.txt`. Build both presets, then run all tests and qmllint.
- [ ] **Step 3: Code review** (`requesting-code-review`, then `receiving-code-review`), and fix the Important
  findings.
- [ ] **Step 4: Progress entry** in `docs/progress.md`:
  - what's verified (tests, grabs) and what isn't;
  - "what Dave should see" as numbered steps;
  - "on your next flight": the clearance shows within a minute of the readback, before pushback, and the ATIS letter
    updates.
- [ ] **Step 5: Commit** "Milestone 13 built: the Pre-Flight panel (0.14.0)". Push `main` once Dave has tried it and
  closes the milestone (D-121).
