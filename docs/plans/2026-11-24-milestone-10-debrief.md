# Milestone 10 (Procedure capture and the debrief) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every flight is debriefed at settling: BlockTime measures twelve procedure checks and the landing from the
event log, stores the measurements, and shows a 0–100 score and 0–5 landing stars computed from them. Claude Opus 5.5
writes a review from the same facts and the ATC log. The Flights screen shows the running averages. The MVP is done.

**Architecture:**
- **Recorder:** a `cockpit` event (landing lights, seat belts, gear, VREF, landing flaps, autopilot) from plain
  SimVars and PMDG's broadcast, now read in normal running (D-159); `touchdown` gains pitch, bank, heading and wind;
  a new `bounces` event 15 s after touchdown.
- **Core (plain C++, tested):** `flight_events` reads one flight's lines back from the log; `debrief_measure` turns
  them into stored measurements; `debrief_score` turns measurements into outcomes, stars and the score, every time
  they're shown (D-160); `debrief_review` builds Claude's request and parses its answer.
- **Database:** migration 7 adds the measurement, landing and review tables, written by `settleFlight` in its
  transaction (the review later, once).
- **App (Qt):** `ClaudeReviewer` makes the HTTPS call; `ReviewWriter` runs it once after settling and on Retry;
  `FlightListModel` adds the debrief to the summary and the averages; FlightSummary and FlightsPage show them.
- **Replay tool:** `blocktime-debrief.exe` runs the same core code on a saved log.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, gtest,
yaml), Qt 6 Quick + Network, SimConnect (MSFS 2020 SDK), PMDG SDK headers (optional), the Claude Messages API.

