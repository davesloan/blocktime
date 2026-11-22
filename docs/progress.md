# Progress log

Newest entries at the top. Add an entry at the end of every working session.

## Template

```
## YYYY-MM-DD — <short title>

**Done:**
- ...

**Working / verified:**
- ...

**Unverified or broken:**
- ... (include what Dave should test in the sim and the expected result)

**Next step:**
- ...
```

---

## 2026-11-22 — Recorder 0.2.0 sim test at KOAK

**Done:**
- Dave ran the 0.2.0 checklist in one MSFS load: a PMDG 737-800 pattern from a GSX gate, then the 777-300ER at a
  gate. Log: `events-2026-11-22.jsonl`, from line 450.
- New observations are in `docs/notes/msfs-observed-behaviour.md`.

**Working / verified in the sim:**
1. **Push:** exactly one `block_out`, timed at the push release (16:42:53.237). `fuel_lb` 15,761.7 matches the
   release sample, after GSX refuelling. It was confirmed 11 s later at 11.1 m. The set after the push and the
   release to taxi made no events.
2. **Hold short:** skipped (Dave forgot). Still covered by the unit tests.
3. **Taxiway stop after landing:** set and released with the engines running. No `block_in`.
4. **Gate:** `block_in` with `time` = the set (17:06:13.713), `shutdown_time` = the last engine off (17:06:15.671)
   and `block_time_s` 1400, which equals in − out. Fuel at the set and at shutdown are both right.
5. **777 loaded with the brake off:** no `block_out`, across two invalid stretches during the load.
6. **777 engines:** `eng_running` went `[false,true]` → `[true,true]` → `[true,false]` → `[false,false]`. No
   `block_in`, because it didn't fly.
7. **Ctrl+C:** `sim_disconnected` (`recorder_exit`), then `recorder_stopped`. The restart reconnected at once. The
   exit code itself wasn't captured, but the exit path ran to the end.
8. **777 push, then MSFS ended:** `block_out` (13.8 m), then `flight_interrupted` with `sim_process_gone`.
   Dave force-terminated MSFS rather than quitting from the main menu, so that is the correct reason. This also
   checks the crash path.

**Unverified or odd:**
- The hold-short set/release (step 2) hasn't been seen in the sim.

**Dave's answers to the milestone 2 open questions (D-046):**
- The recorder never resumes a flight. Exiting to the main menu means the flight is over, and an open one should
  be logged as interrupted. The web app discards or completes interrupted flights.
- Turnarounds happen in the sim: shut down, then either exit to the main menu or deboard, refuel, board, restart
  and fly the next flight. Flights are often chained. The parking brake stays set from block in until the next
  push, so a block-out candidate can't start early, and the "candidate never expires" review item doesn't need
  fixing.
- D-042, D-043 and D-045 are confirmed (Accepted). D-044 is replaced by D-046.

**Next step:**
- Milestone 3, re-scoped by D-046 (see `docs/recorder.md`): end an open flight at the main menu, close a flight
  left open by a recorder crash on the next start, and surface failed block-event writes.

## 2026-11-22 — Recorder milestone 2: block out / block in (0.2.0)

**Done:**
- Plan: `docs/plans/2026-11-22-recorder-milestone-2.md`.
- **Unit tests (D-041):** GoogleTest, a `blocktime-core` library, and `ctest --preset x64-release`. 33 tests.
- **Block detection:** `BlockDetector` implements block out (D-031) and block in (D-032), with the details in
  D-042 and D-045.
- **New events (D-043):** `block_out`, `block_in` and `flight_interrupted`.
- **Lost session (D-044):** an open flight is logged as `flight_interrupted` and forgotten. Resume comes in
  milestone 3.
- **Code review** (requesting/receiving-code-review), two findings fixed:
  - A single `on_ground=false` sample could lose the flight or leave it stuck open.
  - Brake state survived a trip to the main menu, so a flight loaded with the brake off counted as a release.

**Working / verified (without the sim):**
- Release and debug builds have no warnings, and 33/33 tests pass in both.
- With MSFS closed:
  - The exe starts and logs `recorder_started`.
  - A second copy exits with code 3.
  - `--help` works.
- Not checked: Ctrl+C exiting with code 0. It couldn't be signalled from Claude's shell; only one call was added to
  the exit path.

**Decisions for Dave to confirm:**
- D-042 (detection details), D-043 (event fields: fuel at the set **and** after shutdown), D-044 (lost session) and
  D-045 (flicker / invalid stretches) are Proposed.
