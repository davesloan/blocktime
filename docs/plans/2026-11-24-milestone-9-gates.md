# Milestone 9 (Gates) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BlockTime picks the arrival gate and asks SayIntentions for it once SayIntentions has chosen its own. Each
flight records where it parked, and Live shows the load-in gate for the next one. Live gets three columns and a
leaner flight panel. Approach starts at the first flaps, and "Final approach" shows when lined up with a runway.

**Architecture:**
- **Recorder (plain C++, tested):**
  - the facility request reads parking spots as well as runways, and the observer passes each airport to the app;
  - block events carry the spot they happened at;
  - telemetry gains Mach and two flap values; `PhaseDetector` starts Approach at the first flaps (D-142);
  - a new `LinedUpDetector` logs `lined_up` with the runway (D-143, D-145).
- **Gate logic (plain C++, tested):**
  - `pickGate` applies D-074 / D-147 to an airport's spots;
  - `GateAssigner` decides, from SayIntentions' flightJSON, whether to send `assignGate` and what to log.
  - `FlightWatcher` (Qt) runs it during the flight, through `Planner`. Its `arrival_gate` event reaches the log
    through `AppEventQueue` (D-127).
- **Database:** migration 6 adds `flight.origin_parking` and `flight.arrival_parking`.
- **Live:**
  - `FlightStages` gains the gate, lined-up and parked-at lines;
  - the planned flight header shows the load-in gate;
  - the row below it becomes three columns (D-139); the flight panel changes per D-140.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, gtest,
yaml), Qt 6 Quick, SimConnect (MSFS 2020 SDK).

**Spec:**
- `docs/roadmap.md`, milestone 9.
- `docs/design.md`, §13.3 (where to load in), §13.5 (stages, three columns), §13.6 (arrival gate).
- `docs/recorder.md`, the phase table (Approach).
- `docs/notes/sayintentions-api.md` (flightJSON, `assignGate`, `getParking`), `docs/notes/probe-flight.md` (parking
  spots, runways).
- Decisions: D-074, D-122, D-126, D-127, D-139..D-143, **D-145..D-149** (written with this plan).

## Global Constraints

- **Build and test:**
  - no warnings at `/W4 /permissive- /utf-8`, in release and debug;
  - `cmake --build --preset x64-release`, then `ctest --preset x64-release`, all pass;
  - `cmake --build --preset x64-debug --target blocktime-app_qmllint` reports nothing.
- **Files:**
  - every new file starts with the GPL header;
  - QML files `import BlockTime`, use `pragma ComponentBehavior: Bound`, and take every colour, size and font from
    `Theme.qml`;
  - `blocktime-core` and its tests include no Qt, `windows.h` or `SimConnect.h`.
- **Recorder budget:** telemetry gains only these three, all `PERIOD_SECOND` + `CHANGED` like the rest:
  - `AIRSPEED MACH` (`"mach"`, Float64, epsilon 0.002);
  - `TRAILING EDGE FLAPS LEFT PERCENT` (`"percent"`, Float64, epsilon 0.5);
  - `FLAPS HANDLE INDEX` (`"number"`, Int32, epsilon 0).
- **Numbers:**
  - parked spot within **40 m** (D-122);
  - a spot fits when `2 × radius ≥ wingspan`;
  - flaps out above **1 %**;
  - lined up: **10 nm** of the threshold, **10°** of the runway heading, **0.5 nm** of the centreline, held
    **10 s** (D-143), with the track taken from positions at least **150 m** apart;
  - Mach from **0.70**, back to knots below **0.66** (D-140);
  - flightJSON polled every **30 s** from block out until the gate is settled or block in.
- **Never log SayIntentions' flightJSON as a whole:** it holds the pilot's `api_key` and email. Pick fields only.
  No key in any URL that gets logged.
- **Copy:**
  - spot names "Gate E7", "Gate 46", "Parking 70", "N Parking 3", "Dock 2"; short names "E7", "46", "70";
  - Descent lines "Final approach, runway 26L" from `lined_up` until the 1,000 ft `final_approach` marker, then
    "Short final" until touchdown (D-149); a go-around closes both;
  - Arrival "Parked at Gate E7" (plain "Parked" when no spot is known);
  - the gate line, one per flight, added to the current stage:
    - `assigned`: "Arrival gate: Gate E7";
    - `same_as_si`: "Arrival gate: Gate E7 (SayIntentions' choice)";
    - `gate_fallback` reason adds " (no ramp fits)";
    - `unknown_gate`: "Arrival gate: SayIntentions' Terminal 3 Gate E7A (it doesn't know E7)";
    - `error`: "Arrival gate: SayIntentions' Terminal 3 Gate E7A (assignGate failed)";
    - `none_fits`: "Arrival gate: SayIntentions' Terminal 3 Gate E7A (no spot fits)";
  - the planned flight header: "Load in at KLAS, Gate E7" (just "Load in at KLAS" without a spot).