**Spec:**
- `docs/debrief.md` (the design, approved by Dave 2026-11-24). Every number below comes from it.
- Decisions D-122, D-123, D-135, **D-156..D-161**.
- `docs/notes/probe-flight.md` (PMDG fields, plain SimVars that follow them), `docs/notes/sayintentions-api.md` (the
  ATC log's fields), `docs/notes/msfs-observed-behaviour.md` (STD reads as 1013.2).

## Global Constraints

- **Build and test:** no warnings at `/W4 /permissive- /utf-8` in release and debug; `cmake --build --preset
  x64-release` then `ctest --preset x64-release` all pass; `cmake --build --preset x64-debug --target
  blocktime-app_qmllint` reports nothing.
- **Files:** GPL header on every new file; QML `import BlockTime`, `pragma ComponentBehavior: Bound`, every colour,
  size and font from `Theme.qml` (D-150); `blocktime-core` and its tests include no Qt, `windows.h` or `SimConnect.h`.
- **Recorder budget:** telemetry gains only `LIGHT LANDING` (bool, eps 0), `CABIN SEATBELTS ALERT SWITCH` (bool, eps 0),
  `GEAR TOTAL PCT EXTENDED` (percent, eps 1), `PERIOD_SECOND` + `CHANGED` like the rest, and they are **not** added to
  the `telemetry` JSON (the `cockpit` event carries them). The touchdown samples gain `PLANE PITCH DEGREES`,
  `PLANE BANK DEGREES` (degrees), `PLANE HEADING DEGREES TRUE` (degrees), `AMBIENT WIND DIRECTION` (degrees),
  `AMBIENT WIND VELOCITY` (knots). `cockpit` is written at most once a second.
- **Check numbers (D-160, `debrief_score.cpp` only):** beacon ≤ 10 s; seat belts 0 s off (both windows); taxi ≤ 30 kt;
  altimeters ≤ 10 tenths of hPa; both takeoff lights on; ≤ 10 s above 260 kt below 10,000 ft; STD ≤ 60 s; destination
  altimeter read at transition level + 60 s; landing lights 0 s off from `below_10000` + 60 s; stable: gear down,
  landing flaps, sink ≤ 1,000 fpm, VREF −5 to +20 kt, lined up; strobes off ≤ 180 s after taxi in starts.
- **Stars (D-158):** sink ≤ 240 ★5, ≤ 360 ★4, ≤ 600 ★3, ≤ 900 ★2, ≤ 1,200 ★1, else ★0; crosswind ≥ 15 kt +1 (max 5);
  −1 per bounce, bank > 5°, pitch outside 0–8° (737) / 0–7.5° (777), IAS outside VREF ±10; ≥ 1.7 G caps at ★2,
  ≥ 2.0 G at ★1; floor 0.
- **Score (D-160):** 100 − 6 per missed check − landing points (★5 0, ★4 4, ★3 8, ★2 14, ★1 20, ★0 30), floor 0. A
  not-checked check counts neither way; the stable approach scores as one check.
- **Bounce:** an airborne stretch of at least **0.5 s** within **15 s** after the first contact.
- **The review call (D-156, D-161):** `POST https://api.anthropic.com/v1/messages`, headers `x-api-key`,
  `anthropic-version: 2026-10-21`, `anthropic-beta: server-side-fallback-2026-07-01`, `content-type:
  application/json`; body `model: "claude-opus-5-5"`, `max_tokens: 16000`, `thinking: {"type": "adaptive"}`,
  `output_config: {"effort": "medium", "format": {"type": "json_schema", "schema": …}}`, `fallbacks: "default"`; no
  `temperature`. Transfer timeout **300 s**. One automatic try at settling; then only Retry.
- **Secrets:** `claude_api_key` joins `AppSettings::secrets()`; never in a log line, an error text or `errors.log`.
  Error texts from the API are its `error.message`, which quotes no key; network errors say only "Couldn't reach
  Claude".
- **Copy:** checks are labelled exactly "Beacon on before pushback", "Seat belt sign, taxi to 10,000 ft", "Taxi speed",
  "Origin altimeter", "Lights for takeoff", "250 kt below 10,000 ft", "STD by the transition altitude", "Seat belt
  sign for the descent", "Destination altimeter", "Landing lights below 10,000 ft", "Stable at 1,000 ft", "Strobes off
  after landing". Review states: "Writing the debrief…", "Debrief not written: <reason>" + Retry, "Add a Claude API
  key to write debriefs". The strip: "Average score 71 (last 10: 74 ▲)" and "Landings ★★★☆☆ 3.4 (last 10: 3.8)".
- **Version:** 0.11.0.
- **Happy path for config (D-099).** Care goes to the sim data, the API call and the money staying untouched.

## Review Focus

1. **A log without the new data** (a build without PMDG headers, a 777 with the broadcast off, SayIntentions not
   running, a flight under 10,000 ft): every affected check is *not checked* with its reason, never *missed*, and the
   score counts only what was checked. Tests: `DebriefMeasure.NoCockpitPmdgFieldsSkipsBeltsAndVref`,
   `DebriefMeasure.NoAtisSkipsAltimeters`, `DebriefMeasure.LowFlightSkipsTenThousandChecks` (Task 4).
2. **A go-around, then a landing:** the stable approach and the landing lights are judged on the approach that landed;
   no deduction for the go-around. Test: `DebriefMeasure.GoAroundJudgesTheLandingApproach` (Task 4).
3. **Every way the API call fails** (no key, 401, 429/529, timeout, `stop_reason` "refusal" or "max_tokens", a text
   block that isn't the schema's JSON): nothing is stored, the reason is shown, no key reaches the reason. Tests:
   `DebriefReview.ParsesEachFailure`, `ReviewWriter.FailureStoresNothingAndKeepsReason` (Tasks 7, 8).
4. **A diversion:** the destination altimeter is not checked; everything else is. Test:
   `DebriefMeasure.DiversionSkipsDestinationAltimeter` (Task 4).
5. **Restart after a failed or unfinished call:** the flight shows "Debrief not written" with Retry and nothing calls
   Claude on its own. Test: `ReviewWriter.NoAutomaticCallForOldFlights` (Task 8).

---

### Task 1: The `cockpit` event (recorder, D-159)

**Files:**
- Create: `src/cockpit_monitor.h`, `src/cockpit_monitor.cpp` (core), `tests/cockpit_monitor_test.cpp`
- Modify: `src/sim_vars.h` (three SimVars), `src/pmdg_probe.h` + `src/pmdg_probe_ng3.cpp` + `src/pmdg_probe_777.cpp` +
  `src/pmdg_probe_none.cpp` (cockpit decoders), `src/sim_probe.cpp/.h` (no longer registers PMDG; gets the data
  forwarded), `src/sim_connection.cpp/.h` (registers PMDG always, forwards client data), `src/recorder.cpp`,
  `CMakeLists.txt` (new sources, comments: the PMDG dirs now matter in normal running)

**Interfaces:**
- Produces (core, `cockpit_monitor.h`):
  ```cpp
  struct PmdgCockpit {            // one PMDG broadcast, decoded
      int seatBelts = 0;          // 0 off, 1 auto, 2 on
      bool landingLights = false; // 737: any fixed light 1 or retractable 2; 777: any LTS_LandingLights_Sw_ON
      int vrefKt = 0, landingFlaps = 0;  // FMC_LandingVREF, FMC_LandingFlaps; 0 = not set
      bool autopilot = false;     // 737: MCP_annunCMD_A || _B; 777: MCP_annunAP[0] || [1]
  };
  struct PlainCockpit { bool landingLight; bool seatBeltSign; double gearPct; bool paused; };
  class CockpitMonitor {
  public:
      void onPlain(const PlainCockpit& p, TimePoint now);
      void onPmdg(const PmdgCockpit& p, TimePoint now);
      std::optional<nlohmann::json> due(TimePoint now);  // the event's data when it changed and ≥ 1 s since the last
      void reset();                                       // new session
  };
  ```
- Produces (sim, `pmdg_probe.h`): `bool registerPmdg737(void*, unsigned long)`/`777` (unchanged names, now called by
  `SimConnection`), `std::optional<PmdgCockpit> decodeCockpit737(const void*, std::size_t)` / `777` (nullopt when too
  small or the header wasn't built in).
- The `cockpit` JSON: `landing_lights` (bool), `seat_belts` (0/1/2), `gear_down` (bool, gear % ≥ 99.5), `pmdg`
  (bool: PMDG data has arrived this session), and only with PMDG data: `vref_kt`, `landing_flaps`, `autopilot`. With PMDG data, `landing_lights` and `seat_belts` come from it;
  without, from the plain SimVars (`seat_belts` 0 or 2). Paused plain samples change nothing.

- [ ] **Step 1: Write the failing tests** in `tests/cockpit_monitor_test.cpp`:
  `CockpitMonitor.PlainOnlyHasNoPmdgFields` (keys exactly `landing_lights`, `seat_belts`, `gear_down`; a plain seat belt
  sign on gives 2), `CockpitMonitor.PmdgWinsForLightsAndBelts` (plain light off + PMDG light on → true; PMDG belts 1 →
  1; `vref_kt` 125 present), `CockpitMonitor.PausedPlainSampleIgnored` (gear 0 % while paused leaves `gear_down` true),
  `CockpitMonitor.AtMostOncePerSecond` (two changes 300 ms apart → one event at +1 s holding the second),
  `CockpitMonitor.NoEventWithoutChange`.
- [ ] **Step 2: Run** `ctest --preset x64-release -R CockpitMonitor`. Expected: build fails (no header).
- [ ] **Step 3: Implement `CockpitMonitor`** (core) and add the three SimVars to `TelemetrySample`/`kTelemetryVars`
  (layout guard must hold).
- [ ] **Step 4: Run** the tests. Expected: PASS.
- [ ] **Step 5: Wire the sim side.** `SimConnection::setup` calls `registerPmdg737/777` with its own request ids
  (`REQ_PMDG_737 = 7`, `REQ_PMDG_777 = 8`) and logs `pmdg_data` `{"737": bool, "777": bool}` once per session.
  `SIMCONNECT_RECV_ID_CLIENT_DATA` decodes with `decodeCockpit737/777` into a new `CockpitCallback`, and forwards the
  raw block to `SimProbe::onClientData` when the probe is on. `SimProbe::setup` drops its two register calls (its
  `probe_started` keeps `pmdg_737`/`pmdg_777` from what `SimConnection` reports). The recorder owns one
  `CockpitMonitor`: plain values from each valid telemetry sample (with `sim.paused()`), PMDG values from the callback,
  `due()` checked every loop; written as `cockpit` with `flight_id` while a flight is open, without it otherwise.
- [ ] **Step 6: Build release and debug** (with and without `BLOCKTIME_PMDG_*_SDK_DIR`). Expected: no warnings; all
  tests pass.
- [ ] **Step 7: Commit** "Log cockpit state on every flight, PMDG data included (D-159)".

### Task 2: Touchdown attitude and wind, and bounces

**Files:**
- Modify: `src/touchdown_capture.h/.cpp`, `src/sim_connection.cpp` (`TouchdownRecord`, `kTouchdownVars`),
  `src/flight_tracker.cpp`, `tests/touchdown_capture_test.cpp`, `tests/flight_tracker_test.cpp`

**Interfaces:**
- `FastSample` and `Touchdown` gain `double pitchDeg, bankDeg, headingTrueDeg, windDirDeg, windKt` (the first frame on
  the ground's). `TouchdownCapture` gains `std::optional<int> bouncesDue(const FastSample&)`: after the touchdown, it
  counts airborne stretches ≥ 0.5 s and returns the count once, at the first frame ≥ contact + 15 s.
- `FlightTracker` keeps fast sampling on after the touchdown until the bounce count is out (or taxi in, which emits
  it with what was counted). Events: `touchdown` gains `sim_pitch_deg` (raw, rounded to 0.1), `bank_deg`,
  `heading_true_deg`, `wind_dir_deg`, `wind_kt` (whole); new `bounces` `{flight_id, time, bounces}`.
- **The pitch sign is unverified:** `sim_pitch_deg` is logged raw; `debrief_measure` uses `pitch = -sim_pitch_deg`
  (the SDK documents nose up as negative) and the sim check confirms it.

- [ ] **Step 1: Write the failing tests:** `TouchdownCapture.CountsABounce` (ground 0 s, air 0.2–0.9 s, ground after →
  1 at +15 s), `TouchdownCapture.IgnoresAFlicker` (air for 0.3 s → 0), `TouchdownCapture.KeepsAttitudeAndWind`,
  `FlightTracker.BouncesEventAfterTouchdown`, `FlightTracker.TaxiInEmitsBouncesEarly`.
- [ ] **Step 2: Run** them. Expected: FAIL.
- [ ] **Step 3: Implement** in `touchdown_capture.cpp` and `flight_tracker.cpp`; extend the SimConnect record.
- [ ] **Step 4: Run** all tests. Expected: PASS (existing touchdown tests unchanged).
- [ ] **Step 5: Commit** "Touchdown attitude, wind and bounces for the landing rating (D-158)".

### Task 3: One flight's lines from the log, and the KLAS → KSAN fixture

**Files:**
- Create: `src/flight_events.h/.cpp` (core), `tests/flight_events_test.cpp`, `tests/data/flight-klas-ksan.jsonl`,
  `tests/data/README.md`, `tools/make_debrief_fixture.py`
- Modify: `src/event_log.h/.cpp` (`parseUtc`)

**Interfaces:**
- `std::optional<TimePoint> parseUtc(std::string_view iso)`: inverse of `formatUtc` (milliseconds optional).
- `std::vector<LogEvent> loadFlightEvents(const std::filesystem::path& logDir, std::string_view logFlightId,
  std::size_t maxFiles = kOpenFlightLookbackFiles)`: every line from 30 minutes before that flight's `block_out` line
  to its `block_in` line, in log order, across daily files; lines carrying another `flight_id` are dropped; empty if
  the flight isn't closed by a `block_in`.
- **The fixture** is the 2026-11-24 KLAS → KSAN leg (flight_id `2026-11-24T23:53:16.837Z`) in the 0.11.0 format,
  made once by the script from Dave's log: `cockpit` events derived from `probe_pmdg_737` (seat belts, lights, VREF,
  flaps, CMD A/B) and the gear lights; `touchdown` gains `bank_deg` 0, `heading_true_deg` from the telemetry, wind
  330/9; a `bounces` 0 event; probe lines, INTERCOM ATC entries and audio URLs removed. `tests/data/README.md` lists
  the hand-added fields.

- [ ] **Step 1: Make the fixture** with `python -I tools/make_debrief_fixture.py <logdir> tests/data/flight-klas-ksan.jsonl`.
  Expected: about 3,500 lines, no `probe_` types, no `INTERCOM`.
- [ ] **Step 2: Write the failing tests:** `FlightEvents.LoadsTheFixtureFlight` (first event within 30 min before
  block out, last is `block_in`), `FlightEvents.DropsOtherFlightsLines`, `FlightEvents.OpenFlightIsEmpty`,
  `EventLog.ParseUtcRoundTrips`.
- [ ] **Step 3: Run**, expect FAIL; **implement**; **run**, expect PASS.
- [ ] **Step 4: Commit** "Read one flight back from the log, with a real-flight fixture".

### Task 4: The measurements (`debrief_measure`)

**Files:**
- Create: `src/debrief_measure.h/.cpp` (core), `tests/debrief_measure_test.cpp`

**Interfaces:**
- Consumes: `loadFlightEvents` output.
- Produces:
  ```cpp
  enum class Family { B737, B777 };
  struct CheckValue { std::string part; int value; };       // part codes below
  struct CheckSkip { std::string part; std::string reason; };
  struct LandingFacts {
      int vsFpm, gHundredths, bounces, iasKt, headingTrueDeg, windDirDeg, windKt;
      std::optional<int> pitchTenths, bankTenths, vrefKt, offCentrelineDm, offHeadingTenths;
  };
  struct FlightMeasurements {
      Family family;
      std::vector<CheckValue> values;
      std::vector<CheckSkip> skipped;   // every part is in exactly one of the two
      std::optional<LandingFacts> landing;  // nullopt: no touchdown event
  };
  FlightMeasurements measureFlight(const std::vector<LogEvent>& events, bool diverted);
  inline constexpr std::array kCheckParts = {
      "beacon_late_s", "belts_off_departure_s", "taxi_max_kt", "origin_baro_off_tenths", "takeoff_strobes",
      "takeoff_landing_lights", "over_250_s", "std_late_s", "belts_off_descent_s", "destination_baro_off_tenths",
      "landing_lights_off_s", "stable_gear_down", "stable_landing_flaps", "stable_sink_fpm", "stable_vref_offset_kt",
      "stable_lined_up", "strobes_on_after_landing_s"};
  // skip reasons: "no_pmdg_data", "no_vref", "no_atis", "no_marker", "diverted"
  ```
- **What each part measures** (state at a time = the latest line at or before it; "seconds off" sums the time a value
  was false/0 inside the window; a value never reached counts to block in):
  - `beacon_late_s`: deadline = the earlier of block out and the first `eng_running` true; seconds until telemetry
    `beacon` true, 0 if already on.
  - `belts_off_departure_s`: `seat_belts` = 0 from `taxi_out` start to the `above_10000` marker (or, without one, the
    `descent` start). `belts_off_descent_s`: from the `transition_level` marker to block in (`no_marker` without one).
  - `taxi_max_kt`: highest `gs_kt` on the ground during `taxi_out` (to `takeoff`) and `taxi_in` (to block in).
  - `origin_baro_off_tenths`: at `takeoff` start, the larger of the captain's and FO's distance from the origin ATIS
    in force then, or the one before it, whichever is closer; ATIS `altimeter` "30.22" is inHg × 33.8639, above 800 it
    is hPa. `destination_baro_off_tenths`: the same at `transition_level` + 60 s against destination ATIS (`diverted`
    when diverted, `no_marker`, `no_atis`).
  - `takeoff_strobes` / `takeoff_landing_lights`: 1 if on at `takeoff` start.
  - `over_250_s`: seconds airborne below 10,000 ft indicated with `ias_kt` > 260.
  - `std_late_s`: seconds from the `transition_altitude` marker until both altimeters are within 0.05 of 1013.2.
  - `landing_lights_off_s`: `landing_lights` off from the last `below_10000` + 60 s to the touchdown.
  - `stable_*`: at the last `final_approach` marker: `gear_down`; `flaps_handle`'s detent in degrees (737: 0, 1, 2, 5,
    10, 15, 25, 30, 40; 777: 0, 1, 5, 15, 20, 25, 30) equals `landing_flaps`; `-vs_fpm`; `ias_kt - vref_kt`; a
    `lined_up` after the last `go_around` phase start (or block out) and before the marker. Flaps and VREF parts skip
    with `no_vref` when `vref_kt` or `landing_flaps` is 0 or absent.
  - `strobes_on_after_landing_s`: seconds from `taxi_in` start until `strobe` false.
  - On a 737, the seat belt parts skip with `no_pmdg_data` when the flight's `cockpit` events have `pmdg` false (the
    plain SimVar never moves there); on a 777 the plain sign counts. Without `pmdg`, the flaps and VREF parts skip
    with `no_pmdg_data` rather than `no_vref`.
  - Family: `block_out` `aircraft_folder` contains "777" → B777, else B737.
  - Landing: the last `touchdown`, `bounces`, `landing_runway`'s offsets, `vref_kt` at touchdown; pitch = −`sim_pitch_deg`.

