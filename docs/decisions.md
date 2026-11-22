# Decision log

Append-only. Never delete or rewrite an entry; if a decision changes, add a new entry and mark the old one
`Superseded by D-NNN`. Log decisions made by Dave **and** decisions Claude makes on its own while implementing.

## Template

```
## D-NNN — <short title>
- Date: YYYY-MM-DD
- Status: Accepted | Proposed | Superseded by D-NNN
- Decided by: Dave | Claude | Dave + Claude
- Area: recorder | app | design | build | data
- Decision: <what was decided, one or two sentences>
- Why: <the reason>
- Alternatives considered: <what else was on the table, briefly>
```

`Proposed` = Claude made the call without Dave's explicit sign-off; mention it in the end-of-task summary.

---

## D-001 — Two programs: native recorder + local web app
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: A native Windows recorder (C++, SimConnect SDK) runs as its own process; a Node.js/TypeScript web app runs
  locally and owns all airline logic and visuals.
- Why: The recorder keeps working even if the web app restarts; native code keeps resource use minimal next to MSFS.
- Alternatives considered: node-simconnect inside the Node app (one language, but recorder dies with the app).

## D-002 — Recorder in C++
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: Write the recorder in C++ (MSVC, CMake).
- Why: Smallest footprint, no runtime; can borrow from Little Navmap's C++ SimConnect code (GPL-3.0 is acceptable since
  this project is open source).
- Alternatives considered: C# with Microsoft's managed SimConnect wrapper (faster to write, larger runtime).

## D-003 — Build the recorder first
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: Implement and verify the recorder before the web app.
- Why: Everything else depends on reliable flight data.

## D-004 — Runs locally, no hosting or login
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: The web app runs on Dave's PC (localhost). Single SQLite file for all data.
- Why: Personal use; no need for accounts or hosting; backup by copying a file.
- Alternatives considered: AWS-hosted backend (original idea).

## D-005 — Flight boundaries are parking brake release/set
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: A flight starts at parking brake release at the gate and ends at parking brake set at the destination.
  Block time is measured between these.
- Why: Matches how Dave defines block time for rotations.
- Refined by D-031 (which release counts) and D-032 (which set counts).

## D-006 — Plan-aware, forward-only phase detection
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: recorder
- Decision: Climb ends only near the SimBrief planned cruise altitude; phases don't go backwards (except go-around);
  level-offs are sub-states; transitions are debounced. Details in `recorder.md`.
- Why: Avoid FlyHub's misleading climb→cruise→climb flip-flopping during SID level-offs.

## D-007 — Visuals live in the web app
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: The recorder has only a tray icon/status; it detects phases and streams events; the web app shows phases,
  loading and progress.
- Why: Keep the recorder tiny; Dave runs the web app during flights anyway.

## D-008 — Write events locally first
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: recorder
- Decision: Every event is appended to a local log file before any delivery to the web app; the app catches up from the
  log if it missed anything.
- Why: Never lose a flight because the app wasn't running.

## D-009 — Sim crash handling
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: recorder, app
- Decision: An interrupted flight can only be **discarded** (as if it never happened, plane back at origin) or
  **completed** (treated as a background flight, plane moved to destination).
- Why: Simple, forgiving, no partial-flight accounting.

## D-010 — Airline runs on real time with catch-up
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: app
- Decision: Background flights run in real time; the app replays each plane's schedule from its last known point when
  opened and every minute while open. No separate "active mode".
- Why: No always-on server needed; one piece of logic powers catch-up, live boards and future departures.

## D-011 — Rotations are timetable-free loops, one rotation many planes
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: design
- Decision: Rotations are reusable loops tied to one aircraft type; planes fly legs back to back with fixed turnaround
  (737 35 min, 777 60 min); planes join at whatever rotation airport they're parked at.
- Why: Fixed departure times created early/late/sync problems; loops remove them.

## D-012 — Wet leases only, paid monthly in advance
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: Aircraft are wet-leased monthly in advance, returnable any time with prorated refund. No purchases or loans.
- Why: The app won't run for years; leasing keeps fleet changes painless. Wet lease explains crew costs.