- **Open question 1:** a flight that is open when you go to the main menu, or load another flight, without
  quitting MSFS. The recorder keeps it open, which has two effects:
  - The next flight's push is swallowed.
  - Its landing and shutdown close the old `flight_id`, with the old block-out time and fuel. If the old flight
    had taken off, a brake set at the new gate with the engines off even gives an immediate `block_in`.
  - Options: treat "back to the main menu" as `flight_interrupted`, or interrupt only when the position jumps
    when the next flight loads. How often do you leave a flight open like that?
- **Open question 2:** at the gate, do you ever release the brake after shutdown (chocks in) and then push the
  next flight without setting it again? If so, block out would be timed at that early release, with fuel from
  before refuelling.
- A return to the gate without taking off leaves the flight open, with the same effect as question 1.

**Unverified (Dave to test in the sim with 0.2.0):** one MSFS load covers everything. Quitting MSFS is the
last step. Start the recorder first, then fly a KOAK pattern in the PMDG 737 from a GSX gate.

*PMDG 737, KOAK pattern:*
1. **At the gate:** startup profile set and release, then GSX set, release for the push, set after the push, and
   release to taxi.
   - Expect exactly **one** `block_out`, timed at the push release.
   - Its `fuel_lb` should equal the fuel at that moment.
   - The console prints `BLOCK OUT`.
2. **Hold short:** set and release the brake before takeoff. Expect no new events.
3. **After landing:** set and release at a taxiway stop. Expect no `block_in`.
4. **At the gate:** set the brake, then shut down both engines. Expect one `block_in`:
   - `time` = the moment of the brake set
   - `shutdown_time` = when the last engine stopped
   - `block_time_s` = block in − block out
   - The console prints `BLOCK IN`.

*Main menu, then the PMDG 777 at a KOAK gate (no sim restart):*

5. Load the 777 at a gate with the parking brake **off** at load. Expect no `block_out`: a brake that is already
   off at load isn't a release.
6. Set the brake, start the engines, then shut them down with the brake still set. Expect `eng_running` to go all
   false. This hasn't been seen on the 777 before. There's no `block_in`, because this flight didn't take off.

*Recorder restart (MSFS keeps running):*

7. Press Ctrl+C in the recorder window. Expect `sim_disconnected` (`recorder_exit`), then `recorder_stopped`, and
   no error. Start the recorder again; it reconnects.

*Last, because it ends the sim session:*

8. On the 777, release the brake and taxi at least 10 m. Expect `block_out`. Then quit MSFS. Expect
   `sim_disconnected` (`sim_quit`), then `flight_interrupted` with `sim_quit`.

**Deferred review items:**
- A failed write of a block event is only reported on stderr (the 0.1.2 deferred item; it matters more now).
- A block-out candidate never expires (see open question 2).

**Next step:**
- Dave runs the sim checklist and answers the open questions. Then milestone 3 (durable log and resume of an open
  flight), which is also where open question 1 is settled.

## 2026-11-22 — Planning: flight analysis milestone

**Done:**
- Added recorder milestone 7, **procedure capture**, to `docs/recorder.md`. Delivery to the app is now
  milestone 8.
- Added the end-of-flight analysis by Claude (0–100 score) to `docs/design.md` section 13 (D-040).
- Limited aircraft support to the PMDG 737 and 777 on MSFS 2020 (D-039).

**Next step:**
- Milestone 2 (block out / block in) is still next. Brainstorm what to capture at the start of milestone 7.

## 2026-11-22 — Second sim test (recorder 0.1.1)

**Sim test (PMDG 737-800, then PMDG 777-300ER, KSAN gate 32):** the details are in
`docs/notes/msfs-observed-behaviour.md`.

**Working / verified in the sim:**
1. Recorder started before MSFS: no `sim_disconnected` while MSFS started. One `sim_connected` with
   `process_watch: true`.
2. `aircraft_loaded` logged once per aircraft, with no empty paths and no duplicate telemetry. The 737 fuel was
   restored to 9,001 lb shortly after Ready to Fly.
3. Pause flags captured: `8` for the Escape menu, `4` for active pause, `1` for the menu and loading screens.
4. Loading screens: `sim_unresponsive` / `sim_responsive` only. Long stays at the main menu were quiet.
5. 737 engines: `eng_running` and `fuel_flow_pph` followed start and shutdown. `engines` = 2.
6. Quitting MSFS logged `sim_disconnected` with `sim_quit`.
8. The 777's fuel reads correctly.