- [ ] **Step 1: Write the failing tests.** `DebriefMeasure.KlasKsanFixture` asserts the real flight:
  `beacon_late_s` 163 (± 1), `belts_off_departure_s` > 700, `taxi_max_kt` 25, `origin_baro_off_tenths` ≤ 3,
  `takeoff_strobes` 1, `takeoff_landing_lights` 1, `over_250_s` 0, `std_late_s` 111 (± 1), `belts_off_descent_s` 0,
  `destination_baro_off_tenths` 0, `landing_lights_off_s` > 360, `stable_gear_down` 1, `stable_landing_flaps` 1,
  `stable_sink_fpm` 791 (± 1), `stable_vref_offset_kt` 0 (± 1), `stable_lined_up` 1, `strobes_on_after_landing_s` ≤ 60;
  landing 591 fpm, 147, 0 bounces, IAS 125, VREF 125. Plus the Review Focus tests (synthetic logs built in the test):
  `NoCockpitPmdgFieldsSkipsBeltsAndVref`, `NoAtisSkipsAltimeters`, `LowFlightSkipsTenThousandChecks`,
  `GoAroundJudgesTheLandingApproach`, `DiversionSkipsDestinationAltimeter`, and `EveryPartOnce` (values ∪ skipped =
  `kCheckParts`, disjoint) on each.