## D-013 — Fleet growth gated by hours Dave flies
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: Start with one aircraft slot; each further slot needs 10 more hours of Dave's own flying than the last.
- Why: Prevents leasing many planes and letting the airline run itself; makes Dave's flying the progression system.

## D-014 — No empty flights except factory delivery
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: Every flight between airports carries a paying load. The only empty flight is factory delivery, which can be
  flown by Dave or bought as instant delivery.
- Why: Free flights and one-off background flights already move planes while earning money.

## D-015 — No hubs, no airport leases, fixed fees
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: No hubs or gate leases. Airport-related fees are fixed rates by aircraft size, the same everywhere.
- Why: Avoid FlyHub-style complexity; real small airlines use common-use gates anyway.

## D-016 — Economic numbers (initial)
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: design
- Decision: Starting cash $5M; fares, cargo rate, costs, leases and loads as listed in `design.md` §5, §9, §11, §12.
- Why: Tuned so full daytime flights earn well, light night flights roughly break even, VIP never loses, and leases take
  about half of a busy plane's monthly profit. Values live in config for tuning.

## D-017 — Reference data
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: data
- Decision: Airports from OurAirports; time zones derived from coordinates; fare distance = great-circle distance.
- Why: Free public data; distance can't be gamed by routing.

## D-018 — Project name: BlockTime
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: The project is called **BlockTime**; the Windows app is "BlockTime Recorder".
- Why: The whole design is built on block time (parking brake release to parking brake set).
- Alternatives considered: Tailbook, Loopline.

## D-019 — Link SimConnect statically; use the DLL C runtime
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21)
- Decided by: Claude, approved by Dave
- Area: build
- Decision: Link `lib\static\SimConnect.lib`, so no `SimConnect.dll` ships next to the exe. Build with `/MD` and the
  vcpkg triplet `x64-windows-static-md`.
- Why: A single exe is simpler. The SDK's static lib is built with `/MD`, so `/MT` fails with LNK2038. The VC++
  runtime is always installed alongside MSFS.
- Alternatives considered: Dynamic `SimConnect.lib` + copying the DLL; a fully static CRT (impossible with this lib).

## D-020 — Event log: JSON Lines, one file per UTC day, in %LOCALAPPDATA%
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21)
- Decided by: Claude, approved by Dave
- Area: recorder, data
- Decision: Events are appended to `%LOCALAPPDATA%\BlockTime\events\events-YYYY-MM-DD.jsonl`, one JSON object per
  line, `{"ts": UTC ISO-8601 ms, "type", "data"}`, and flushed after every line. `--log-dir` overrides the folder.
- Why: Append-only, easy for the web app to read, still readable if a line is torn in a crash. Daily files keep
  each file small. Outside the repo, so rebuilds and `git clean` can't touch it.
- Alternatives considered: SQLite in the recorder (heavier, and the app owns the DB); one ever-growing file; a
  binary format.

## D-021 — nlohmann-json for JSON
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21)
- Decided by: Claude, approved by Dave
- Area: build
- Decision: Use `nlohmann-json` from vcpkg. `vcpkg.json` pins a `builtin-baseline`.
- Why: Header-only, widely used, MIT license. The VS-bundled vcpkg requires a baseline.
- Alternatives considered: Writing JSON by hand; RapidJSON; Glaze.

## D-022 — Console app first, tray icon later
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21)
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision: Milestone 1 is a console program that prints a short form of each event. The tray icon (D-007) comes
  later.
- Why: Easier to see and debug while sim behaviour is still being verified.

## D-023 — Telemetry: once a second, only on change, single thread
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21); the plan to read `ZULU TIME` at block events is dropped by D-042
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision: One data definition on the user aircraft, requested with `SIMCONNECT_PERIOD_SECOND` +
  `DATA_REQUEST_FLAG_CHANGED`. The thresholds are: lat/lon 0.00001°, altitudes 1 ft, VS 10 fpm, speeds 0.5 kt,
  fuel 1 lb / 0.1 gal. A one-off `PERIOD_ONCE` snapshot is requested on sim running, aircraft loaded and unpause,
  so a parked aircraft still produces a sample. The main loop drains `GetNextDispatch` and then sleeps 100 ms,
  on one thread. `ZULU TIME` is left out of the definition, because it changes every second and would defeat the
  CHANGED flag. It will be read on demand at block events (milestone 2).