- **Version:** 0.10.0.
- **Happy path for config (D-099):** care goes to the sim, the network calls and the money.

## Review Focus

1. **SayIntentions not running, or no SayIntentions flight:** flightJSON fails or has no `assigned_gate`. Expected:
   no gate line, nothing in `errors.log`, polling stays quiet until block in. Task 5 (`NoSayIntentionsNoGate`).
2. **SayIntentions flies a different destination** (Dave filed something else there): its gate is for another
   airport. Expected: ignored, nothing sent. Task 5 (`OtherDestinationIgnored`).
3. **A go-around, then an approach to another runway:** expected: a second "Final approach" line naming the new
   runway. Task 3 (`FiresAgainAfterGoAround`).
4. **A diversion:** block in at an airport whose spots weren't loaded. Expected: plain "Parked", `arrival_parking`
   NULL, settling unaffected. Task 6 (`NoParkingStoredAsNull`).
5. **Flaps out above 10,000 ft** (an early configuration in a hold): expected: no Approach until below 10,000 ft;
   the 3,000 ft AGL fallback still works with no flaps seen. Task 2 (`FlapsAbove10000NoApproach`,
   `ApproachFallbackBelow3000Agl`).

---

### Task 1: Parking spots from the sim, and the parked spot in block events

**Files:**
- Create: `src/parking.h`, `src/parking.cpp` (in `blocktime-core`).
- Modify: `src/runway_match.h` (`AirportRunways` stays for `nameRunway`).
- Modify: `src/sim_connection.h/.cpp`:
  - `requestRunways` becomes `requestAirports`, which also reads the airport's LAT/LON and its `TAXI_PARKING`
    children (TYPE, NAME, SUFFIX, NUMBER, HEADING, RADIUS, BIAS_X, BIAS_Z, as `sim_probe.cpp` asks);
  - `RunwaysCallback` becomes `AirportCallback(AirportFacilities)`.
- Modify: `src/flight_tracker.h/.cpp`: `setRunways` becomes `setAirport(AirportFacilities)`; add `parkingAt`.
- Modify: `src/block_detector.h`: `BlockOut` and `BlockIn` gain `std::string parking`.
- Modify: `src/recorder.h/.cpp`:
  - fill `parking` before logging a block event, and log it as `"parking"`;
  - add the observer method;
  - pass airports to the tracker and the observer.
- Modify: `src/recorder_bridge.h/.cpp`: queue each airport to the GUI thread; keep the latest per ICAO.
- Modify: `CMakeLists.txt`.
- Test: `tests/parking_test.cpp`, `tests/flight_tracker_test.cpp`.

**Interfaces:**
- Produces:
  ```cpp
  struct ParkingSpot {
      int name = 0, number = 0, suffix = 0, type = 0;
      double lat = 0, lon = 0, headingDeg = 0, radiusM = 0;
  };
  struct AirportFacilities {
      std::string icao;
      double lat = 0, lon = 0;  // reference point
      std::vector<Runway> runways;
      std::vector<ParkingSpot> spots;
  };
  inline constexpr double kParkedSpotMaxM = 40.0;
  std::string spotName(const ParkingSpot& s);       // "Gate E7", "Gate 46", "Parking 70", "N Parking 3", "Dock 2"
  std::string spotShortName(const ParkingSpot& s);  // "E7", "46", "70"
  // From the facility record's offsets: bias_x metres east, bias_z metres north of the reference point.
  void placeSpot(ParkingSpot& s, double airportLat, double airportLon, double biasXM, double biasZM);
  std::optional<ParkingSpot> nearestSpot(const std::vector<ParkingSpot>& spots, double lat, double lon,
                                         double maxM = kParkedSpotMaxM);
  // FlightTracker
  void setAirport(AirportFacilities airport);
  std::string parkingAt(double lat, double lon) const;  // spotName of the nearest spot in any loaded airport, or ""
  // RecorderObserver
  virtual void onAirport(const AirportFacilities& /*airport*/) {}
  // RecorderBridge: const blocktime::AirportFacilities* airport(const QString& icao) const;
  //                 signal airportLoaded(QString icao)
  ```