- [ ] **Step 2: Run**, expect FAIL. **Step 3: Implement.** **Step 4: Run**, expect PASS.
- [ ] **Step 5: Commit** "Measure the procedure checks and the landing from the log (D-157, D-160)".

### Task 5: Outcomes, stars and the score (`debrief_score`)

**Files:**
- Create: `src/debrief_score.h/.cpp` (core), `tests/debrief_score_test.cpp`

**Interfaces:**
- Consumes: `FlightMeasurements`.
- Produces:
  ```cpp
  enum class CheckState { Passed, Missed, NotChecked };
  struct CheckOutcome { std::string label, detail; CheckState state; int points; };  // points: 0 or -6
  struct ScoredFlight {
      std::vector<CheckOutcome> checks;   // the twelve, in the spec's order
      std::optional<int> stars;           // nullopt without a landing
      int landingPoints;                  // 0..-30
      std::optional<int> crosswindKt;
      int score;
  };
  ScoredFlight scoreFlight(const FlightMeasurements& m);
  struct Averages { std::optional<double> score, scoreLast10, stars, starsLast10; };
  Averages averages(const std::vector<ScoredFlight>& newestFirst);
  std::string starsText(int stars);  // "★★★☆☆"
  ```
- Details: "163 s after block out", "off 13 min", "25 kt", "0.3 hPa off", "strobes and landing lights on", "1 min 51 s
  late", "gear, flaps 40, 791 fpm, VREF +0, lined up"; a not-checked detail is the reason in words ("No PMDG data",
  "VREF not entered", "No ATIS logged", "Not reached", "Diverted"). Crosswind = |wind_kt × sin(wind_dir − heading)|,
  rounded.