**Open issues (debugging):**
- **A. A flight reload isn't logged: won't fix (D-034).** The 777 was loaded, PMDG failed, and Dave reloaded with
  Escape → Reload around 07:00:45. Only a `sim_stopped`/`sim_running` pair shows the reload, because
  `logAircraft()` skips a path equal to the last one logged (D-029). Dave only reloads at the gate, when PMDG fails
  to load, so this is accepted.
- **B. 777 parking brake: not a bug.** Dave set it once and never released it, which matches the log (set at
  07:02:17).

**Not tested:** 7 (ending `FlightSimulator.exe` mid-flight) and engine shutdown on the 777. Default aircraft no
longer need testing (D-039).

**Done after the test (recorder 0.1.2):**
- `paused` now has `kinds` with the `Pause_EX1` bits named (`full`, `active`, `sim`, ...), alongside the raw
  `flags` (D-035, Proposed). Release and debug build with no warnings. The new field hasn't been seen in the sim
  yet: on the next run, Escape should log `"kinds":["sim"]` and active pause `"kinds":["active"]`.
- **Code review fixes:**
  - A torn last line in the log is repaired when the log is opened (D-036).
  - Only one recorder can run at a time (D-037).
  - Messages that already arrived are processed before a sim crash ends the session.
  - The vcpkg version now matches (0.1.2).
- **Review items left for later:**
  - Logoff and shutdown don't write `recorder_stopped`: console handlers don't get those events once `user32` is
    loaded. The tray app will fix this.
  - Non-ANSI log paths make `path::string()` throw.
  - An event that fails to write is only reported on stderr, not queued and retried.
  - The `FlightSimulator.exe`-not-found warning isn't logged if the handshake times out.
  - There's no delay before reconnecting after a lost session.
  - The README doesn't list a few minor fields (`skipped_invalid`, the exception `send_id`/`index`).

**Next step:**
- Milestone 1 is merged to `main` (no pull request, D-038). Next is milestone 2 (block out / block in, D-031,
  D-032), with a written plan and TDD (D-033).
- D-035, D-036 and D-037 were approved by Dave.

## 2026-11-22 — First sim test; milestone 1 fixes (recorder 0.1.1)

**Sim test results (PMDG 737-800, KSAN gate 32, recorder 0.1.0):**
- Works: PMDG parking brake and fuel weight read correctly. The junk filter caught the main menu (0/0). A parked
  aircraft stays quiet. Pause is detected for both the Escape menu and active pause.
- Found: false `no_response` disconnects while MSFS was starting and on loading screens. There was an unexplained
  `UNRECOGNIZED_ID`. `aircraft_loaded` came with empty or duplicate paths. Telemetry lines were duplicated. The
  pause type wasn't recorded. The README run step assumed the wrong folder.
- Learned: details are in `docs/notes/msfs-observed-behaviour.md`, mainly that PMDG restores its saved fuel about
  60 s after load, and that 737 AGL reads about 8 ft on the ground.

**Done:**
- **Connection (D-028):**
  - The recorder waits for SimConnect's OPEN reply before counting a session as connected.
  - A sim crash is detected by watching the `FlightSimulator.exe` process.
  - Silence now only logs `sim_unresponsive` / `sim_responsive`.
  - `sim_disconnected` is only logged after a `sim_connected`.
- **Exceptions:** each `simconnect_exception` now names the failing call (`call`). An `UNRECOGNIZED_ID` from an
  early snapshot request is logged as ignored.
- **Less noise (D-029):** `aircraft_loaded` is deduplicated, and identical telemetry samples are skipped.
- **Pause (D-030):** pause now uses `Pause_EX1`, with the raw `flags` logged.
- **Engine data:** telemetry now has `engines`, `eng_running` per engine and `fuel_flow_pph`. These are needed for
  the block-in rule.
- **Block rules recorded (D-031, D-032):** Dave decided both; they will be implemented in milestone 2.
  - Block out = the brake release that leads to 10 m or more of movement, timed at the release.
  - Block in = a brake set after landing that stays set until engine shutdown.
- **Docs:** the README run step was fixed, the new events were documented, and the version went to 0.1.1.

**Working / verified (without the sim):**
- Release and debug builds have no warnings at /W4.
- With MSFS closed, Ctrl+C exits with code 0, and no `sim_disconnected` is logged.

**Unverified (Dave to test in the sim with 0.1.1):**
1. Start the recorder **before** MSFS. There should be no `sim_disconnected` lines while MSFS starts. Then one
   `sim_connected` with `"process_watch": true`.