- Name codes and placement are in D-147. `placeSpot` uses the flat offset `lat += z / 111320`,
  `lon += x / (111320 × cos lat)`: it put SayIntentions' KLAS E7A 16.6 m from MSFS's Gate E7.

- [ ] **Step 1: Write the failing tests**
  - `Parking.NamesGateWithLetter`: name 16, number 7 → `spotName` "Gate E7", `spotShortName` "E7".
  - `Parking.NamesPlainGate`: name 10, number 46 → "Gate 46" / "46".
  - `Parking.NamesRamp`: name 1, number 70 → "Parking 70" / "70"; name 2, number 3 → "N Parking 3" / "3".
  - `Parking.PlaceSpotOffsets`: reference 36.0, -115.0, bias 0 / 1113.2 → lat 36.0 + 0.01 (±1e-6), lon unchanged.
  - `Parking.NearestWithin40m`: spots 5 m and 30 m away → the 5 m one.
  - `Parking.NoneBeyond40m`: nearest 41 m away → `nullopt`.
  - `FlightTracker.ParkingAtNamesNearestSpot`: `setAirport` with KSFO and one spot (name 15, number 14) → `parkingAt`
    5 m from it is "Gate D14"; 100 m away it is "".
- [ ] **Step 2: Run them to make sure they fail**
  Run: `ctest --preset x64-release -R "Parking|FlightTracker.ParkingAt"`. Expected: build failure (no `parking.h`).
- [ ] **Step 3: Implement `parking.h/.cpp`, then the tracker, recorder, SimConnection and bridge changes**
  - The block event's `parking` is filled from `tracker.parkingAt(lat, lon)` before `logBlockEvent`.
  - `askRunways` becomes `askAirports`, same trigger (a new target).
  - `--probe`'s own facility request stays as it is.
- [ ] **Step 4: Run the tests and make sure they pass**
  Run: `cmake --build --preset x64-release` then `ctest --preset x64-release`. Expected: all pass, no warnings.
- [ ] **Step 5: Commit** — "Recorder reads parking spots and names the spot at each block event (D-147, D-148)".

### Task 2: Mach and flaps in telemetry; Approach at the first flaps (D-142)

**Files:**
- Modify: `src/sim_vars.h` (three fields before `title`, matching specs), telemetry JSON logging, `src/recorder.cpp`
  (`PhaseSample.flapsOut`).
- Modify: `src/phase_detector.h/.cpp`: remove `kApproachRangeNm` and its rule; add `kFlapsOutPct = 1.0`.
- Modify: `docs/recorder.md` (the phase table row and §7).
- Test: `tests/phase_detector_test.cpp`; update any replay expectations that relied on the 30 nm rule (the replays
  have no flaps, so they take the 3,000 ft AGL fallback).

**Interfaces:**
- Produces:
  - `TelemetrySample` gains `double mach`, `double flapsTrailingPct` and `std::int32_t flapsHandleIndex`;
  - `PhaseSample` gains `bool flapsOut` (`flapsTrailingPct > kFlapsOutPct`);
  - the logged telemetry gains `mach`, `flaps_te_pct` and `flaps_handle`.
- The rule: Cruise or Descent → Approach when `(flapsOut && altFt < kApproachBelowFt) || aglFt < kApproachAglFt`.

- [ ] **Step 1: Write the failing tests**
  - `PhaseDetector.ApproachAtFirstFlapsBelow10000`: in Descent at 9,000 ft with no flaps, still Descent; the next
    sample has flaps → Approach at that sample's time.
  - `PhaseDetector.FlapsAbove10000NoApproach`: flaps out at 11,000 ft → Descent; crossing 9,900 ft with flaps →
    Approach.
  - `PhaseDetector.ApproachFallbackBelow3000Agl`: no flaps, AGL 2,900 → Approach.
  - `PhaseDetector.ThirtyNmNoLongerStartsApproach`: 25 nm out, 9,000 ft, no flaps → still Descent.
  - `PhaseDetector.CruiseStraightToApproachWithFlaps`: a short flight in Cruise at 8,000 ft, flaps out → Approach.
- [ ] **Step 2: Run them to make sure they fail**
  Run: `ctest --preset x64-release -R PhaseDetector`. Expected: build failure (`flapsOut`).