- [ ] **Step 1: Write the failing tests:** `DebriefScore.KlasKsanScores68` (the fixture's measurements → checks 1, 2,
  7, 10 missed, ★3, score 68), `DebriefScore.StarBands` (240 → 5, 241 → 4, 600 → 3, 1201 → 0),
  `DebriefScore.CrosswindGivesAStarBack` (700 fpm, 16 kt cross → 3), `DebriefScore.GCaps` (200 fpm 1.8 G → 2;
  2.0 G → 1), `DebriefScore.DeductionsFloorAtZero`, `DebriefScore.NotCheckedCountsNeitherWay`,
  `DebriefScore.StableIsOneCheck` (two parts missed → −6), `DebriefScore.ScoreFloorsAtZero`,
  `DebriefScore.AveragesLastTen`.
- [ ] **Step 2–4:** run (FAIL), implement, run (PASS).
- [ ] **Step 5: Commit** "Score the flight and rate the landing from its measurements (D-158, D-160)".

### Task 6: Migration 7, written at settling, read back

**Files:**
- Modify: `src/airline_db.cpp` (`kMigration7`, `kAirlineSchemaVersion` 7), `src/flight_settlement.h/.cpp`,
  `src/airline_queries.h/.cpp`, `docs/database.md` (written first, with the diagram), `tests/airline_db_test.cpp`,
  `tests/flight_settlement_test.cpp`

