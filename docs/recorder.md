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
4. **Phases.** Implement the phase rules below, including level-off sub-states and go-arounds.
5. **Touchdown capture.** Fast sampling near the ground; record touchdown rate.
6. **SimBrief plan.** Fetch the latest OFP to get planned cruise altitude, origin, destination (needed by phase rules).
7. **Procedure capture.** Log the cockpit state the end-of-flight analysis needs (D-040). Examples: lights
   (beacon, landing, seatbelt signs), gear and flaps, altimeter settings, and whether the aircraft is configured
   and stable at 1,000 ft. The recorder only reports what changed and when; judging it is the app's job. What to
   capture gets brainstormed at the start of this milestone. Many PMDG switches may not show up in standard
   SimVars, so start with a probe flight on the 737 and the 777.
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
- The app shows the "Not recording" banner from the same check, run on each sample while no flight is open.

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
7. **Approach:** based on distance to destination plus height (e.g. within ~30 nm and below ~10,000 ft), not
   vertical speed alone.
8. **Go-around:** the one allowed backwards step. A sustained climb from Approach becomes "Go-around", then back to
   Approach. Log it as an event.
9. **No plan / no cruise altitude:** fall back to the highest level held for several minutes.

All thresholds are starting points. When you tune one, record the value and reason in `docs/decisions.md`.

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
- Later (milestone 7, procedure capture): cockpit state for the flight analysis, including the altimeter setting
  to check against the SayIntentions ATIS (D-040)

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
- **When:** from the first valid telemetry sample in a flight (the same point at which block-out detection starts),
  whether or not a flight is open. Change-based, at most once a second.
- **Event:** `gsx_service` with `service`, `state`, `time` (and counts where they apply). Read only: the recorder
  never writes to GSX.
- The app decides which leg each service belongs to: deboarding goes with the leg that just ended; catering,
  fuel and boarding with the next leg.

## Parking spots and runways (D-074)

The app needs an airport's parking spots (name, position, radius, type) and runways for the departure runway, the
arrival gate and the gate Dave parked at. The recorder fetches them on request through SimConnect's facility data
while the sim runs, and hands them to the app. Matching positions to spots and runways is the app's job. That match
names a runway or gate; it never judges precision (D-122).

## Sampling strategy

- Use change-based or once-per-second updates for most values.
- Switch to fast sampling only briefly near the ground on approach/landing to capture touchdown, then drop back.
- Live data for the app's screens: about once per second is enough.
- Implemented (D-023): one definition, `PERIOD_SECOND` + `CHANGED` with per-variable thresholds, and a one-off snapshot
  on sim start, aircraft load and unpause. Junk samples are filtered before logging (D-025).
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
| `telemetry` | A valid sample: on change, at most once a second; identical repeats are skipped. Includes `engines`, `eng_running` (per engine) and `fuel_flow_pph` (total) |
| `telemetry_invalid` | Samples start being rejected as junk; `reason` says why |
| `block_out` | The flight started: a matching parking brake release that was followed by 10 m of movement. `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` are from the release; also `flight_id` (= that time), `moved_m`, `atc_id`, `title`, `planned_flight_id`, `aircraft_folder` |
| `block_out_refused` | A release didn't match the planned flight, so no flight opened (D-088). `reason` is `no_planned_flight`, `wrong_aircraft_type`, `wrong_airport` or `fuel_below_record`; the rest is what the check saw |
| `block_in` | The flight ended: after landing, a brake set held until every engine was off. `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` are from the set; also `flight_id`, `shutdown_time`, `shutdown_fuel_lb`, `block_time_s` |
| `flight_interrupted` | A flight ended without block in; `flight_id`, `reason`. The reason is the session's lost reason (`sim_quit`, `sim_process_gone`, ...), `recorder_exit`, `sim_stopped` (the flight was left: main menu, restart, or MSFS killed) or `recorder_crash` (logged on the next start for a flight a killed recorder left open). The flight is never resumed (D-046) |
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