- [ ] **Step 3: Implement the fields and the rule; fix the existing tests that used the 30 nm rule**
- [ ] **Step 4: Run all tests.** Expected: all pass.
- [ ] **Step 5: Commit** — "Approach starts at the first flap extension below 10,000 ft (D-142, D-148)".

### Task 3: `LinedUpDetector` and the `lined_up` event (D-143, D-145)

**Files:**
- Create: `src/lined_up.h`, `src/lined_up.cpp`.
- Modify: `src/flight_tracker.cpp`:
  - at block out, build one from the destination's runways if they're loaded;
  - feed it each sample with the phase;
  - log `lined_up` and tell the observer.
- Modify: `CMakeLists.txt`, `docs/recorder.md` (the event).
- Test: `tests/lined_up_test.cpp`.

**Interfaces:**
- Consumes: `Runway`, `runwayEndName` (runway_match.h), `PhaseSample`, `Phase`.
- Produces:
  ```cpp
  inline constexpr double kLinedUpRangeNm = 10.0, kLinedUpOffHeadingDeg = 10.0, kLinedUpOffCentrelineNm = 0.5,
                          kLinedUpHoldS = 10.0, kTrackBaseM = 150.0;
  struct LinedUp { std::string runway; TimePoint time; int altFt = 0; double distanceNm = 0; };
  class LinedUpDetector {
  public:
      explicit LinedUpDetector(std::vector<Runway> destinationRunways);
      // Only airborne in Descent or Approach. GoAround re-arms it. `time` is when the hold began.
      std::optional<LinedUp> onSample(const PhaseSample& s, Phase phase);
  };
  ```
  The log event is `lined_up`: `{"time", "runway": "26L", "alt_ft", "distance_nm"}` (one decimal).
- For each runway end: the landing heading `h` (primary = `headingDeg`, secondary = `h + 180`), and the threshold
  at the centre minus half the length along `h`. The aircraft is lined up with that end when:
  - it is before the threshold, within 10 nm along the approach;
  - it is within 0.5 nm of the extended centreline;
  - its track is within 10° of `h`.
  Of the ends that qualify, the one with the smallest lateral offset wins. The hold restarts if the winning end
  changes.

- [ ] **Step 1: Write the failing tests** (a synthetic airport, runway 09/27 and a parallel 09L/27R 230 m north)
  - `LinedUp.FiresAfterTenSeconds`: 8 nm out on the 27 centreline, tracking 270 for 11 s → `runway` "27", `time` the
    first sample's.
  - `LinedUp.NotBeforeTenSeconds`: 9 s → nothing.
  - `LinedUp.DownwindIsNot`: 1.5 nm abeam, tracking 090 → nothing.
  - `LinedUp.OutsideTenNmNotYet`: 12 nm out → nothing; at 9.9 nm, held → fires.
  - `LinedUp.PicksNearerParallel`: on the parallel's centreline → its name.
  - `LinedUp.PastThresholdIsNot`: beyond the threshold on runway heading (a missed approach) → nothing.
  - `LinedUp.CrabDoesNotMatter`: heading off by 8°, positions moving along the centreline → fires.
  - `LinedUp.OnlyInDescentOrApproach`: in Cruise, same geometry → nothing.
  - `LinedUp.FiresAgainAfterGoAround`: fires on 27; a GoAround sample; lined up with 27R → fires again with "27R".
- [ ] **Step 2: Run them to make sure they fail.** Run: `ctest --preset x64-release -R LinedUp`.
- [ ] **Step 3: Implement it, then the FlightTracker wiring**
  Add `FlightTracker.LogsLinedUp`, a test that fails first: block out with the destination loaded, then samples
  down the centreline → a `lined_up` event.
- [ ] **Step 4: Run all tests.** Expected: all pass.
- [ ] **Step 5: Commit** — "Log lined_up when lined up with a destination runway (D-143, D-145)".

### Task 4: `pickGate`, wingspans and preferred gates

**Files:**
- Create: `src/gate_picker.h`, `src/gate_picker.cpp`.
- Modify: `src/catalog.h/.cpp`: `CatalogType` gains `double wingspanM` (`wingspan_m`, required).
- Modify: `config/catalog.yaml`: 737-600 34.3, 737-800 35.8, BBJ2 35.8, BCF 35.8, 777-200ER 60.9, 777-300ER 64.8,
  777F 64.8.