**Interfaces:**
- Tables (all `STRICT`, `flight_id INTEGER NOT NULL REFERENCES flight ON DELETE CASCADE`, written once):
  - `check_part (code TEXT PRIMARY KEY)` seeded with `kCheckParts`; `check_skip_reason (code TEXT PRIMARY KEY)`
    seeded with the five reasons;
  - `flight_check_value (flight_id, part REFERENCES check_part, value INTEGER NOT NULL, PRIMARY KEY (flight_id, part))`;
  - `flight_check_skip (flight_id, part REFERENCES check_part, reason REFERENCES check_skip_reason, PRIMARY KEY
    (flight_id, part))`;
  - `flight_debrief (flight_id PRIMARY KEY, family TEXT NOT NULL CHECK (family IN ('737','777')))`: one row marks
    "this flight was measured"; the value, skip and landing rows reference it;
  - `flight_landing (flight_id PRIMARY KEY REFERENCES flight_debrief, vs_fpm, g_hundredths, bounces, ias_kt,
    heading_true_deg, wind_dir_deg, wind_kt` NOT NULL; `pitch_tenths, bank_tenths, vref_kt, off_centreline_dm,
    off_heading_tenths` NULL meaning "not read" only, D-090);
  - `flight_review (flight_id PRIMARY KEY REFERENCES flight_debrief, summary, landing, atc, model TEXT NOT NULL,
    written_at TEXT NOT NULL instant)`, `flight_review_item (flight_id REFERENCES flight_review, seq, text, PRIMARY
    KEY (flight_id, seq))`.
  - Indexes: the composite primary keys lead with `flight_id`; add indexes on `part` and `reason` columns.
- `FlownFlight` gains `std::optional<FlightMeasurements> measurements`; `settleIfFlown` fills it with
  `measureFlight(loadFlightEvents(logDir, id), diverted)` (diverted from the arrival it already computes) and
  `settleFlight` writes it in the same transaction.
- `std::map<std::string, FlightMeasurements> loadMeasurements(db::Database&)` (by `log_flight_id`);
  `struct StoredReview { std::string summary, landing, atc, model, writtenAt; std::vector<std::string> workOn; };`
  `std::map<std::string, StoredReview> loadReviews(db::Database&)`;
  `bool storeReview(db::Database&, std::string_view logFlightId, const StoredReview&, std::string* error)` (one
  transaction; fails if one exists).

- [ ] **Step 1: Write `docs/database.md`'s "Milestone 10's tables (schema version 7)"** section and diagram.
- [ ] **Step 2: Write the failing tests:** `AirlineDb.Migration7BuildsFromEmpty` (and `EXPLAIN QUERY PLAN` of
  `loadMeasurements` uses the indexes), `FlightSettlement.WritesMeasurementsInTheTransaction` (fixture log → rows
  equal `measureFlight`), `FlightSettlement.FailedSettleWritesNoMeasurements`, `AirlineQueries.ReviewStoredOnce`
  (second `storeReview` fails, first kept).
- [ ] **Step 3–4:** run (FAIL), implement, run (PASS).
- [ ] **Step 5: Commit** "Store each flight's measurements at settling (migration 7, D-160)".

### Task 7: Claude's request and answer (`debrief_review`)

**Files:**
- Create: `src/debrief_review.h/.cpp` (core), `tests/debrief_review_test.cpp`, `tests/data/review_*.json` (answers)

