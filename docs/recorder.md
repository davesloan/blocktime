# Recorder spec

The recorder is the part of the BlockTime app (C++) that connects to Microsoft Flight Simulator 2020 through the
SimConnect SDK. Since D-063 it isn't a separate program: it's a library (`Recorder`, D-053) that the app runs on its
own thread. Its only job is to **observe the sim and report facts** about the flight: events, phases, and a light
stream of live data. All airline logic (money, loads, schedules, scoring) belongs to the rest of the app.

It was built **first**, because everything else depends on reliable flight data.

## Goals

1. Detect the start and end of a flight using the parking brake (block out / block in).
2. Detect flight phases using plan-aware, forward-only rules (below).
3. Capture key numbers: fuel at block out and block in, times, positions, touchdown rate.
4. Write everything to a local log file **before** anything else, so nothing is lost even if the rest of the app
   fails (D-008).
5. Survive sim crashes and recorder restarts mid-flight.
6. Stay light: minimal memory and CPU, minimal SimConnect load on the sim.

## Non-goals

- No airline logic (costs, revenue, passenger loads, schedules).
- No UI of its own. It reports to the app through `RecorderObserver`, and the app shows everything (D-063).

## Milestones (suggested order)

1. **Connect and read.** Connect to MSFS, subscribe to a handful of SimVars, print/log them. Handle "sim not running"
   and reconnect.
2. **Block out / block in.** Detect parking brake release and set; write events with time, position, fuel.
   **Built in 0.2.0** (D-041 to D-044); the in-sim checks are listed in `docs/progress.md`.
3. **Ending flights cleanly (no resume, D-046).** **Built in 0.3.0** (D-047 to D-049); the in-sim checks are in
   `docs/progress.md`.
   - Exiting to the main menu with a flight open logs `flight_interrupted` (`sim_stopped`).
   - If the recorder itself is killed or the PC loses power mid-flight, no `flight_interrupted` gets written. On
     the next start, the recorder closes a flight left open in the log as interrupted (`recorder_crash`), so it
     isn't silently lost.
   - A block event whose write failed is held, retried, and shown on the console.
   - **Then the Qt UI test builds (D-050):** two test builds on the branch `qt-ui-spike` decided between one native
     Qt app and the recorder plus a web app. **Qt won (D-063)**, the branch was merged, and the recorder now runs
     inside the app (`docs/qt-ui-test-context.md`). The console recorder was dropped (D-065).
4. **Phases.** Implement the phase rules below, including level-off sub-states and go-arounds. **Built in 0.8.0** (roadmap
   milestone 7, D-124).