- Modify: `src/airline_config.h/.cpp`: `AirlineConfig` gains
  `std::map<std::string, std::vector<std::string>> preferredGates` (`preferred_gates:` mapping, optional).
- Modify: `config/airline.example.yaml`, with an example: `preferred_gates: { KPDX: [D14] }`.
- Modify: `CMakeLists.txt`, `tests/test_airline.h` if its catalog text needs the new field.
- Test: `tests/gate_picker_test.cpp`, `tests/catalog_test.cpp`, `tests/airline_config_test.cpp`.

**Interfaces:**
- Consumes: `ParkingSpot`, `AirportFacilities`, `spotShortName` (Task 1); `runwayEndName`.
- Produces:
  ```cpp
  enum class SpotUse { Gate, Ramp, CargoRamp };
  SpotUse spotUseFor(std::string_view flightKind);  // passenger, vip → Gate; charter, delivery → Ramp; cargo → CargoRamp
  enum class PickReason { Preferred, Closest, AnyRamp, GateFallback };
  std::string_view pickReasonCode(PickReason r);  // "preferred", "closest", "any_ramp", "gate_fallback"
  struct GatePick { ParkingSpot spot; PickReason reason; };
  struct GateContext { double wingspanM = 0; SpotUse use = SpotUse::Gate; std::vector<std::string> preferred; };
  bool spotFits(const ParkingSpot& s, double wingspanM);  // 2 * radiusM >= wingspanM
  // landingRunway "26L" or "" (then the reference point is used).
  std::optional<GatePick> pickGate(const AirportFacilities& airport, std::string_view landingRunway,
                                   const GateContext& context);
  ```
- Spot type sets: gates {8, 9, 10}; GA ramps {2, 3, 4}; cargo ramps {5}; any ramp {2, 3, 4, 5}.
- The order:
  - Gate use: the first preferred name whose spot fits (`Preferred`), then the closest fitting gate (`Closest`);
    otherwise `nullopt`.
  - Ramp and CargoRamp: the closest fitting spot of their own set (`Closest`), then any ramp (`AnyRamp`), then a gate
    (`GateFallback`).
- The distance is to the runway as a segment between its two thresholds.