2. Load the PMDG 737 at a gate. Expect:
   - `aircraft_loaded` logged once, with no empty path, and no duplicate telemetry lines.
   - About 60 s after load, `fuel_lb` should be about **9,000** if PMDG remembers the last FMC fuel.
3. Pause with Escape, then with active pause. Note the `flags` value logged for each.
4. Go back to the main menu, load another flight, and wait on the loading screen. You may see `sim_unresponsive` /
   `sim_responsive`, but **no** `sim_disconnected`.
5. Start the engines, then shut them down. `eng_running` should go `[true,true]` and back to `[false,false]`,
   `fuel_flow_pph` should rise and fall, and `engines` should be 2.
6. Quit MSFS normally. It should log `sim_disconnected` with `sim_quit`.
7. Optional: end `FlightSimulator.exe` in Task Manager during a flight. It should log `sim_disconnected` with
   `sim_process_gone` within a second or two.
8. Still pending from the first checklist: fuel and parking brake on a default aircraft and the PMDG 777.

**Next step:**
- Review the 0.1.1 log, name the `Pause_EX1` bits, commit milestone 1, then start milestone 2 (block out / block in
  per D-031 and D-032).

## 2026-11-21 — Recorder milestone 1: connect and read

**Done:**
- `/recorder` now builds: a CMake + presets + vcpkg (nlohmann-json) C++20 console app, `blocktime-recorder.exe`.
- Connects to MSFS with SimConnect (statically linked), retries with back-off, and detects a lost connection
  (QUIT / errors / heartbeat silence).
- Reads parking brake, on ground, fuel (lb + gal), lat/lon, MSL/AGL/indicated altitude, VS, GS, IAS, title and
  ATC ID once a second, only when values change. Junk samples are filtered out.
- Logs everything to daily JSONL files in `%LOCALAPPDATA%\BlockTime\events`.
- Compared the plan against Little Navmap's SimConnect code and adjusted it. Notes are in `docs/notes/`.
- Decisions D-019 to D-027 added; all approved by Dave.
- CLAUDE.md now says VS "2022 or newer", and `/app` was renamed to `/webapp` to match the real folder.

**Working / verified (without the sim):**
- Release and debug builds compile with no warnings at /W4.
- With MSFS closed it logs `recorder_started`, waits, and Ctrl+C exits with code 0 after logging `recorder_stopped`.
- `--help` works, and an unknown argument exits with code 2.

**Unverified (Dave to test in the sim):**
1. Start the recorder, then MSFS. It should log `sim_connected` within ~10 s of MSFS starting, and **no**
   `telemetry` lines at the main menu (at most `telemetry_invalid`).
2. Load a flight at a gate. It should log `sim_running`, `aircraft_loaded`, then `telemetry` with the right title,
   ATC ID, lat/lon, `on_ground: true` and the parking brake state.
3. Toggle the parking brake. A telemetry line should appear within ~1 s with the new `parking_brake`.
4. Compare `fuel_lb` / `fuel_gal` with the EFB or SimBrief load. Test the default aircraft **and** the PMDG
   737/777, and repeat the parking brake check on PMDG.
5. Pause and unpause. It should log `paused` and `unpaused`.
6. Go back to the main menu, then quit MSFS. It should log `sim_stopped`, then `sim_disconnected` (reason
   `sim_quit`), and go back to waiting.
7. Check that the junk filter isn't too strict or too loose during loading. Bring the JSONL file back so the
   thresholds can be tuned.
- Also unverified: whether the "Pause" event fires as expected in MSFS 2020; whether a sim crash is detected by the
  30 s silence timeout; and whether a parked aircraft really stays quiet with the CHANGED thresholds.

**Next step:**
- Review the JSONL from the in-sim test, then start milestone 2 (block out / block in events with time, position
  and fuel).

## 2026-11-21 — Design complete, repo created

**Done:**
- Product design worked out with Dave on a whiteboard and written up in `docs/design.md`.
- Recorder spec written in `docs/recorder.md`.
- Initial decisions recorded in `docs/decisions.md` (D-001 onward).

**Working / verified:**
- Nothing built yet.

**Next step:**
- Recorder milestone 1: create `/recorder` with a CMake + vcpkg C++ project, locate the MSFS 2020 SimConnect SDK,
  connect to the sim, read a few SimVars (parking brake, on-ground, fuel weight, position), and log them.
  Write `/recorder/README.md` with build and run steps.