5. **Touchdown capture.** Fast sampling near the ground; record touchdown rate. **Built in 0.8.0** (D-125).
6. **SimBrief plan.** Fetch the latest OFP to get planned cruise altitude, origin, destination (needed by phase rules).
7. **Procedure capture.** Log the cockpit state the end-of-flight analysis needs (D-040). **Built in 0.11.0** (roadmap
   milestone 10, `docs/debrief.md` section 1, D-159): a change-based `cockpit` event (landing lights, seat belt sign,
   gear, and from PMDG's SDK broadcast VREF, landing flaps and the autopilot), attitude and wind on `touchdown`, and a
   `bounces` event. The recorder only reports what changed and when; judging it is the app's job.
8. ~~Delivery to the app.~~ **Not needed (D-063):** the recorder runs inside the app. The app reads flights back
   from the event log (D-060), and live values reach the UI through `RecorderObserver`.

**Replanned (2026-11-22, D-077):** from here on, milestones work toward the complete flight flow in `design.md`
section 13 (D-069). The list above stops at milestone 3; **`docs/roadmap.md` has milestones 4–10.** The old
milestones 4–7 live on there: phases and touchdown in milestone 7, procedure capture in milestone 10, and the
SimBrief plan in milestone 5.

Each milestone should be verifiable in the sim by Dave. When finishing one, write in `docs/progress.md` exactly
what to test and what result to expect.

## Flight lifecycle

- **Block out (flight start):** parking brake released while on the ground at the departure gate (D-005, D-031).
  - **Typical gate sequence:** the PMDG startup profile may set the brake (or Dave sets it), Dave sets it for GSX
    pushback, releases it for the push, sets it after the push, releases it to taxi, and may set and release it
    again at hold short.
  - **Which release counts:** before a flight is open, each release on the ground is a candidate. It is confirmed
    only when the aircraft moves **≥ 10 m** from where the brake was released, with the brake still off. If the
    brake is set first, the candidate is dropped.
  - **Recorded time:** the block-out time is the time of the confirmed release (like ACARS OUT).
  - **After block out:** later sets and releases don't affect it.
- **Block in (flight end):** parking brake set while stopped on the ground after a landing, and kept set until all
  engines are shut down (D-005, D-032). Block in = the time of that set. A set that is released again with an
  engine still running (hold short, waiting for a gate) is ignored.
- **Details (D-042):**
  - A release or set is a change between two valid samples. The first sample after start or after a lost session
    only sets the baseline.
  - "Landed" means on the ground after being airborne, and airborne means `SIM ON GROUND` false and at least
    100 ft AGL.
  - Engines off means no `GENERAL ENG COMBUSTION:n` is true.
  - **D-045:** a set drops a block-out candidate and a release drops a block-in candidate. A `SIM ON GROUND`
    flicker drops neither. After junk samples (main menu, loading) with no flight open, the brake state starts
    over.
  - Times are real-world UTC when the sample arrives (up to about 1 s late).
- **Not handled yet:** a flight that returns to the gate without taking off stays open until the main menu or
  the end of the session interrupts it.
- Block time = block in − block out. This is the definition used by the whole app.
- A flight that has started but not ended is **open**. Only one flight is open at a time. An open flight is
  never resumed (D-046): if it ends any way other than block in, it is logged as interrupted.
- **Turnarounds (D-046):** after block in Dave either exits to the main menu, or deboards, refuels, boards,
  restarts the engines and flies the next flight from the same gate. Several flights are often chained in one sim
  session.
  The parking brake stays set from block in until the next push (Dave, 2026-11-22), so the next block out is
  timed at the push release, with fuel after the refuel.

### Sim crash / disconnect during a flight

The recorder must not guess. It marks the open flight as **interrupted**. The app then **discards** it as if it
never happened, and the plane stays at the origin (D-072, `design.md` section 13.8). There is no "complete" option.

**No resume (D-046):** when the sim session ends, the recorder exits, or Dave exits to the main menu with a
flight open, the recorder logs `flight_interrupted` and forgets the flight. It never picks it up again, even if
the sim reconnects.
- **Main menu (D-047):** the signal is the sim's `Sim` state changing to stopped. It comes within about 1 s of
  leaving a flight (main menu, restart, Escape → Quit). It doesn't come while flying, in the Escape menu or in
  active pause. Reason `sim_stopped`. Killing MSFS also logs `sim_stopped` first (about 5 s before the process is
  gone), so a sim kill shows as `sim_stopped`, not `sim_process_gone`.
- **Recorder killed or power cut (D-048):** at the next start, after the single-instance check, the recorder looks
  at up to 7 daily log files, newest first. If the last `block_out` / `block_in` / `flight_interrupted` line is a
  `block_out`, it logs `flight_interrupted` for it with reason `recorder_crash`. Known gap: if the sim is still in
  that flight, the next release and 10 m of taxi opens a new flight there, if the brake was set at the restart or
  is set later.

### Matching the planned flight (D-088)

A flight is only recorded when the sim matches the flight Dave set up in the app (`design.md` section 13.4):
- The app gives the recorder the **planned flight** (`FlightTarget` in `flight_match.h`, through a mutex-guarded
  slot): the type's sim folder, the origin airport's position and the airplane's fuel on record. The registration
  isn't compared (one livery serves many registrations, D-087).
- **The aircraft type (D-112)** is the folder holding the loaded `aircraft.cfg`, from the `AircraftLoaded` path. Each
  PMDG variant has its own folder (`PMDG 737-800`, `PMDG 737-800BBJ2`, `PMDG 777-200ER GE`…), and liveries point at
  their variant's folder. A type matches when the folder equals its `sim_folder` or starts with it plus a space,
  ignoring case, so the BBJ2 isn't a 737-800.
- **At the origin:** within 3 km of the airport reference point (D-088).
- **Fuel (D-110):** at least the airplane's fuel on record minus 300 lb. The fuel on record is what was in the tanks
  when the last engine stopped after its previous flight. A delivery (0 on record) always passes.
- The check runs at each brake release with no flight open, in that order. A release only becomes a block-out
  candidate when it matches. Otherwise no flight opens and the recorder logs `block_out_refused` once for that
  release, with the reason (`no_planned_flight`, `wrong_aircraft_type`, `wrong_airport`, `fuel_below_record`) and
  what it saw, so the log still explains what happened. A matched `block_out` carries the `planned_flight_id`.
- The app shows the "Not recording" banner from the same check, run on each sample while no flight is open, and
  again against the last sample when the planned flight changes (`MatchReporter`): telemetry comes only on change, so
  a parked airplane sends none.

### Diversions

If the flight ends (block in) at an airport other than the planned destination, record the actual airport.
The app handles the consequences.

## Phases

Phases are reported as events and shown live in the app.

1. At gate
2. Pushback (starts at block out)
3. Taxi out
4. Takeoff
5. Climb
6. Cruise
7. Descent
8. Approach
9. Landing (touchdown captured here)
10. Taxi in
11. Arrived (block in)

### Rules

These exist because simpler detectors (e.g. FlyHub) flip between climb and cruise when the aircraft levels off
temporarily, which is misleading.

1. **Plan-aware climb.** Climb ends only when the aircraft levels off **near the planned cruise altitude** from the
   SimBrief plan (e.g. within ~1,000 ft). A level-off at 9,000 ft on a KOAK departure is still Climb.
2. **Forward-only.** Phases don't go backwards. Once in Cruise you can't return to Climb; once in Descent you can't
   return to Cruise.
3. **Level-offs are sub-states,** not new phases: "Climb — level at 9,000 ft", "Descent — level at 11,000 ft".
4. **Step climbs stay in Cruise:** "Cruise — step climb".
5. **Debounce.** A transition only counts if its condition holds for a sustained period (start with ~30–60 s;
   tune and log). Brief bumps from turbulence or pitch changes must not trigger transitions.
   **A sim freeze adds nothing to a hold** (D-058). MSFS can stop for a minute while it loads scenery, with sim
   time stopped too. Time the sim spends sending no messages at all (the silence the recorder already tracks for
   `sim_unresponsive`) doesn't count toward any hold or timer, including short silences that never reach the
   30 s warning. Otherwise a freeze right after a level-off would look like a sustained level-off.
6. **Top of descent:** leaving cruise altitude with a sustained descent, not a small drift.
7. **Approach:** the first flap extension below 10,000 ft, with 3,000 ft AGL as the fallback (D-142), not
   vertical speed alone.
8. **Go-around:** the one allowed backwards step. A sustained climb from Approach becomes "Go-around", then back to
   Approach. Log it as an event.
9. **No plan / no cruise altitude:** fall back to the highest level held for several minutes.

All thresholds are starting points. When you tune one, record the value and reason in `docs/decisions.md`.

### Built (0.8.0, D-124)

`PhaseDetector` (`src/phase_detector.h`) runs from block out to block in. It was tuned on Dave's KBFI→KPDX and
KPDX→KSFO flights, which the tests replay (`tests/data/replay-*.csv`). Altitudes are indicated. Holds count only
active time (D-058): each sim message adds at most 2 s.

| Change | Rule |
|---|---|
| Pushback → Taxi out | The brake set after block out, then 3 kt with the brake off; or 6 kt at any time |
| Taxi out → Takeoff | 40 kt on the ground. Timed from where the acceleration began (walk back while each earlier sample is ≥ 0.5 kt slower) |
| Takeoff → Climb | Airborne at 1,000 ft AGL |
| Level-off (sub-state) | Within ±100 ft for 20 s. Reported as the window's mean: the nearest 1,000 ft when within 90 ft of it, else the nearest 100 ft (D-133); ends more than 200 ft away, with the whole level's mean rounded the same way |
| Climb → Cruise | A level within 1,000 ft of the plan's cruise altitude. Without a plan, a level above 10,000 ft held 5 minutes |
| Cruise: step climb | Leaving the cruise level upward; ends at the next level. A descent too far out for top of descent is a step down |
| Climb/Cruise → Descent | VS ≤ −500 fpm for 60 s. From Cruise only within 2 × 3 nm per 1,000 ft of the destination |
| Cruise/Descent → Approach | The first flap extension below 10,000 ft (`TRAILING EDGE FLAPS LEFT PERCENT` above 1 %), or below 3,000 ft AGL (D-142, D-148; 0.10.0). Until 0.9.0: within 30 nm and below 10,000 ft |
| Approach → Go-around | +500 fpm for 10 s, 200 ft above the approach's lowest point. The next level-off returns to Approach |
| → Landing | The first sample on the ground after being airborne |
| Landing → Taxi in | Below 30 kt on the ground |
| → Arrived | Block in |

Markers fire once per flight when crossed: above and below 10,000 ft, the plan's transition altitude (climbing) and
level (descending), 200 nm from the destination, and final approach at 1,000 ft AGL.

A pause (any `Pause_EX1` flag) adds nothing to holds either: the sim keeps answering the heartbeat while paused
(D-130). Approach can also start straight from Cruise when no descent was confirmed (D-130).

**Times:** a flight event's `data.time` is when the thing happened, which can be earlier than the line's `ts` and than
lines written before it. The takeoff is timed back to where the roll began, a level-off or cruise to the start of its
20 s hold, and top of descent to the start of its 60 s hold. Readers order by `data.time`, not by line. The touchdown's
time is that of the first sim frame on the ground, give or take 0.1 s: frames are stamped when the loop reads them.

## Data to capture

At minimum (exact SimVar names to be confirmed against the MSFS 2020 SDK docs and recorded in `docs/notes/`):

- Parking brake state
- On-ground flag
- Total fuel weight (lb) — SimBrief and the app work in weight, not gallons
- Latitude / longitude, altitude (MSL and AGL), vertical speed, ground speed, indicated airspeed
- Engines running (`GENERAL ENG COMBUSTION:n`) and fuel flow (`ENG FUEL FLOW PPH:n`). These are needed for block in
  (D-032).
- Aircraft title (so the app can check the loaded aircraft matches the selected airplane)
- Touchdown vertical speed (use a dedicated touchdown SimVar if MSFS 2020 provides a reliable one; otherwise sample
  vertical speed at high rate around the moment on-ground changes)
- Procedure capture (0.11.0, D-159): the `cockpit` event, at most once a second when a value changes:
  `landing_lights` (bool), `seat_belts` (0 off, 1 auto, 2 on), `gear_down` (bool, `GEAR TOTAL PCT EXTENDED` ≥ 0.995: it reads 0 to 1 despite the "percent" unit,
  ignored while paused), `pmdg` (whether PMDG's broadcast has arrived this session) and, only with it, `vref_kt`,
  `landing_flaps` and `autopilot`. With PMDG data the lights and seat belts come from it (the 737's seat belt SimVar
  never moves and `LIGHT LANDING` misses its retractable lights); without, from `LIGHT LANDING` and
  `CABIN SEATBELTS ALERT SWITCH`. It carries the `flight_id` while a flight is open, and one is written at block out
  whatever changed, so each flight's log holds the cockpit state at the push.
- `touchdown` also carries `sim_pitch_deg` (raw `PLANE PITCH DEGREES`: the SDK documents nose up as negative, unverified),
  `bank_deg`, `heading_true_deg`, `wind_dir_deg` and `wind_kt` (true, from `AMBIENT WIND DIRECTION` and `VELOCITY`) of
  the first frame on the ground. Fast sampling stays on for 15 s after the first contact, then a `bounces` event counts
  the airborne stretches of at least 0.5 s (a rollout slowed to taxi first logs the count so far).
- The altimeter settings (D-123), checked against the SayIntentions ATIS by the debrief

**Aircraft scope (D-039):** only the PMDG 737 and 777 families on MSFS 2020. Every 737 variant uses the same
variables, and so does every 777 variant, so testing one of each is enough. Default and other add-on aircraft are
not tested or supported.

**PMDG note:** PMDG aircraft sometimes expose systems through their own SDK rather than standard SimVars. Verify that
each value reads correctly on the 737 and 777; record findings in `docs/notes/`.

## GSX Pro (D-073)

GSX Pro reports each ground service's state as an L:var, which SimConnect can read (since MSFS SU12).
- **What's read:** the state of catering, refuelling, boarding, deboarding, and departure (pushback), plus the
  passenger counts while boarding and deboarding and the cargo loading percentage. Whether GSX is running is
  read first; without GSX, nothing is logged and nothing else changes.
- **States** as reported by community tools: 1 available, 2 not available, 3 bypassed, 4 requested, 5 performing,
  6 completed.
  - The first probe flight (2026-11-23, `docs/notes/probe-flight.md`) saw 1, 3, 4, 5 and 6 with those meanings. 6
    lasts about 10 s before the state goes back to 1.
  - Pushback (`DEPARTURE_STATE`) goes 4 → 5 → 1 with no 6, and boarding did too.
  - Requesting pushback sets the other services to 3.
  - Deboarding hasn't been seen yet.
  - The 777 flight (2026-11-24) saw boarding go 5 → 6 → 1, and deboarding 4 → 5 → 6 → 1.
- **Built in 0.9.0 (D-138):** `GsxWatcher` (`src/gsx_watcher.h`). SimConnection reads the five states and the boarding
  and deboarding passenger totals in their own definition, on every session (not only with `--probe`), once a second
  on change. Whether or not a flight is open.
  - Each change of a service's state is logged as `gsx_service`. A 0 (GSX not running, or the flicker around a load)
    is ignored, and so is a service's first state when it's "available". A new sim session starts afresh.
  - Read only: the recorder never writes to GSX.
- **The app's reading (D-138), in `GsxTimeline` (`src/gsx_timeline.h`), used by Live and by settling:**
  - a service starts when it's performing and is complete when it then shows completed or goes back to available;
    bypassed while performing abandons it;
  - catering, refuelling and boarding belong to the next block out, counted from the later of the last block in and
    the last `sim_stopped` / `sim_disconnected` (not BlockTime's own exit, `recorder_exit`);
  - deboarding belongs to the last block in, until the next block out;
  - an interrupted flight drops its services (D-072);
  - pushback is never a service; Live's pushback line says "(GSX)" when GSX was pushing at block out.

## ATC log, ATIS and altimeter (milestone 7, D-123)

For the flight analysis (D-040). None of it can be fetched after the flight, so it's logged while the flight is open.
- **ATC log:** SayIntentions' `getCommsHistory`, polled about once a minute and at block in. It logs each exchange
  once: the whole current flight is fetched each time, and exchanges already logged are skipped.
- **ATIS:** SayIntentions' `getWX`, logged whenever it changes:
  - the origin's every 10 minutes from block out to takeoff;
  - the destination's every 10 minutes from top of descent to landing.
- **Altimeter:** `KOHLSMAN SETTING MB:1/2` and `KOHLSMAN SETTING STD:1/2`, change-based. PMDG's SDK has no baro value.
  Whether PMDG drives these SimVars is checked first (`docs/notes/probe-flight.md`).
- **Ownership:** the web calls are the app's (the recorder reports sim facts), but the results go into the same event
  log, so the analysis reads one file. **Built in 0.8.0 (D-127):** the app's `FlightWatcher` pushes `atis` and
  `atc_exchange` events onto an `AppEventQueue`, and the recorder thread writes them, so the log has one writer. An
  exchange is logged when it's new and again when its content changes (a late reply); a reader keeps the last line
  per `id`. A failed call logs nothing and is retried at the next poll.

## Parking spots and runways (D-074)

The app needs an airport's parking spots (name, position, radius, type) and runways for the departure runway, the
arrival gate and the gate Dave parked at. The recorder fetches them on request through SimConnect's facility data
while the sim runs, and hands them to the app. Matching positions to spots is the app's job. That match names a gate;
it never judges precision (D-122).

**Runways (built in 0.8.0, D-126):** the recorder asks for the planned origin's and destination's runways at connect
and when the planned flight changes, logs them as `airport_runways`, and names the runways itself: the takeoff runway
at liftoff and the landing runway at Taxi in. A runway end fits when the roll's track (on the ground at 40 kt or more)
is within 20° of its heading and the roll is on average 75 m or less from its centreline; the nearest fit wins, and
with none the runway is `""`. The offsets are logged as information only.

## Sampling strategy

- Use change-based or once-per-second updates for most values.
- Switch to fast sampling only briefly near the ground on approach/landing to capture touchdown, then drop back.
- Live data for the app's screens: about once per second is enough.
- Implemented (D-023): one definition, `PERIOD_SECOND` + `CHANGED` with per-variable thresholds, and a one-off snapshot
  on sim start, aircraft load and unpause. Junk samples are filtered before logging (D-025).
- Milestone 7's requests are each their own definition, so a name the sim refuses loses only that request: the
  altimeter settings (`PERIOD_SECOND` + `CHANGED`, D-123), and the touchdown samples, requested every sim frame only
  from 300 ft AGL on the way down until the touchdown is captured (D-125).
- Connection: retry with back-off. A session counts as connected only after SimConnect's OPEN reply. It ends on
  QUIT, when `FlightSimulator.exe` exits, or after 10 failed calls in a row. Silence only logs a warning (D-028).

## Output: local event log

- Append-only JSON Lines files, one per UTC day: `%LOCALAPPDATA%\BlockTime\events\events-YYYY-MM-DD.jsonl` (D-020).
  The event types implemented so far are listed below.
- One record per event, each with a timestamp (UTC), event type, and payload.
- Example event types: `sim_connected`, `sim_disconnected`, `block_out`, `phase_changed`, `level_off`,
  `touchdown`, `go_around`, `block_in`, `flight_interrupted`, `telemetry` (periodic live sample).
- Block events (D-043). Times in `data` are the moment of the brake change; the line's `ts` is when it was written:
  - `block_out`: `flight_id` (= block-out time), `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` (at the release),
    `moved_m`, `atc_id`, `title`, `planned_flight_id` (the planned flight it matched), `aircraft_folder`.
  - `block_out_refused`: `time` (the release), `reason`, `planned_flight_id` (0 without one), `aircraft_folder`,
    `lat`, `lon`, `distance_m` (from the origin), `fuel_lb`, `min_fuel_lb`.
  - `block_in`: `flight_id`, `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` (at the set), `shutdown_time`,
    `shutdown_fuel_lb` (when the last engine stopped), `block_time_s`.
  - `flight_interrupted`: `flight_id`, `reason` (the session's lost reason, `recorder_exit`, `sim_stopped` or
    `recorder_crash`).
  - The recorder doesn't name airports; the app resolves them from `lat`/`lon`.
- **Failed writes (D-049):** block events go through an outbox. One whose write fails is held, shown in the app's
  event feed as a `WARNING` line, and retried oldest first: about every 5 s, or at each reconnect try (10–40 s)
  while waiting for the sim. Any still held at exit are written as `LOST:` lines to `errors.log`
  (`%LOCALAPPDATA%\BlockTime\errors.log`, D-067). A late write only moves the line's `ts`; the event's own times
  are in `data`.
- The log is the record the rest of the app reads back (D-060).
- At startup, if the log shows a flight that a killed app left open, it's closed as interrupted (`recorder_crash`,
  D-048) and the feed says so.

### Event types logged so far

| Type | When |
|---|---|
| `recorder_started` / `recorder_stopped` | The app starts recording / closes cleanly |
| `sim_connected` | MSFS answered the connection; includes MSFS and SimConnect versions, and `process_watch` (whether `FlightSimulator.exe` was found) |
| `sim_disconnected` | The session ended; `reason` is `sim_quit`, `sim_process_gone`, `too_many_errors` or `recorder_exit`. Only logged after a `sim_connected` |
| `sim_unresponsive` / `sim_responsive` | No messages for 30 s or more (common on loading screens) / messages are back; `silent_s` says how long. The session is kept |
| `sim_running` / `sim_stopped` | The sim enters / leaves a flight (MSFS "Sim" state). Flips several times while loading |
| `paused` / `unpaused` | The pause state changes. `flags` is the raw `Pause_EX1` value. `kinds` names its bits: `full` (main menu, loading screens), `active` (active pause), `sim` (Escape menu), `legacy` (unused), or `bit_<n>` for an unknown bit |
| `aircraft_loaded` | Path of the loaded `aircraft.cfg`; logged only when the aircraft changes |
| `telemetry` | A valid sample: on change, at most once a second; identical repeats are skipped. Includes `engines`, `eng_running` (per engine), `fuel_flow_pph` (total), from 0.8.0 `heading_true_deg` (logged only, D-129), from 0.9.0 `beacon` and `strobe` (`LIGHT BEACON`, `LIGHT STROBE`: the shutdown checklist, D-135), and from 0.10.0 `mach` (`AIRSPEED MACH`, D-140), `flaps_te_pct` (`TRAILING EDGE FLAPS LEFT PERCENT`, the Approach rule) and `flaps_handle` (`FLAPS HANDLE INDEX`, logged to learn whether it follows PMDG's lever; D-148) |
| `telemetry_invalid` | Samples start being rejected as junk; `reason` says why |
| `block_out` | The flight started: a matching parking brake release that was followed by 10 m of movement. `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` are from the release; also `flight_id` (= that time), `moved_m`, `atc_id`, `title`, `planned_flight_id`, `aircraft_folder`, and from 0.10.0 `parking`: the name of the nearest parking spot of the loaded airports within 40 m ("Gate D14", "Parking 70"), or "" (D-122, D-148) |
| `block_out_refused` | A release didn't match the planned flight, so no flight opened (D-088). `reason` is `no_planned_flight`, `wrong_aircraft_type`, `wrong_airport` or `fuel_below_record`; the rest is what the check saw |
| `block_in` | The flight ended: after landing, a brake set held until every engine was off. `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` are from the set; also `flight_id`, `shutdown_time`, `shutdown_fuel_lb`, `block_time_s`, and from 0.10.0 `parking` (as `block_out`) |
| `flight_interrupted` | A flight ended without block in; `flight_id`, `reason`. The reason is the session's lost reason (`sim_quit`, `sim_process_gone`, ...), `recorder_exit`, `sim_stopped` (the flight was left: main menu, restart, or MSFS killed) or `recorder_crash` (logged on the next start for a flight a killed recorder left open). The flight is never resumed (D-046) |
| `phase_changed` | A new phase (rules above): `flight_id`, `phase`, `previous`, `time`, `alt_ft`, `flight_level`. A go-around is a `phase_changed` to `go_around` |
| `level_off` / `level_off_ended` | A level-off began / ended: `flight_id`, `phase`, `time`, `alt_ft`, `flight_level` |
| `step_climb` / `step_climb_ended` | The cruise level was left / a new one reached: `flight_id`, `time`, `from_ft` / `alt_ft`, `flight_level` |
| `flight_marker` | `marker` is `above_10000`, `transition_altitude`, `within_200nm`, `transition_level`, `below_10000` or `final_approach`; also `flight_id`, `time`, `alt_ft`. Each once per flight, except `final_approach` (1,000 ft AGL in Approach), which a go-around re-arms from 0.10.0 (D-149) |
| `liftoff` | The first airborne sample after the takeoff roll: `flight_id`, `time`, `lat`, `lon`, `ias_kt`, `airport`, `runway`, `off_centreline_m`, `off_heading_deg` (D-126) |
| `touchdown` | From the sim-frame samples (D-125): `flight_id`, `time`, `lat`, `lon`, `vs_fpm` (the last airborne frame), `g` (the peak in the second after contact), `ias_kt` |
| `landing_runway` | At Taxi in: `flight_id`, `airport`, `runway`, `off_centreline_m`, `off_heading_deg` |
| `lined_up` | From 0.10.0, once per approach (a go-around re-arms it): airborne in Descent or Approach, the track (over at least 150 m) within 10° of a destination runway end's heading, within 0.5 nm of its extended centreline and 10 nm before its threshold, held 10 s. Every runway end at the destination is checked; the nearest centreline wins (D-143, D-145, D-148). `flight_id`, `time` (when the hold began), `airport`, `runway`, `alt_ft`, `distance_nm` (to the threshold) |
| `altimeter` | At block out, then on any change while a flight is open: `flight_id`, `capt_mb`, `fo_mb` (to 0.1 hPa), `capt_std`, `fo_std` (D-123) |
| `airport_runways` | An airport's runways from the sim's facility data: `icao`, `runways` (`lat`, `lon`, `heading`, `length_m`, `width_m`, `primary_number`, `primary_designator`, `secondary_number`, `secondary_designator`). From 0.10.0 the same request also reads the airport's reference point and parking spots; the spots aren't logged (the scenery's static data) but go to the app, which picks the arrival gate from them (D-147) |
| `gsx_service` | A GSX service's state changed (D-138), flight or not: `service` (`catering`, `refuelling`, `boarding`, `deboarding`, `pushback`), `state` (`available`, `not_available`, `bypassed`, `requested`, `performing`, `completed`), `time`; boarding and deboarding add `passengers` (GSX's own count so far) |
| `atis` | Written for the app (D-127): `flight_id`, `icao`, `role` (`origin` / `destination`), `letter`, `text`, `metar`, `altimeter`, `departing`, `arriving` |
| `atc_exchange` | Written for the app (D-127): `flight_id`, `si_flight_id`, `entry` (SayIntentions' exchange, verbatim) |
| `simconnect_exception` | SimConnect reported an exception; `call` names the request that caused it |
| `warning` | Something unexpected but not fatal (e.g. `FlightSimulator.exe` not found) |

## Fuel tracking context (for the app, but the recorder must supply the data)

The app tracks fuel on board per airplane between flights:

- Fuel charged = fuel at block out − fuel remaining from the airplane's previous flight.
- Fuel can't be removed: if the sim shows less fuel at block out than the airplane had, the app warns the user to load
  at least the stored amount and won't accept the departure until it matches.

So the recorder must report accurate fuel weight at block out and block in.

## Resource budget

The recorder runs alongside MSFS, PMDG aircraft, SayIntentions and other add-ons. Keep memory and CPU minimal and keep
SimConnect requests few and slow. Prefer simple, single-threaded designs unless there's a clear need otherwise.