- [ ] **Step 1: Write the failing tests**
  - `GatePicker.BbjFitsKsfoD14`: radius 18 fits 35.8 m; radius 17.8 doesn't.
  - `GatePicker.PreferredFirstThatFits`: preferred [D12, D14] with D12 too small → D14, `Preferred`.
  - `GatePicker.ClosestGateToRunway`: two fitting gates → the one nearer 26L's segment, not the nearer threshold.
  - `GatePicker.CharterGetsRampNotGate`: a gate nearer than a GA ramp → the ramp.
  - `GatePicker.PreferredIgnoredForCharter`: preferred D14 and a charter → a ramp.
  - `GatePicker.FreighterGetsCargoRamp`.
  - `GatePicker.NoGaRampFallsToAnyRamp`: only a cargo ramp fits a charter → it, `AnyRamp`.
  - `GatePicker.NoRampFallsToGate`: → `GateFallback`.
  - `GatePicker.MilitaryNeverPicked`.
  - `GatePicker.UnknownRunwayUsesReferencePoint`.
  - `GatePicker.NoGateFitsGivesNothing`.
  - `Catalog.ReadsWingspan`, `AirlineConfig.ReadsPreferredGates` (and it's optional).
- [ ] **Step 2: Run them to make sure they fail.** Run: `ctest --preset x64-release -R "GatePicker|Catalog|AirlineConfig"`.
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run all tests.** Expected: all pass.
- [ ] **Step 5: Commit** — "Pick the arrival gate by D-074's rules (D-147)".

### Task 5: SayIntentions' gate, `assignGate` and the `arrival_gate` event

**Files:**
- Modify: `src/sayintentions.h/.cpp`: flightJSON fields, the URLs and the answers.
- Create: `src/gate_assigner.h`, `src/gate_assigner.cpp` (plain C++, the decisions).
- Modify: `src/planner.h`, `src/qt_planner.h/.cpp`: four calls.
- Modify: `src/flight_watcher.h/.cpp`: poll, decide, send, log.
- Modify: `src/app_main.cpp`: give `FlightWatcher` the gate context (from the planned flight's kind, the catalog
  type's wingspan, `preferred_gates`) and the bridge's airports.
- Modify: `docs/notes/sayintentions-api.md`.
- Test: `tests/atis_test.cpp` (the SayIntentions parsers), `tests/gate_assigner_test.cpp`, `tests/qt_models_test.cpp`
  (FlightWatcher with the fake planner).

**Interfaces:**
- Consumes: `pickGate`, `GateContext`, `spotName`, `spotShortName`, `nearestSpot` (Tasks 1, 4).
- Produces:
  ```cpp
  // sayintentions.h
  inline constexpr const char* kSiFlightJsonUrl = "http://localhost:43117/flightJSON";
  struct SiFlight {
      std::string destination, assignedGate, arrivingRunway;  // runway normalized, "" if none
      std::optional<double> gateLat, gateLon;
  };
  // flight_details.current_flight's flight_destination, assigned_gate, assigned_gate_lat/lon (strings), and
  // flight_plan_arriving_runway, else flight_details.atc.approach.runway. Nothing else is kept.
  std::optional<SiFlight> parseSiFlight(std::string_view json);
  std::string assignGateUrl(std::string_view apiKey, std::string_view gate, std::string_view icao);
  std::string getParkingUrl(std::string_view apiKey);
  std::string getAirportUrl(std::string_view apiKey);
  std::optional<std::string> parseAssignedGate(std::string_view json);  // assigned_gate_name
  struct SiParking { std::string name; double lat = 0, lon = 0; };
  std::optional<SiParking> parseParking(std::string_view json);
  // Planner (each `done` called once; `error` set for network, HTTP or {"error": ...} failures, with the message)
  virtual void fetchSiFlight(std::function<void(QByteArray body, QString error)> done) = 0;
  virtual void assignGate(QString gate, QString icao, std::function<void(QByteArray body, QString error)> done) = 0;
  virtual void fetchParking(std::function<void(QByteArray body, QString error)> done) = 0;
  virtual void fetchSiAirport(std::function<void(QByteArray body, QString error)> done) = 0;  // --probe only
  // gate_assigner.h
  struct GatePlan {
      bool send = false;
      std::string shortName;  // to send
      nlohmann::json event;   // arrival_gate, complete when !send; finished by finishSend when send
  };
  // nullopt while SayIntentions has no gate, or its destination isn't `destinationIcao`.
  std::optional<GatePlan> planGate(const SiFlight& si, const std::string& destinationIcao,
                                   const AirportFacilities& airport, const GateContext& context);
  // result "assigned", "unknown_gate" (an error starting "Unknown gate") or "error".
  nlohmann::json finishSend(nlohmann::json event, const std::string& error, const std::optional<SiParking>& readBack);
  ```
- The `arrival_gate` event:
  - `{"time", "icao", "runway", "si_gate", "gate", "reason", "sent", "result", "si_parking", "error"}`;
  - `gate` is the `spotName` ("" if none fits);
  - `reason` is a `pickReasonCode`, or "none_fits";
  - `result` is `same_as_si`, `assigned`, `unknown_gate`, `error` or `none_fits`;
  - `si_parking` is `getParking`'s name after a send.
- FlightWatcher's flow:
  - poll from block out every 30 s;
  - stop at block in, or once an `arrival_gate` is logged (one try per flight);
  - wait while the destination's airport isn't loaded yet;
  - errors from flightJSON are silent (SayIntentions may not be running).
- With `--probe`, after the `arrival_gate`: one `getAirport`, logged as `probe_si_airport` with any `api_key` member
  removed at every depth.

- [ ] **Step 1: Write the failing tests**
  - `SayIntentions.SiFlightPicksFieldsOnly`: a flightJSON with `api_key` and `email` → the fields above, gate
    position as doubles; runway from `flight_plan_arriving_runway`, else `atc.approach.runway`.
  - `SayIntentions.SiFlightWithoutGate`: `assigned_gate` "" → `assignedGate` "".
  - `SayIntentions.AssignGateUrl`: escapes; the gate and the airport in the query.
  - `SayIntentions.ParsesAssignedGateAndParking`: and the `{"error": ...}` forms give `nullopt`.
  - `GateAssigner.SameSpotNotSent`: SayIntentions' gate is 10 m from the picked spot → `send` false, result
    `same_as_si`.
  - `GateAssigner.OtherSpotSent`: → `send` true, `shortName` "E7".
  - `GateAssigner.WaitsWithoutGate`: → `nullopt`.
  - `GateAssigner.OtherDestinationIgnored`: → `nullopt`.
  - `GateAssigner.NoneFits`: → `send` false, result `none_fits`.
  - `GateAssigner.UnknownGateResult`: `finishSend` with "Unknown gate: E7" → `unknown_gate`.
  - `FlightWatcher.SendsAndLogsOnce`: the fake planner returns a SayIntentions gate. Expected: one `assignGate`, one
    `getParking`, one `arrival_gate` in the queue, and no second call on the next poll.
  - `FlightWatcher.NoSayIntentionsNoGate`: flightJSON errors → no event, no error line.
- [ ] **Step 2: Run them to make sure they fail.** Run: `ctest --preset x64-release -R "SayIntentions|GateAssigner|FlightWatcher"`.
- [ ] **Step 3: Implement the parsers, `gate_assigner`, the Planner calls and the FlightWatcher flow**
- [ ] **Step 4: Run all tests.** Expected: all pass.
- [ ] **Step 5: Commit** — "Ask SayIntentions for BlockTime's gate once it has picked its own (D-141, D-147)".

### Task 6: The parked spot stored and shown; the new Live lines

**Files:**
- Modify: `src/airline_db.cpp`: migration 6.
  ```sql
  ALTER TABLE flight ADD COLUMN origin_parking TEXT CHECK (origin_parking IS NULL OR origin_parking <> '');
  ALTER TABLE flight ADD COLUMN arrival_parking TEXT CHECK (arrival_parking IS NULL OR arrival_parking <> '');
  ```
- Modify: `src/flight_settlement.h/.cpp`: `FlownFlight` gains `originParking` and `arrivalParking`, from the block
  events ("" stored as NULL).
- Modify: `src/airline_queries.h/.cpp`, `src/planned_flight.h/.cpp`, `src/planned_flight_model.*`: the load-in spot.
- Modify: `src/flight_stages.cpp`:
  - Arrival "Parked at …";
  - the `arrival_gate`, `lined_up` and renamed 1,000 ft lines (copy in Global Constraints).
- Modify: `qml/PlannedFlightPanel.qml` ("Load in at …"), `qml/FlightSummary.qml` (the parked spot).
- Modify: `docs/database.md`, `docs/design.md` §13.3, §13.5, §13.6.
- Test: `tests/airline_db_test.cpp`, `tests/flight_settlement_test.cpp`, `tests/flight_stages_test.cpp`,
  `tests/planned_flight_test.cpp`.

**Interfaces:**
- Consumes: `block_out` / `block_in` `parking` (Task 1); `lined_up` (Task 3); `arrival_gate` (Task 5).
- Produces:
  - `FlightView` gains `std::string arrivalParking`;
  - `PlannedFlightView` gains `std::string loadInParking`: the airplane's latest flight's `arrival_parking`, when
    that flight's arrival airport is the airplane's location; "" otherwise.

- [ ] **Step 1: Write the failing tests**
  - `AirlineDb.Migration6AddsParking`: an empty database through every migration has both columns, and `''` is
    refused.
  - `FlightSettlement.ParkingStored`: block events with "Gate D14" and "Gate E7" → stored.
  - `FlightSettlement.NoParkingStoredAsNull`.
  - `PlannedFlight.LoadInParkingFromLastFlight`: and "" when the airplane has moved since (another airport).
  - `FlightStages.ParkedAtSpot`: and plain "Parked" with `parking` "".
  - `FlightStages.ArrivalGateLines`: one case per `result`, with the exact copy.
  - `FlightStages.LinedUpLine`: "Final approach, runway 26L" under Descent.
  - `FlightStages.ShortFinalAtThousandFeet`: the final approach line ends at the marker and "Short final" starts
    there, ending at touchdown; without `lined_up`, "Short final" alone.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement**
- [ ] **Step 4: Run all tests.** Then migrate a scratch copy of Dave's `blocktime.db` with `--demo` off and check
  `errors.log` stays empty.
- [ ] **Step 5: Commit** — "Store and show where each flight parked; Live's gate and final approach lines".

### Task 7: Live in three columns; the leaner flight panel (D-139, D-140)

**Files:**
- Create: `src/airspeed_display.h` (header-only, `blocktime-core`).
- Create: `qml/StatusTile.qml`: a label on green (on) or grey (off), colours from Theme.
- Modify: `src/recorder_bridge.h/.cpp`:
  - `Q_PROPERTY bool showMach`, `double mach`;
  - `QVariantList enginesRunning` (one bool per engine, clamp 1..4);
  - `bool parkingBrakeSet`.
- Modify: `qml/LivePage.qml`: the header full width; below it a row with FlightProgress, FlightPanel and EventFeed.
- Modify: `qml/FlightPanel.qml`:
  - the Aircraft card and the Position tile go;
  - Parking brake and engines 1..n become `StatusTile`s;
  - the airspeed tile shows "M.78" or "285 kt".
- Modify: `qml/Theme.qml` (status colours, if missing), `src/app_main.cpp` (the `--demo` sample gets Mach 0.78).
- Test: `tests/airspeed_display_test.cpp`, `tests/qt_models_test.cpp`.

**Interfaces:**
- Produces: `bool showMach(bool wasMach, double mach)`, which is true at 0.70 or above, false below 0.66, and
  `wasMach` in between.

- [ ] **Step 1: Write the failing tests**
  - `AirspeedDisplay.SwitchesAt070`, `AirspeedDisplay.BackBelow066`, `AirspeedDisplay.HoldsBetween` (0.68 keeps
    either).
  - `RecorderBridge.EnginesRunningPerEngine`: 2 engines, engine 2 only → `[false, true]`.
- [ ] **Step 2: Run them to make sure they fail.**
- [ ] **Step 3: Implement, then the QML**
- [ ] **Step 4: Check**
  - all tests pass;
  - qmllint reports nothing;
  - `build\x64-debug\BlockTime.exe --demo --grab` shows the three columns, the status tiles and "M.78";
  - look at the grab before calling it done.
- [ ] **Step 5: Commit** — "Live in three columns; status tiles and Mach (D-139, D-140)".

### Task 8: Version, docs, review, Dave's config

**Files:**
- Modify: `CMakeLists.txt` (0.10.0).
- Modify: `docs/roadmap.md` (milestone 9 built), `docs/progress.md`, `README.md` (if config steps changed).
- Modify: Dave's `%LOCALAPPDATA%\BlockTime\catalog.yaml`: add `wingspan_m` to each type, after a backup
  `catalog.before-m9.yaml`. Back up `blocktime.db` to `blocktime.before-m9.db` before 0.10.0 migrates it.

- [ ] **Step 1:** Bump the version; make `docs/recorder.md`, `design.md` and `database.md` true to the code.
- [ ] **Step 2:** Clean release and debug builds with no warnings; all tests pass; qmllint is clean
  (`superpowers:verification-before-completion`).
- [ ] **Step 3:** Whole-milestone review (`superpowers:requesting-code-review`, then `receiving-code-review`). Fix
  Critical and Important findings with a test that fails first.
- [ ] **Step 4:** Dave's config and backups, as above. Start the app once and check that `errors.log` gets nothing
  new.
- [ ] **Step 5:** Progress entry with Dave's sim checks (below), printed in the reply too. Commit, then push `main`
  after Dave's sim test (D-121).

## Dave's sim test (two legs in N814SX, the BBJ2)

N814SX is at KSFO with 6,434 lb on record, so load at least 6,134 lb. Start with `--probe`, which logs both flap
values and SayIntentions' `getAirport` answer once.

- **Leg 1, KSFO → KLAS, VIP airliner (a gate):**
  - the gate line in cruise once SayIntentions picks its gate;
  - whether SayIntentions accepts "E7"-style names at KLAS (its own name there was "E7A") and shows BlockTime's gate
    on its status screen (D-141's open question);
  - Approach at the first flaps;
  - "Final approach, runway …" around 10 nm out, turning into "Short final" at 1,000 ft;
  - "Parked at Gate …" and the stored spot.
- **GSX turnaround at KLAS**, then **leg 2, KLAS → KSAN, Charter (a ramp):**
  - "Load in at KLAS, Gate …" on the planned flight;
  - a ramp picked at KSAN, where its GA ramps (radius 23–24 m) fit the BBJ2;
  - how SayIntentions takes a ramp's short name ("70").
- **At KSAN:** New Flight shows "Load in at KSAN, Parking …"; then cancel it.
- **Throughout:** the three-column Live; the brake and engine tiles; Mach from about FL220.
- **Not covered by the BBJ2:** whether the 777's flap value follows its lever (D-142). The 3,000 ft AGL fallback
  covers it until the next 777 flight.

The numbered steps with expected results are written at the end of the milestone, when the build is ready.
