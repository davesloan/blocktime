# Replaying a flight (milestone 11)

The design for **flight replay**, from the brainstorm with Dave on 2026-11-24, after milestone 10 was built. Nothing
here is built yet. Approved by Dave the same evening. Decisions D-163 to D-168.

## Intent

Dave wants to watch the dashboard move without flying. A recorded flight plays back on Live, sped up, the way it
looked in the sim. It's the test bench for the dashboard milestones after it (`roadmap.md`): each change to the
dashboard is checked against real flights, on the desktop or the laptop, with no sim.

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

### Anti-goals

- **A replay changes anything.** No log lines, no database writes (money, location, fuel, hours, debrief), no calls
  to SayIntentions (`assignGate`, ATIS, the ATC log), SimBrief or Claude.
- **A replay costs a real flight.** The recorder keeps running underneath, and a real flight always wins.
- Weight next to the sim: within the usual budget (under 100 MB, under 1% CPU).

## Approaches considered

| Approach | Trade-off | |
|---|---|---|
| **A. A second, replay-only `RecorderBridge` fed from the log** | Today's code from the bridge up; safe by wiring, since nothing that writes or calls out listens to it | **Chosen (Dave)** |
| B. Re-run the recorder over the logged telemetry | Would replay recorder changes too, but the log holds about one sample a second, on change, and none of the frame samples, PMDG broadcast, GSX variables or facility data the recorder reads: the touchdown, GSX and landing would replay wrong. Much bigger | |
| C. One bridge with a "replay" switch | Least wiring, but every piece that writes or calls out must check the switch, now and in every later milestone; one forgotten check posts money | |

## What Dave sees

**The Replay button.** On every Flight Summary of a settled flight. Greyed out, with the reason under it, when:
- the flight's log files are gone: `The log for 24 Nov is no longer on this PC`;
- a real flight is open (block out to block in): `Not while a flight is in progress`.

**The replay.** Replay switches to Live and plays the flight there. A bar across the top of Live:

```
 ▶ REPLAY  XLY1231 KLAS → KSAN · 24 Nov   [1×] [2×] [3×] [10×]   ❚❚   ■ Stop
```

- **Speed** 1×, 2×, 3× or 10×, changeable at any time. Starts at 1× (Dave).
- **Pause / Resume** (Claude's call: nearly free).
- **Stop** goes back to that flight's summary on Flights.
- **Where it starts** (Claude's call): at the start of the lead-in that `loadFlightEvents` reads (up to 30 minutes
  before block out), so the Pre-Flight stage and its GSX services replay too. 10× gets through it in 3 minutes.
- **Where it ends:** at block in Live shows that flight's Flight Summary, the way a real flight does (D-134, later
  D-154), but without At Gate Turnaround and Done for now, which act on the real airline. In their place: **Replay
  again** and **Back to Flights** (Claude's call). The debrief's **Retry** is hidden too: it calls Claude.
- **Times are the flight's own** (Claude's call). The block clock runs at the replay's speed; times show as they
  happened that day (`Block out 3:53 PM`).
- **Paused stretches are skipped** (Claude's call): where the log shows the sim paused (`paused` to `unpaused`), the
  replay's clock jumps to the unpause, so a 20-minute coffee break doesn't play.
- **The planned flight** shown on Live is the replayed flight's stored plan, read from the database.
- **The connection** reads `Replay`, not `Connected` or `Waiting for sim` (Claude's call).
- **Things that only make sense live** stay hidden: the Not Recording banner, Cancel flight, the plan-import buttons.
- **A real block out during a replay** stops the replay; Live switches back to the real flight.

## How it's built

**One bridge class, two instances (Claude's call).** The replay bridge is another `RecorderBridge`, not a second class
with the same interface. The bridge itself holds logic the dashboard depends on (the stages, the Mach switch, the event
feed); a second class would be a second copy of it, and the replay would test the copy. The recorder never sees the
replay bridge: the replayer calls the same `apply*` methods `BridgeObserver` calls, on the GUI thread. A new replay
bridge is made for each replay and thrown away at Stop, so a replay always starts clean.

**Live binds to whichever bridge is active.** `Main` gets the real bridge and a `ReplaySession`; Live's `recorder`
is the replay bridge while a replay runs, the real one otherwise.

**A read-only planned flight for the replay.** Live also reads `PlannedFlightModel` (the header, the summary state),
which settles flights. The replay gets its own small read-only model presenting the properties Live reads, built from
the settled flight's row and its stored plan (`ofp_json`). Here Dave's "same interface" idea is the right one: a
second class, not a replay mode inside `PlannedFlightModel`, for approach C's reason.

**The replayer (in `blocktime-core`, plain C++, tested).**
- Reads the flight's lines: the day files from the lead-in's start to the block-in day (a flight can cross UTC
  midnight), found from the flight id (the block-out time), not limited to the last 7 or 30 files.
- Turns each line back into what the recorder sent: `telemetry` to a `TelemetrySample`, block events to a
  `BlockEvent`, `airport_runways` to an `AirportFacilities` (runways only: parking spots aren't logged), and every
  other line to a flight event (type and data), as `BridgeObserver::onFlightEvent` does. Fields a log doesn't have
  (an older flight's Mach, its cockpit events) are left at zero or empty, not an error.
- A replay clock: the replay time advances by the wall time since the last tick times the speed; speed changes and
  pauses take effect from the next tick; paused stretches in the log are skipped. It returns the lines now due.

**`ReplaySession` (Qt).** Owns the replay bridge, the read-only planned flight and a timer (Claude's call: about
every 50 ms, enough for 10 samples a second at 10×; nothing runs when no replay does). Each tick it hands the due
lines to the replay bridge. It stops itself when the real bridge reports `block_out`.

**One clock for Live.** Today `FlightPanel.qml` uses `Date.now()` for the block clock and `EventFeedModel` stamps
lines with the wall clock. Both move to a `now` on the bridge: the wall clock on the real bridge, the replay time on
the replay bridge. **From here on, nothing on Live reads the wall clock directly** (a rule for the dashboard
milestones).

**Safety by wiring, checked by a test.** `FlightWatcher`, `PlannedFlightModel`, `ReviewWriter` and the gate assigner
stay connected to the real bridge only. A test replays a whole fixture flight to block in and checks that the
database is unchanged, no log file was written, and fake `Planner` and review clients got no calls.

**For Claude Code:** `BlockTime.exe --replay <flight id> [--speed N] [--at <phase or mm:ss>] [--grab <file>]`.
`--at` feeds every line up to that point at once (the only place a replay jumps ahead), then plays; `--grab` saves a
screenshot there and quits, like `--demo --grab` (Claude's call).

**Test data.** The debrief fixture (`tests/data/flight-klas-ksan/`) strips position, fuel and the title from its
telemetry. The replay's tests need a slice in the full format: the same leg, trimmed but keeping every field.

## Later, not in this milestone

- **Another ATC system** (BeyondATC, Dave's idea). ATC doesn't come through the bridge: `FlightWatcher` polls
  SayIntentions and writes `atis` and `atc_exchange` lines to the log, which the app and a replay read back. Another
  ATC system would be another source behind `FlightWatcher`. The one thing to protect now: `atc_exchange` keeps
  SayIntentions' entry verbatim, so dashboard code should read ATC through one small function that gives a plain
  form (time, station, frequency, ATC's words), not SayIntentions' fields directly. Then a second source only has to
  fill that form.

## Open for the builder

- Exact names and file layout, the timer interval, how the bar looks in `Theme.qml`.
- Whether the read-only planned flight shares a QML-facing base with `PlannedFlightModel` or just matches its property
  names.
