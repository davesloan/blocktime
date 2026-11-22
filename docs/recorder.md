# Recorder spec

The recorder is a small native Windows program (C++) that connects to Microsoft Flight Simulator 2020 through
the SimConnect SDK. Its only job is to **observe the sim and report facts** about the flight: events, phases,
and a light stream of live data. All airline logic (money, loads, schedules, scoring) belongs to the web app.

It is being built **first**, because everything else depends on reliable flight data.

## Goals

1. Detect the start and end of a flight using the parking brake (block out / block in).
2. Detect flight phases using plan-aware, forward-only rules (below).
3. Capture key numbers: fuel at block out and block in, times, positions, touchdown rate.
4. Write everything to a local log file **before** anything else, so nothing is lost if the web app isn't running.
5. Survive sim crashes and recorder restarts mid-flight.
6. Stay light: minimal memory and CPU, minimal SimConnect load on the sim.

## Non-goals

- No airline logic (costs, revenue, passenger loads, schedules).
- No rich UI. A tray icon with a short status line is enough (e.g. "Connected · Recording N738DV · KSAN→KSLC · Climb").
  Visual dashboards live in the web app.
- No live map.

## Milestones (suggested order)

1. **Connect and read.** Connect to MSFS, subscribe to a handful of SimVars, print/log them. Handle "sim not running"
   and reconnect.
2. **Block out / block in.** Detect parking brake release and set; write events with time, position, fuel.
3. **Durable local log.** Append-only event log on disk; recorder restart mid-flight resumes the open flight.
4. **Phases.** Implement the phase rules below, including level-off sub-states and go-arounds.
5. **Touchdown capture.** Fast sampling near the ground; record touchdown rate.
6. **SimBrief plan.** Fetch the latest OFP to get planned cruise altitude, origin, destination (needed by phase rules).
7. **Delivery to the app.** Send events to the local web app (mechanism TBD; log the decision). Until the app exists,
   the log file is the output.

Each milestone should be verifiable in the sim by Dave. When finishing one, write in `docs/progress.md` exactly
what to test and what result to expect.

## Flight lifecycle

- **Block out (flight start):** parking brake released while on the ground at the departure gate.
- **Block in (flight end):** parking brake set while on the ground after a landing.
- Block time = block in − block out. This is the definition used by the whole app.
- A flight that has started but not ended is **open**. Open-flight state must be persisted so a recorder or sim
  restart can resume it.

### Sim crash / disconnect during a flight

The recorder must not guess. It keeps the open flight and its events, marks the flight as **interrupted**, and
leaves the resolution to the user (in the web app):

- **Discard:** the flight is removed as if it never happened; the plane stays at the origin.
- **Complete:** the flight is treated like a background flight; the plane is moved to the destination.

If the sim reconnects and the user carries on flying the same flight, the recorder may resume it (design the
resume rules carefully and log the decision).

### Diversions

If the flight ends (block in) at an airport other than the planned destination, record the actual airport.
The app handles the consequences.

## Phases

Phases are reported as events and shown live in the web app.

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
- Aircraft title (so the app can check the loaded aircraft matches the selected airframe)
- Touchdown vertical speed (use a dedicated touchdown SimVar if MSFS 2020 provides a reliable one; otherwise sample
  vertical speed at high rate around the moment on-ground changes)
- Later (SayIntentions features): altimeter setting, for checking it against the ATIS

**PMDG note:** PMDG aircraft sometimes expose systems through their own SDK rather than standard SimVars. Verify that
parking brake and fuel read correctly with the PMDG 737 and 777; record findings in `docs/notes/`.

## Sampling strategy

- Use change-based or once-per-second updates for most values.
- Switch to fast sampling only briefly near the ground on approach/landing to capture touchdown, then drop back.
- Live data for the web app's dashboard: about once per second is enough.

## Output: local event log

- Append-only file on disk (format to be decided and logged; JSON Lines is a natural fit).
- One record per event, each with a timestamp (UTC), event type, and payload.
- Example event types: `sim_connected`, `sim_disconnected`, `block_out`, `phase_changed`, `level_off`,
  `touchdown`, `go_around`, `block_in`, `flight_interrupted`, `telemetry` (periodic live sample).
- The log must be readable by the web app later to catch up on anything it missed.

## Fuel tracking context (for the app, but the recorder must supply the data)

The app tracks fuel on board per airframe between flights:

- Fuel charged = fuel at block out − fuel remaining from the airframe's previous flight.
- Fuel can't be removed: if the sim shows less fuel at block out than the airframe had, the app warns the user to load
  at least the stored amount and won't accept the departure until it matches.

So the recorder must report accurate fuel weight at block out and block in.

## Resource budget

The recorder runs alongside MSFS, PMDG aircraft, SayIntentions and other add-ons. Keep memory and CPU minimal and keep
SimConnect requests few and slow. Prefer simple, single-threaded designs unless there's a clear need otherwise.
