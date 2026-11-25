# Replaying a flight (milestone 11)

The design for **flight replay**. Brainstormed with Dave on 2026-11-24 (D-163 to D-168). Revised on 2026-11-25 after a
gut check: the replay is a separate, read-only process, not a mode of the live app (D-175). The approach table below
keeps both versions.

## Intent

Dave wants to watch the dashboard move without flying. A recorded flight plays back on Live, sped up, the way it
looked in the sim. It's the test bench for the dashboard milestones after it (`roadmap.md`): each change to the
dashboard is checked against real flights, on the desktop or the laptop, with no sim. Since D-174 (few sim tests),
it is the main way dashboard work gets checked.

"It has to replay the dashboard in its current state, so any changes underneath are replayed correctly." (Dave)

### Goals

- Any settled flight whose log still exists can be replayed from its Flight Summary.
- The replay drives **today's code** for everything from the recorder's output up: the bridge, the stages, the
  models, the QML. A replay of an old flight shows the latest dashboard.
- Claude Code can start a replay from the command line at a chosen point and grab a screenshot, so it can check a
  dashboard panel itself before handing it to Dave.

### Non-goals

- **Jumping ahead in the app.** Speed covers it (Dave): 10× turns a 10-hour leg into an hour.
- **Replaying the recorder's rules.** The log holds what the recorder concluded on the day (phases, line-up,
  touchdown). A changed phase rule or a new event, such as a takeoff-roll event, shows only on flights recorded after
  the change. Recorder rules stay tested with tests on real logs.
- Replaying interrupted flights: they're discarded (D-072), so they aren't in Flights.
- **Replaying beside a real flight in one window.** The replay has its own window and process (D-175).

### Anti-goals

- **A replay changes anything.** No event log lines, no database writes (money, location, fuel, hours, debrief), no
  calls to SayIntentions (`assignGate`, ATIS, the ATC log), SimBrief or Claude.
- **A replay costs a real flight.** It never takes the recorder's place: the real app keeps recording beside it.
- Weight: within the usual budget (under 100 MB, under 1% CPU) while it plays.

## Approaches considered

| Approach | Trade-off | |
|---|---|---|
| **D. A separate process, `BlockTime.exe --replay <flight>`**, like `--demo`: no recorder, the database opened read-only, a second `RecorderBridge` fed from the log | Today's code from the bridge up. Safe because the process never builds anything that writes or calls out, and SQLite refuses writes anyway. Nothing to coordinate with a real flight | **Chosen (Dave, 2026-11-25, D-175)** |
| A. A replay-only `RecorderBridge` inside the live app | Chosen on 2026-11-24 (D-165). Most of its cost was sharing a window with the real recorder: Live switching bridges, a real block out stopping the replay, Replay disabled while a flight is open, every live-only control hidden in replay mode (and every later one) | Superseded |
| B. Re-run the recorder over the logged telemetry | Would replay recorder changes too, but the log holds about one sample a second, on change, and none of the frame samples, PMDG broadcast, GSX variables or facility data the recorder reads: the touchdown, GSX and landing would replay wrong. Much bigger | |
| C. One bridge with a "replay" switch | Least wiring, but every piece that writes or calls out must check the switch, now and in every later milestone; one forgotten check posts money | |

## What Dave sees

**The Replay button.** On every Flight Summary of a settled flight, on Flights and on Live after block in. Pressing it
opens a second BlockTime window playing that flight; the main window carries on as before, recording included.
Greyed out, with the reason under it, when the flight's log is gone: `The log for 24 Nov is no longer on this PC`.

**The replay window.** Live alone: no side menu, no Flights, Fleet or Money. Its header reads
`Replay · XLY1231 KLAS → KSAN · 24 Nov` and the connection pill reads `Replay`. Under the header, a bar:

```
 ▶ REPLAY   [1×] [2×] [3×] [10×]   ❚❚ Pause   ■ Close
```

- **Speed** 1×, 2×, 3× or 10×, changeable at any time. Starts at 1× (Dave).
- **Pause / Resume.**
- **Close** closes the replay window.
- **Where it starts:** at the start of the lead-in (up to 30 minutes before block out), so the Pre-Flight stage and
  its GSX services replay too. 10× gets through it in 3 minutes.
- **Where it ends:** at block in Live shows that flight's Flight Summary, as a real flight does, with the bar's
  **Replay again** and **Close** in place of At Gate Turnaround and Done for now. The debrief's **Retry** is hidden:
  it calls Claude.
- **Times are the flight's own.** The block clock runs at the replay's speed; times show as they happened that day
  (`Block out 3:53 PM`).
- **Paused stretches are skipped:** where the log shows the sim paused (`paused` to `unpaused`), the replay's clock
  jumps over it, so a 20-minute coffee break doesn't play.
- **The planned flight** shown on Live is the replayed flight as it was planned, from the database (its row, cabins
  and stored plan). It reads `Replay` where a real one reads `Recording`, and has no buttons except Copy route.