- Why: The lightest load on the sim that still gives about 1 Hz live data while moving. Simpler than Little
  Navmap's 500 ms one-shot polling thread, which is built for AI traffic and live maps.
- Alternatives considered: Little Navmap-style polling; `PERIOD_SIM_FRAME` (only near touchdown, milestone 5).

## D-024 — Reconnect and connection-loss rules
- Date: 2026-11-21
- Status: Superseded by D-028
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision: Try `SimConnect_Open` every 10 s, then every 20 s after 30 min of waiting and every 40 s after 60 min.
  The session counts as lost on `QUIT`, after 10 SimConnect calls fail in a row, or when nothing at all arrives for
  30 s despite a `RequestSystemState("Sim")` heartbeat every 5 s. After a loss it closes and goes back to retrying;
  it never stops for good.
- Why: The back-off copies Little Navmap's workaround for leaked SimConnect handles (its issue #891). A crashed sim
  sends no QUIT, so a heartbeat is needed.
- Alternatives considered: A fixed 5 s retry; stopping after 10 errors (as Little Navmap does).

## D-025 — Junk-data filter for telemetry
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21)
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision: Reject samples unless the sim is running, the title is a valid non-empty string, altitude is within
  −2,000…200,000 ft and finite, and the aircraft isn't at 0/0 (±0.1°) below 5 kt. Log `telemetry_invalid` only
  when the reason changes. Details are in `docs/notes/msfs-junk-data.md`.
- Why: MSFS sends junk while loading. Later block-out detection must never fire on it. Thresholds from Little Navmap.

## D-026 — Little Navmap used as a reference only
- Date: 2026-11-21
- Status: Accepted
- Decided by: Dave + Claude
- Area: recorder
- Decision: We studied atools `simconnecthandler.cpp` / `datareaderthread.cpp` and littlenavmap `mapwidget.cpp`
  for ideas and thresholds. No code was copied. Notes are in `docs/notes/little-navmap-reference.md`.
- Why: Dave asked for the plan to be checked against Little Navmap. Copying wasn't needed for milestone 1.

## D-027 — Build with Visual Studio's bundled CMake/Ninja/vcpkg
- Date: 2026-11-21
- Status: Accepted (approved by Dave 2026-11-21)
- Decided by: Claude, approved by Dave
- Area: build
- Decision: `CMakePresets.json` (`x64-debug`, `x64-release`) uses Ninja and `$env:VCPKG_ROOT`, run from a VS
  Developer PowerShell. VS 2022 or newer is supported. Development is on VS 18 (2026), so CLAUDE.md now says
  "2022 or newer".
- Why: Nothing extra to install. Presets give copy-pasteable commands.

## D-028 — Connection lifecycle: handshake first, watch the process, silence only warns
- Date: 2026-11-22
- Status: Accepted (approved by Dave in the fix plan, 2026-11-22)
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision:
  - **Retry interval:** unchanged from D-024 (10 s, then 20 s after 30 min, then 40 s after 60 min).
  - **Handshake:** a session only counts as connected when SimConnect's OPEN reply arrives. Only then is
    `sim_connected` logged. Before that, the recorder waits without timing out.
  - **Process watch:** at open, the recorder finds `FlightSimulator.exe` and checks every loop whether it has
    exited. If it has, the session is lost with reason `sim_process_gone`.
  - **Other ways a session ends:** `QUIT`, or 10 failed calls in a row.
  - **Silence:** 30 s with no messages logs `sim_unresponsive`, and `sim_responsive` when messages return. Silence
    never ends a session.
  - **Exe not found:** if `FlightSimulator.exe` can't be found, a `warning` is logged, and the handshake wait is
    capped at 5 min.
  - **Log pairing:** `sim_disconnected` is logged only if `sim_connected` was.