**Interfaces:**
- Consumes: `loadFlightEvents`, `FlightMeasurements`, `ScoredFlight`, `StoredReview`.
- Produces:
  ```cpp
  struct ReviewInput {
      std::string header;                        // "KLAS → KSAN · SOX8 · N814SX (PMDG 737-800BBJ2) · out 23:53Z in 00:52Z"
      std::vector<LogEvent> events;
      FlightMeasurements measurements;
      std::vector<std::vector<std::string>> recentMisses;  // last five flights' missed check labels, newest first
  };
  std::string buildTimeline(const std::vector<LogEvent>& events);  // one line per event, "00:49:50 touchdown 591 fpm…"
  std::string buildReviewRequest(const ReviewInput& in);           // the JSON body (Global Constraints)
  struct ReviewAnswer { std::optional<StoredReview> review; std::string error; };
  ReviewAnswer parseReviewResponse(int httpStatus, std::string_view body);  // model id from the body's "model"
  ```
- The timeline drops `telemetry` except derived lines (the state at the `final_approach` marker, the sink from it to
  touchdown, each `autopilot` change); ATC lines only from `COM` channels, labelled "Pilot:" (`incoming_message`) and
  "<station_name>:" (`outgoing_message`), each once by `id`; ATIS `text` once per letter.
- The user message holds, in order: the header, the checks with their details and states, the landing with stars,
  the timeline, the ATC log, the ATIS, the recent misses. The system prompt (a constant in the .cpp) asks for what
  went well, what to work on (most important first), the landing explained, and ATC compliance as information (say
  when an instruction looks wrong or late instead of calling a bust), no score numbers, about 300 words, plain text.
- Schema: object `{summary: string, work_on: array of string, landing: string, atc: string}`, all required,
  `additionalProperties: false`.
- Errors: HTTP ≥ 400 → "Claude: <error.message>" (or "Claude answered HTTP <n>"); `stop_reason` "refusal" → "Claude
  declined to write it"; "max_tokens" → "Claude's answer was cut off"; no text block or bad JSON → "Claude's answer
  wasn't readable".