## How it's built

**The replay process (`app_main.cpp`, `--replay`).** It builds only what Live needs to display:

- no recorder thread and no single-instance mutex, so it runs beside a recording copy;
- no `AirlineSession` (no migrations, lease settling or config import): the airline database is opened with
  `SQLITE_OPEN_READONLY`;
- no `FlightWatcher`, `PlannedFlightModel`, `ReviewWriter`, `ClaudeReviewer`, `NewFlightModel` or planner;
- a `FlightListModel` on the read-only database, for the summary at block in;
- a `ReplaySession` and a `ReplayWindow.qml` root in place of `Main.qml`.

The one thing it writes is the "started" banner in `errors.log`, which every BlockTime process writes.

**One bridge class.** The replay's bridge is another `RecorderBridge`: the bridge holds logic the dashboard depends on
(the stages, the Mach switch, the event feed), and a second class would be a copy of it. The replay calls the same
`apply*` methods `BridgeObserver` calls, on the GUI thread. Replay again makes a fresh bridge, so a replay always
starts clean.

**What each log line becomes.** The replayer hands the bridge what the recorder handed it on the day:

- `telemetry` → `applySample`, through `telemetryFromJson` (the inverse of the logged form). Fields the log doesn't
  have (per-engine fuel flow; an older flight's Mach) are zero.
- `block_out`, `block_in` → `applyBlockEvent`, and the feed's `BLOCK OUT` / `BLOCK IN` line.
- Lines only the log gets (`cockpit`, `telemetry_invalid`, `paused`, `unpaused`, `pmdg_data`, `probe_*`,
  `recorder_*`, `sim_*` other than `sim_running` / `sim_stopped`, `simconnect_exception`, `warning`,
  `aircraft_loaded`, `block_out_refused`, `airport_runways`) → nothing. Airports are only read by the gate assigner.
- Everything else (the flight events, GSX, `atis`, `atc_exchange`, `arrival_gate`, `sim_running` / `sim_stopped`) →
  `applyFlightEvent`, and its feed line. A new flight event replays without changes to the replayer.

The feed lines come from the same core functions the recorder uses (`feed_text.h`), so a replay's feed reads like the
day's.

**The replayer (`replay.h`, in `blocktime-core`, plain C++, tested).**

- Reads the flight's lines with `loadFlightEvents`, from the day files starting the day before block out (the
  lead-in can cross UTC midnight), not limited to the last 7 or 30 files.
- **The timeline:** each line's offset from the replay's start, with paused stretches taken out.
- **The clock:** the offset advances by the wall time since the last tick times the speed; speed changes and pauses
  take effect from the next tick. It returns the lines now due.
- **Seeking** (`--at` only): the offset of a phase's `phase_changed`, of block out, or of a time after block out.

**`ReplaySession` (Qt).** Owns the replay bridge, the read-only planned flight and a timer about every 50 ms (enough
for 10 samples a second at 10×; it stops when paused and at block in). Each tick it hands the due lines to the bridge.

**The read-only planned flight.** Live reads the planned flight's properties (the header, the state, the summary's
flight id). A QML-facing base class, `PlannedFlightDisplay`, holds those properties and their formatting.
`PlannedFlightModel` (the real one, which settles flights) and `ReplayPlannedFlight` (built from the settled flight,
writes nothing) both derive from it. Live and `PlannedFlightPanel` take the base.

**One clock for Live (D-166).** `FlightPanel.qml` used `Date.now()` for the block clock, and `EventFeedModel` and the
shutdown checks stamped lines with the wall clock. They all read the bridge's clock now: the wall clock on the real
bridge, the replay time on a replay bridge. **From here on, nothing on Live reads the wall clock directly** (a rule
for the dashboard milestones).

**For Claude Code:** `BlockTime.exe --replay <flight id | last> [--speed N] [--at <phase | block_out | H:MM>]
[--grab <file>]`. `--at` feeds every line up to that point at once (the only place a replay jumps ahead); `H:MM` is
after block out. With `--grab` the replay holds there, saves a screenshot after 6 s and quits, like `--demo --grab`.

**Test data.** The debrief fixture (`tests/data/flight-klas-ksan/`) strips position, fuel and the title from its
telemetry. The replay's tests use a slice in the full format.

## Later, not in this milestone

- **Another ATC system** (BeyondATC, Dave's idea). ATC doesn't come through the bridge: `FlightWatcher` polls
  SayIntentions and writes `atis` and `atc_exchange` lines to the log, which the app and a replay read back. Another
  ATC system would be another source behind `FlightWatcher`. The one thing to protect now: `atc_exchange` keeps
  SayIntentions' entry verbatim, so dashboard code should read ATC through one small function that gives a plain
  form (time, station, frequency, ATC's words), not SayIntentions' fields directly (D-168). Then a second source only
  has to fill that form.