- Why: In the first sim test, MSFS accepted the connection minutes before answering. It also didn't answer the
  heartbeat during loading screens. The 30 s silence timeout of D-024 caused 6 false disconnects, and some
  `sim_disconnected` lines had no matching `sim_connected`. A process check finds a crash right away, without
  guessing from silence.
- Alternatives considered: A longer silence timeout (still guesswork, and slow to notice a crash).

## D-029 — Less log noise: skip duplicate telemetry; aircraft_loaded only on change
- Date: 2026-11-22
- Status: Accepted (approved by Dave in the fix plan, 2026-11-22)
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision:
  - **Duplicate telemetry:** a valid sample that is byte-identical to the last one logged is skipped. The first
    good sample after an invalid stretch, or after a reconnect, is always logged.
  - **`aircraft_loaded`:** empty paths are skipped. Otherwise it is logged only when the normalized path changes.
    The path is normalized by lower-casing it and keeping only the part from `simobjects\` on. The raw path is
    kept in the log.
- Why: In the first test, the snapshot request and the periodic request produced identical lines. The same
  aircraft was also reported in two path formats, sometimes with an empty path.

## D-030 — Pause via Pause_EX1, raw flags logged
- Date: 2026-11-22
- Status: Accepted (approved by Dave in the fix plan, 2026-11-22)
- Decided by: Claude, approved by Dave
- Area: recorder
- Decision: Subscribe to `Pause_EX1` instead of `Pause`. Log `paused` with `"flags"` set to the raw value, again
  whenever the non-zero value changes, and log `unpaused` when it returns to 0. The bits will be named in code
  once a sim test has confirmed them. From memory of the MSFS docs: 1 = full pause, 4 = active pause, 8 = sim
  paused.
- Why: `Pause` fired for both the Escape menu and active pause, with no way to tell them apart.

## D-031 — Block out: the brake release that leads to movement
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision:
  - **Candidates:** before a flight is open, every parking brake release on the ground is a candidate.
  - **Confirmation:** a candidate counts once the aircraft has moved **≥ 10 m** from where the brake was released,
    with the brake still off.
  - **Dropped candidates:** if the brake is set again first, the candidate is dropped.
  - **Block-out time:** the time of the confirmed release, like ACARS OUT.
  - **After block out:** once the flight is open, further sets and releases don't matter. That covers after
    pushback, during taxi and at hold short.
- Why: The brake is toggled several times at the gate. The PMDG startup profile may set it, Dave sets it for GSX,
  GSX asks for it again after pushback, and he may set it at hold short. Only the release that leads to the
  aircraft actually leaving the gate is block out.
- Alternatives considered: First movement, ignoring the brake. Timing block out from the start of movement rather
  than from the release.
- Implemented in milestone 2. The 10 m threshold may be tuned.

## D-032 — Block in: brake set, then engines shut down
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision:
  - **Rule:** after a landing, block in is a parking brake set while stopped that **stays set until all engines are
    shut down**.
  - **Block-in time:** the time of that set.
  - **Ignored sets:** a set that is released again while an engine is still running is ignored, e.g. at hold
    short or while waiting for a gate.
- Why: Brake sets during taxi-in must not end the flight. Shutdown at the gate is the clear end of a flight.
- Alternatives considered: Brake set plus stopped for 2 minutes or more. That doesn't need engine data, but a long
  wait could end the flight early.
- Depends on `GENERAL ENG COMBUSTION:n`, now recorded in telemetry (`eng_running`), which still has to be verified
  on PMDG. Implemented in milestone 2.

## D-033 — Which Superpowers skills to use
- Date: 2026-11-22
- Status: Accepted; the branch and pull request part is superseded by D-038
- Decided by: Dave + Claude
- Area: build
- Decision: Use systematic-debugging now, for the milestone 1 sim-test fixes. From milestone 2 on, also use
  writing-plans, executing-plans, test-driven-development and verification-before-completion. Every milestone,
  starting with milestone 1, is built on its own branch and merged to `main` by pull request after
  requesting-code-review and finishing-a-development-branch. Use brainstorming only sometimes. Skip the
  worktree and multi-agent skills. The list is in `CLAUDE.md`.
- Why: Claude can't run MSFS, so logic that can be tested without the sim should have tests, and work done
  across sessions needs written plans. A reviewed pull request per milestone gives `main` a clean history of
  milestones. The worktree and agent skills add overhead to a one-person project.
- Alternatives considered: Following the plugin's default rule of invoking any skill that might apply. Not
  using the plugin at all.

## D-034 — Flight reloads are not tracked
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: Escape → Reload of the same aircraft isn't detected or logged. The `aircraft_loaded` dedup of D-029
  stays as it is.
- Why: Dave only reloads at the gate, when PMDG fails to load, so it's always before block out. Block out
  (D-031) needs a brake release plus movement, so a reload at the gate can't start or split a flight.
- Alternatives considered: A probe to log raw `AircraftLoaded` and `FlightLoaded` events on reload, then a
  `flight_loaded` event. Not worth the effort for a rare case at the gate.

## D-035 — Pause_EX1 bits named in the log
- Date: 2026-11-22
- Status: Accepted (approved by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder
- Decision: `paused` keeps the raw `flags` and adds `kinds`, a list of names for the bits that are set:
  1 = `full` (main menu, loading screens), 2 = `legacy`, 4 = `active` (active pause), 8 = `sim` (Escape menu).
  Unknown bits are logged as `bit_<value>`. Recorder version 0.1.2.
- Why: The 0.1.1 sim test confirmed 1, 4 and 8, which match the SDK's list (D-030 said to name them once
  confirmed). Keeping `flags` means old and new log lines can be read the same way. A list allows for several
  bits being set at once.
- Alternatives considered: Replacing `flags` with a single name. That would lose information if two bits were
  ever set together.

## D-036 — Repair a torn last line when opening the log
- Date: 2026-11-22
- Status: Accepted (approved by Dave, 2026-11-22)
- Decided by: Claude (from the milestone 1 code review)
- Area: recorder
- Decision: When the recorder opens a day's log file and the file doesn't end in `\n`, it writes a `\n` first.
  The torn fragment stays as one bad line, and the readers skip lines they can't parse (D-020). Lines are still
  flushed only to the OS cache, not forced to disk with `FlushFileBuffers`.
- Why: After an OS crash or power loss, the next `recorder_started` would otherwise be glued onto the fragment,
  and that's the line crash recovery relies on. A recorder crash on its own doesn't lose data, because the OS
  cache survives it.
- Alternatives considered: `FlushFileBuffers` after every line. That costs a disk write a second and only helps
  with OS crashes; it can be added for key events (block out and block in) later.

## D-037 — Only one recorder at a time
- Date: 2026-11-22
- Status: Accepted (approved by Dave, 2026-11-22)
- Decided by: Claude (from the milestone 1 code review)
- Area: recorder
- Decision: At startup, the recorder creates the named mutex `Local\BlockTimeRecorder`. If it already exists, the
  recorder prints a message and exits with code 3, before writing anything to the log.
- Why: Two recorders would append every event to the same file twice, which would give two block outs in
  milestone 2.
- Alternatives considered: A lock file next to the log. It can go stale after a crash; a mutex is released by
  Windows when the process exits.

## D-038 — No pull requests; work goes straight to main
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: build
- Decision: There are no pull requests and no feature branches. Work is committed to `main` and pushed. A code
  review (requesting-code-review / receiving-code-review) still runs before each milestone's final commit.
  `finishing-a-development-branch` is no longer used. This supersedes the branch and pull request part of D-033.
- Why: It's a one-person project; pull requests added steps without adding review that the review subagent
  doesn't already give.
- Alternatives considered: One branch and pull request per milestone (D-033), used once, for milestone 1.

## D-039 — Only the PMDG 737 and 777 on MSFS 2020 are supported
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: The recorder is built and tested only for the PMDG 737 and 777 families on MSFS 2020. All 737
  variants use the same variables, and so do all 777 variants, so one of each is enough to test. Default and other
  add-on aircraft aren't tested or supported. MSFS 2024 is out of scope.
- Why: These are the only aircraft the airline flies (design.md section 1). Testing others is wasted effort.
- Alternatives considered: Also testing a default aircraft, which earlier test checklists asked for. Dropped.

## D-040 — End-of-flight analysis and 0–100 score by Claude
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: app, recorder
- Decision: When a flight ends, the web app sends Claude, through the Claude API:
  - the recorder's events for the flight,
  - the SayIntentions ATC log,
  - the ATIS for the origin and destination airports.

  Claude returns a written analysis and a 0–100 score. Examples of what it checks: configured and stable by
  1,000 ft, beacon on before pushback, landing lights below 10,000 ft, seatbelt signs below 18,000 ft, and
  altimeter settings. The recorder gets a new milestone 7, procedure capture, which logs this cockpit state as
  facts; delivery to the app moves to milestone 8.
- Why: Dave wants feedback on how well each flight was flown. Keeping the recorder to facts and the judging in the
  app follows the recorder ground rules.
- Alternatives considered: None yet. The full list of checks, how they're scored, whether the score affects the
  airline, and how SayIntentions data is fetched will be brainstormed when milestone 7 starts.

## D-041 — GoogleTest and a core library for unit tests
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave (GoogleTest), Claude (layout)
- Area: build
- Decision: Unit tests use GoogleTest from vcpkg (`gtest`). Code that doesn't touch SimConnect or `windows.h`
  (event log, junk filter, geometry, block detection) builds as the static library `blocktime-core`. Both
  `blocktime-recorder.exe` and `blocktime-tests.exe` link it. `ctest --preset x64-release` runs the tests.
  `/external:W0` keeps third-party headers out of the `/W4` no-warnings rule. `BLOCKTIME_BUILD_TESTS=OFF` skips
  the tests.
- Why: Claude can't run the sim, so the flight logic has to be testable without it (D-033). GoogleTest is the most
  widely used framework and shows up in Visual Studio's Test Explorer.
- Alternatives considered: doctest (lighter, single header) and Catch2 v3.

## D-042 — Block detection details
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder
- Decision: These details fill in D-031 and D-032:
  - **Release / set:** a change in the brake between two valid samples. The first sample after start or after a
    lost session only sets the baseline, so a brake that is already off is never a release.
  - **Block-out candidate:** only a release on the ground counts. Movement is the haversine distance from the
    release point; it is confirmed at ≥ 10 m with the brake off and on the ground.
  - **Airborne:** `SIM ON GROUND` false **and** `PLANE ALT ABOVE GROUND` ≥ 100 ft. **Landed:** on the ground
    after being airborne.
  - **Block-in candidate:** only a set on the ground after landing counts. Speed isn't checked: a brake held
    through engine shutdown on the ground only happens when stopped, and a speed check could leave a flight
    stuck open.
  - **Engines off:** no `GENERAL ENG COMBUSTION:n` true for n up to `NUMBER OF ENGINES` (fuel flow decays for
    about 50 s after shutdown, so it isn't used).
  - **Times:** real-world UTC when the sample arrives, so they can be up to about 1 s late. Sim `ZULU TIME` isn't
    read. The airline runs on real time (design.md), so this drops D-023's plan to read it at block events.
- Why: The 100 ft AGL check means a one-sample `on_ground` flicker during taxi can't count as a flight; PMDG
  reads 8–14 ft on the ground. Needing a transition stops a recorder started mid-taxi from inventing a block
  out.
- Alternatives considered: Treating a brake that is off on the first sample as a release. A gate speed check for
  block in. Reading sim time.
- Known gap: a return to the gate without taking off leaves the flight open. This is rare, and milestone 3 can
  revisit it.

## D-043 — Block event fields
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder, data
- Decision:
  - `block_out`: `flight_id, time, lat, lon, fuel_lb, fuel_gal, moved_m, atc_id, title`. Position and fuel are
    taken at the release.
  - `block_in`: `flight_id, time, lat, lon, fuel_lb, fuel_gal, shutdown_time, shutdown_fuel_lb, block_time_s`.
    Position and fuel are taken at the set, and `shutdown_*` when the last engine stopped.
  - `flight_interrupted`: `flight_id, reason`.
  - `flight_id` is the block-out time (`2026-11-22T14:00:05.000Z`).
  - Times in `data` are the moment of the brake change; the line's `ts` is when it was written.
  - The airport isn't resolved; the app finds it from lat/lon (OurAirports, D-017). That also covers diversions.
- Why: Fuel at the set is the block-in fact. Fuel after shutdown is what is left in the tanks for the next
  flight's fuel check (recorder.md, "Fuel tracking"). Giving both lets the app choose. One flight is open at a
  time, so its start time is a unique ID.
- Alternatives considered: A GUID flight ID. Fuel at block in only. Looking up the nearest airport in the recorder.

## D-044 — Lost session ends the open flight as interrupted (until milestone 3)
- Date: 2026-11-22
- Status: Superseded by D-046 (the rule stays, and is now permanent; there will be no resume)
- Decided by: Claude
- Area: recorder
- Decision: When the sim session ends (`sim_quit`, `sim_process_gone`, `too_many_errors`, `handshake_timeout`) or
  the recorder exits (`recorder_exit`) with a flight open, `flight_interrupted` is logged after
  `sim_disconnected`. The detector then forgets everything: the flight, any candidate, the brake baseline and the
  landed state. There is no resume yet.
- Why: State is in memory only in milestone 2. Keeping stale state across a reconnect could turn a new session's
  first samples into a false block out or block in. D-009 wants interrupted flights kept and resolved in the app,
  and the recorder may resume them. Milestone 3 adds persistence and decides the resume rules.
- Alternatives considered: Keeping the flight open across a reconnect with no persistence. It would be lost
  anyway if the recorder restarted.

## D-045 — Ground flicker and invalid stretches in block detection
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22) (its "Open flights" bullet is answered by D-046)
- Decided by: Claude (from the milestone 2 code review)
- Area: recorder
- Decision:
  - **Dropping candidates:** a block-out candidate is dropped only by a brake set, and a block-in candidate only
    by a release. A sample with `SIM ON GROUND` false doesn't drop either one. Block out is still only confirmed
    on a sample that is on the ground.
  - **Invalid stretches:** when the junk filter rejects a sample (main menu, loading screen) and no flight is
    open, the detector forgets the brake baseline and any block-out candidate. So the next valid sample, e.g. a
    newly loaded flight with the brake off, is not a release. That stops it from becoming a block out at the
    load time with PMDG's default fuel.
  - **Open flights:** an invalid stretch doesn't change an open flight. What should happen to an open flight when
    Dave goes to the main menu or loads another flight is open, and is a question for Dave.
- Why: The review found that one flicker sample between the push release and 10 m would lose the whole flight. A
  flicker between the gate set and engine shutdown would leave it open. It also found that brake state survived
  a trip to the main menu.
- Alternatives considered: Re-baselining an open flight too. That could miss a brake set made during the gap and
  leave the flight stuck open.

## D-046 — The recorder never resumes a flight; the main menu ends it
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: recorder, app
- Decision:
  - The recorder never picks up an old flight. A flight that ends without block in is logged as
    `flight_interrupted`, and that is final. Dave then discards it or completes it in the web app (D-009).
  - This applies when the sim session is lost (quit, crash, kill), when the recorder exits, **and when Dave exits
    to the main menu** with a flight open. Dave only goes to the main menu to change aircraft, restart a flight or
    quit, so a flight is always over by then.
  - Turnarounds happen in the sim: after block in, Dave either exits to the main menu, or deboards, refuels,
    boards, restarts the engines and flies the next flight from the same gate. Several flights are often chained
    in one sim session.
- Why: Resuming adds rules and persisted state for a case that never happens in Dave's flying, and the web app
  can already handle an incomplete flight.
- Alternatives considered: Persisting the open flight and resuming it after a recorder restart or sim reconnect
  (the old milestone 3 plan). Ending an open flight only if the position jumps when the next flight loads.
- Replaces: D-044's "until milestone 3", D-045's open question about open flights, and the resume wording in
  `docs/recorder.md`.