- [ ] **Step 1: Write the failing tests:** `DebriefReview.TimelineOfTheFixture` (contains "touchdown", "autopilot
  off", no "Cabin Crew", no `telemetry` lines, the 5,300 ft exchange labelled "SoCal Approach:"),
  `DebriefReview.RequestBody` (parses as JSON; model, max_tokens 16000, effort "medium", fallbacks "default", schema
  keys, no "temperature"), `DebriefReview.ParsesAnAnswer` (thinking block then text block → four fields, model id),
  `DebriefReview.ParsesEachFailure` (401 body, 529 body, refusal, max_tokens, non-JSON text).
- [ ] **Step 2–4:** run (FAIL), implement, run (PASS).
- [ ] **Step 5: Commit** "Build Claude's debrief request and read its answer (D-161)".

### Task 8: The call, once after settling, and Retry

**Files:**
- Create: `src/claude_reviewer.h/.cpp` (Qt), `src/review_writer.h/.cpp` (Qt), tests in `tests/qt_models_test.cpp`
- Modify: `src/app_settings.h/.cpp` (`claudeApiKey`, `"claude_api_key"`, in `secrets()`), `tests/app_settings_test.cpp`,
  `src/app_main.cpp`, `CMakeLists.txt`

**Interfaces:**
- `class ReviewClient { virtual void write(std::string body, std::function<void(int status, QByteArray body, QString
  networkError)> done) = 0; virtual bool hasKey() const = 0; }`; `ClaudeReviewer : QObject, ReviewClient` posts with
  the Global Constraints headers and timeout.
- `class ReviewWriter : QObject` (db, logDir, ReviewClient&):
  - `void onSettled(const QString& logFlightId)`: the one automatic try;
  - `Q_INVOKABLE void retry(const QString& logFlightId)`;
  - `QString state(const QString& logFlightId) const`: "written", "writing", "failed", "no_key", "missing" (measured,
    no review, nothing tried this session), "none" (not measured);
  - `QString error(const QString&) const` (this session only);
  - `signal reviewChanged(QString logFlightId)`.
  It builds `ReviewInput` (events from the log, measurements and the last five flights' misses from the database),
  calls `buildReviewRequest`, `parseReviewResponse`, then `storeReview`.
- `app_main.cpp`: `PlannedFlightModel::settled` → `reviewWriter.onSettled(id)`; `reviewChanged` → `flights.reload()`.

- [ ] **Step 1: Write the failing tests** with a fake `ReviewClient`: `ReviewWriter.WritesOnceAfterSettling` (one call;
  review stored; state "written"), `ReviewWriter.FailureStoresNothingAndKeepsReason` (529 → state "failed", error
  "Claude: Overloaded", no row, the key string absent from the error), `ReviewWriter.RetryCallsAgain`,
  `ReviewWriter.NoAutomaticCallForOldFlights` (a measured flight with no review at construction → "missing", zero
  calls), `ReviewWriter.NoKeyMakesNoCall`, `AppSettings.ReadsClaudeKeyAsSecret`.
- [ ] **Step 2–4:** run (FAIL), implement, run (PASS).
- [ ] **Step 5: Commit** "Write the review once after settling, with Retry (D-161)".

### Task 9: The Flight Summary's debrief and the Flights strip

**Files:**
- Create: `qml/DebriefPanel.qml`, `qml/CheckList.qml`
- Modify: `src/flight_list_model.h/.cpp`, `qml/FlightSummary.qml`, `qml/FlightsPage.qml`, `qml/LivePage.qml`,
  `qml/Theme.qml`, `CMakeLists.txt` (QML files), `tests/qt_models_test.cpp`

**Interfaces:**
- `FlightListModel` takes `ReviewWriter&`; `summary(row)` adds `debrief`: `{ measured: bool, scoreText: "68",
  starsText: "★★★☆☆", checks: [{label, detail, state: "passed"|"missed"|"not_checked", pointsText: "−6"|""}],
  landingLines: ["591 fpm · 1.47 G · no bounce · pitch 5.1° · bank 0.8° · VREF +0 · wind 330/9 (8 kt cross)", "2.7 m
  off the centreline, 0.6° off the heading (information)"], landingPointsText: "−8", reviewState, reviewError,
  review: {summary, workOn: [...], landing, atc} }`. New properties `averageScoreText`, `averageStarsText`
  (the strip's two strings, "" with no measured flight), refreshed on reload.
- `DebriefPanel` (in FlightSummary, between the stat tiles and the money/map row; scrollable within
  `Theme.summaryHeight`): the score and stars line, the review (or its state with Retry calling
  `reviewWriter.retry`), the checks, the landing. Flights without `measured` show no panel.

- [ ] **Step 1: Write the failing tests:** `FlightListModel.SummaryHasDebrief` (fixture-measured flight → scoreText
  "68", 12 checks, 4 missed), `FlightListModel.AverageStripText` (three flights → "Average score 71 (last 10: 71)"
  form), `FlightListModel.OldFlightHasNoDebrief`.
- [ ] **Step 2–4:** run (FAIL), implement, run (PASS).
- [ ] **Step 5: Look at it:** debug build, `--demo`, hot reload; Flights with a measured flight shows the panel
  without clipping at 150 % scaling; `blocktime-app_qmllint` reports nothing.
- [ ] **Step 6: Commit** "Show the debrief in the Flight Summary and the averages on Flights".

### Task 10: The replay tool

**Files:**
- Create: `src/debrief_main.cpp`
- Modify: `CMakeLists.txt` (`blocktime-debrief`: Qt6::Core + Qt6::Network console app, linked with blocktime-core and
  `claude_reviewer.cpp`; deployed with the app), `README.md`

**Interfaces:**
- `blocktime-debrief --log-dir <dir> --flight <flight_id> [--diverted] [--prompt] [--claude]`: prints each check
  ("✗ Beacon on before pushback  163 s after block out  −6"), the landing and stars, the score; `--prompt` prints
  `buildReviewRequest`'s body (no recent misses: the tool reads no database); `--claude` sends it with
  `claude_api_key` from settings.json and prints the four parts or the error. Exit 1 when the flight isn't found.

- [ ] **Step 1: Implement**, then **run** it on the fixture's folder:
  `build\x64-release\blocktime-debrief.exe --log-dir tests\data --flight 2026-11-24T23:53:16.837Z`. Expected: the
  four misses and "Score 68 · Landing ★★★☆☆".
- [ ] **Step 2: Run `--prompt`** and check the request by eye; then one `--claude` call (about $0.15, only with Dave's
  key in settings.json and his OK in chat) to tune the instructions. Record the token counts in
  `docs/notes/claude-debrief.md`.
- [ ] **Step 3: Commit** "blocktime-debrief: replay a flight's checks and its review request".

### Task 11: Docs, version and the sim checks

**Files:**
- Modify: `CMakeLists.txt` (0.11.0), `README.md` (the key, the tool, the PMDG dirs now needed for full checks),
  `docs/design.md` §13.7 and §14, `docs/recorder.md` (Data to capture, procedure capture built), `docs/roadmap.md`
  (milestone 10 built; the MVP done), `docs/debrief.md` (built; anything that changed), `docs/notes/sayintentions-api.md`
  (incoming/outgoing, `copilot`, INTERCOM), `docs/README.md`, `docs/progress.md`

- [ ] **Step 1: Update the docs** listed above.
- [ ] **Step 2: Put the sim steps in the sim checks form** (run `m10-debrief`), ordered to need one 737 flight and one
  777 flight: `cockpit` lines follow the landing lights (fixed and retractable), seat belts, gear (and while paused),
  autopilot; VREF appears after entering it; a deliberate small bounce is counted; `sim_pitch_deg` is negative with the
  nose up at touchdown; the summary shows the debrief within a couple of minutes; Retry works with the network off,
  then on.
- [ ] **Step 3: Verification** (superpowers:verification-before-completion): clean release and debug builds, all
  tests, qmllint.
- [ ] **Step 4: Code review** (requesting-code-review, then receiving-code-review), fixes with tests.
- [ ] **Step 5: Commit and push `main`** (D-121) "Milestone 10: procedure checks, landing stars and Claude's debrief
  (0.11.0)".
