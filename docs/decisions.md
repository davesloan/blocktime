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
- Status: Superseded by D-063 (Qt won the UI test)
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
- Status: Superseded by D-063 (Qt won the UI test)
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
- Status: Superseded by D-063 (Qt won the UI test)
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
- Status: Superseded by D-072
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
- Note: the time zone part is superseded by D-084 (zones from an airport dataset joined on ICAO).
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
- Status: Accepted (amended by D-051: test builds get their own branch)
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
- Status: Accepted; the score part superseded by D-156 (BlockTime's code scores, Claude explains)
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

## D-047 — Leaving the flight is detected from the Sim stopped state
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder
- Decision: When the sim's `Sim` system state changes to stopped and a flight is open, log `flight_interrupted`
  with reason `sim_stopped` and reset the detector as for a lost session (D-044). This covers exiting to the main
  menu, restarting a flight and Escape → Quit. A stop with no flight open also resets the detector.
- Why: D-046 says leaving the flight ends it. In the 2026-11-22 logs every exit to the main menu logged
  `sim_stopped` within about 1 s. It never appeared while flying, in the Escape menu (pause flag 8) or in active
  pause (flag 4). The first junk sample sometimes came about 60 s after the exit.
- Side effect: force-terminating MSFS also logged `sim_stopped`, about 5 s before the process was gone. So a sim
  kill (and maybe a crash to desktop, not yet seen) is reported as `sim_stopped`, not `sim_process_gone`. Both are
  interrupted flights that Dave resolves the same way (D-009), so the reason isn't used to tell them apart.
- Alternatives considered: The first junk sample (`onInvalidSample`), which is late and could be a one-off glitch
  at the gate. `Pause_EX1` flag 1 (`full`), which arrives at the same moment but is a pause flag rather than the
  flight state.

## D-048 — A flight left open by a killed recorder is closed on the next start
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder
- Decision: At startup, after the single-instance check (D-037) and `recorder_started`, scan up to 7 daily log
  files (`events-YYYY-MM-DD.jsonl`), newest first. In the newest file with any `block_out`, `block_in` or
  `flight_interrupted` line, the last one decides: a `block_out` means the flight was left open, so log
  `flight_interrupted` with its `flight_id` and reason `recorder_crash`, and say so on the console. Lines that
  don't parse are skipped.
- Why: A killed recorder or a power cut writes no `flight_interrupted`, and CLAUDE.md says a flight must never be
  silently dropped. Doing it after the mutex means a second copy can't close a running copy's flight. Seven files
  covers a long flight across UTC midnight and a restart days later, without reading years of logs.
- Alternatives considered: Leaving it to the web app to treat an unended `block_out` as interrupted. Persisting
  the open flight in a state file.
- Known gap: if the sim is still in that flight after the restart, the next brake release followed by 10 m of
  taxi opens a new flight there. That needs the brake to be set at some point: either already set when the
  recorder restarts, or set later (e.g. a stop on the taxiway). The main menu or the session end then interrupts
  it. Only possible after a recorder crash, so it's accepted. Seen in the 0.3.0 sim test, where the brake was set
  before the kill.

## D-049 — Block events whose write failed are held and retried
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder
- Decision: `block_out`, `block_in` and `flight_interrupted` go through an `EventOutbox`. A failed write is held,
  with a console warning showing the event's JSON, and retried about every 5 s (at each reconnect try, 10–40 s,
  while waiting for the sim), oldest first; a new event waits behind held ones. At exit the outbox is flushed once more, and anything still held is printed to stderr as `LOST:`.
  The line's `ts` is when it was finally written; the event's own times are in `data` (D-043). Telemetry and
  connection events are not retried.
- Why: "Nothing may be lost" (CLAUDE.md), and the milestone 2 review deferred this. Block events exist only in
  memory, so a failed write lost the flight with only a stderr line.
- Alternatives considered: A console warning only. A separate fallback file.

## D-050 — Qt UI test builds between recorder milestones 3 and 4
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave + Claude
- Area: design
- Decision: Before milestone 4, build two throwaway Qt 6 + QML test builds to decide whether BlockTime becomes one
  native Windows app instead of the recorder plus a local web app (D-001). Test 1: a Qt shell running the recorder
  core on its own thread, a dark-only theme and a live flight panel. Test 2: a post-flight route map (live position
  is a bonus), a fleet list from SQLite and a side menu. Afterwards Dave decides, and milestones 4–8 are adjusted. Qt comes from
  the official installer, not vcpkg. D-001 and D-004 stay in force until then. Full context:
  `docs/qt-ui-test-context.md`.
- Why: Dave wants the lightest possible app next to the sim and a polished, FlyHub / Sim Toolkit Pro style
  dark-only UI. Qt is the lightest option but means slower UI work and less fluent help from Claude, so a real build
  answers the question better than a guess.
- Alternatives considered: Rails + React + Postgres (dropped: needs WSL2, and Dave won't run a VM while flying);
  C# + .NET (heavier, recorder rewrite); a native shell with WebView2 + React (Chromium-level memory); deciding
  without a test.

## D-051 — Test builds get their own branch
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: build
- Decision: Amends D-038. Milestone work still goes straight to `main`. A test build that exists to evaluate an
  option gets its own branch (the Qt UI test is `spike/qt-ui`). If it's kept, it gets the missing tests and a code
  review on the branch, then is merged. If not, the branch is deleted. Either way, what was learned goes into the
  decision log. Merge `main` into the branch now and then while it's open.
- Why: Trying something out and then merging or pruning it is exactly what branches are for, and it keeps
  throwaway code off `main`.
- Alternatives considered: A `spikes/` folder on `main` (throwaway code on `main` while it's being judged).
- Note (2026-11-22): Dave named the branch `qt-ui-spike` instead (D-052).

## D-052 — The Qt test app is built inside the recorder project
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: build
- Decision: On the branch `qt-ui-spike`, the Qt app is one more target (`blocktime-app`) in `recorder/CMakeLists.txt`.
  Its C++ is in `recorder/src/`, its QML in `recorder/qml/`, and the existing presets build it, so Qt is a normal
  requirement of the one build. Qt comes from the installer kit through the cache variable `BLOCKTIME_QT_DIR`
  (default `C:/Qt/6.12.0/msvc2022_64`, or `%BLOCKTIME_QT_DIR%`). The console recorder stays as a second target
  during the test. Dave also named the branch `qt-ui-spike` (D-051 said `spike/qt-ui`).
- Why: If Qt wins there is one app and one build, so the test is built that way from the start and can be merged as
  it is.
- Alternatives considered: An `app/` subfolder with an on/off option and its own presets (Claude's first plan). A
  top-level `app/` with its own build.

## D-053 — The recorder loop is a library with an observer
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: recorder
- Decision: The loop that was in `main.cpp` is now `Recorder::run()` (`recorder/src/recorder.{h,cpp}`). It reports
  everything through a `RecorderObserver` (`onStarted`, `onLine`, `onConnection`, `onTelemetry`, `onBlockEvent`).
  With `sim_connection.cpp` and the single-instance mutex (`single_instance.{h,cpp}`), it forms the static library
  `blocktime-sim`. The console recorder and the Qt app both use it. The app runs it on a `std::jthread`, and its
  observer only posts copies to the GUI thread with queued calls. Closing the window stops and joins the thread,
  so the clean exit (an open flight is interrupted with `recorder_exit`, then `recorder_stopped`) still happens.
- Why: One copy of the loop, so the app's log is exactly the recorder's log, and the logic stays plain C++ with no
  Qt in it (the context doc's rule: QML only displays).
- Alternatives considered: Copying the loop into the app. Making the loop a QObject.

## D-054 — QML look: Basic style, one Theme singleton, Segoe UI Variable, dark only
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: app
- Decision: Qt Quick Controls' **Basic** style, the one meant to be fully re-skinned. Every colour, size and font is
  in the `Theme` singleton (`recorder/qml/Theme.qml`); components never hard-code them. The font is Segoe UI
  Variable (Windows 11's system font, nothing to bundle) with tabular figures for numbers.
  `QStyleHints::setColorScheme(Dark)` also makes the Windows title bar dark. No continuous animations; the only
  timer is the 1 s block clock while a flight is open.
- Why: Dave wants a polished, FlyHub / Sim Toolkit Pro style and dark only, at the lowest CPU cost.
- Alternatives considered: The Material or Fusion styles (harder to make look like our own). Bundling Inter.

## D-055 — QML hot reload in debug builds
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: app
- Decision: In debug builds, a URL interceptor maps the module's `qrc:/qt/qml/BlockTime/<name>.qml` to
  `recorder/qml/<name>.qml`. A file watcher reloads the window about 200 ms after a `.qml` file is saved. The
  reload clears the singletons, so Theme changes show too. The recorder thread and the event feed keep running.
  Each QML file has an explicit `import BlockTime`, because a file loaded from the source folder can't otherwise
  see the module's C++ types. A failed reload shows a small error window, so the app can still be closed (and the
  recorder still stops cleanly). Release builds only use the QML compiled into the exe.
- Why: The change → see-it loop is a pass criterion. Without a rebuild, a QML change shows in about 1.5 s.
- Alternatives considered: Qt Creator's QML Preview (ties UI work to Qt Creator). Rebuilding for every change.

## D-056 — windeployqt after every app build
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: build
- Decision: A POST_BUILD step runs `windeployqt` (`--qmldir recorder/qml --no-translations --no-system-d3d-compiler
  --no-opengl-sw --no-compiler-runtime`) on `blocktime-app.exe`. The exe then runs from `build\x64-*\` without Qt on
  PATH.
- Why: Dave starts the app from PowerShell or Explorer; it has to find Qt's DLLs and QML plugins next to it.
- Alternatives considered: Putting the Qt `bin` folder on PATH. An install step.

## D-057 — The app has a --demo mode
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-22)
- Decided by: Claude
- Area: app
- Decision: `blocktime-app.exe --demo` fills the panel with made-up cruise values and an open flight. It doesn't
  start the recorder and writes no log.
- Why: UI work shouldn't need a 20-minute MSFS load to see a filled-in screen. It also gives Dave a screen to react
  to before flying.
- Alternatives considered: Replaying a recorded log into the panel (more work, and better left until a later UI
  test needs it).

## D-058 — Sim freezes don't count toward phase-detection holds
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave + Claude
- Area: recorder
- Decision: Phase detection (milestone 4) times its debounce holds and other timers only while the sim is
  sending messages. Any silence from the sim (no messages at all, as tracked for `sim_unresponsive`) adds
  nothing, however short. The recorder itself keeps logging freezes as facts (`sim_unresponsive` /
  `sim_responsive`, D-028) and does nothing else about them; the debrief can report them from the log.
- Why: On Dave's KSAN departure (2026-11-22, Qt UI test 1) MSFS froze for 51 s at 2,500 ft while loading scenery,
  with sim time stopped (the aircraft moved about 1 s worth across the gap). Dave sees this regularly near KSAN.
  Timing a 30–60 s hold by the clock would let a freeze after a level-off fake a sustained level-off.
- Alternatives considered: Timing holds by the wall clock (a freeze fakes a hold). Counting samples (telemetry
  is sent on change, so a sample count isn't a steady clock). Using a sim-time SimVar (not checked yet; the
  silence the recorder already measures is enough).

## D-059 — Map tiles: CARTO Dark Matter through Qt Location, key in a local settings file
- Date: 2026-11-22
- Status: Accepted for the provider (Dave chose CARTO Dark Matter, 2026-11-22); the details below are Proposed
- Decided by: Dave + Claude
- Area: app
- Decision: The route map (UI test 2) is Qt Location's `osm` plugin with CARTO Dark Matter raster tiles,
  `https://basemaps.cartocdn.com/rastertiles/dark_all/%z/%x/%y.png?key=<key>`, passed as
  `osm.mapping.custom.host`. The key is `carto_key` in `%LOCALAPPDATA%\BlockTime\settings.json`, never in the repo,
  never printed. Tiles are cached in `%LOCALAPPDATA%\BlockTime\tiles\<hash of the URL>\`, capped at 100 MB on
  disk (16 MB memory, 32 MB texture cache), and files older than 30 days are deleted at startup. Qt's own
  copyright box is hidden and CARTO's credit line is drawn in the theme's colours. The plugin is told not to fetch
  Qt's provider list. Without a key, the track is drawn on the plain dark background.
- Why: Dark tiles match the dark-only UI, and CARTO's free non-commercial tier (5 million tiles a month, key by
  email link, no account) is far more than this app needs. The 30-day limit, the visible credit line and our own
  User-Agent are CARTO's terms. A per-URL cache folder stops a wrong key's "API KEY REQUIRED" tiles (served with
  HTTP 200) from outliving a fixed key, since Qt's cache file names don't include the URL.
- Alternatives considered: Stadia Smooth Dark (smaller free tier, needs an account); no tiles; MapLibre Native Qt
  (more polished, heavy dependency); a local provider file for the osm plugin (works, but not needed).

## D-060 — The Flights screen reads flights back from the event log
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision: `loadFlightHistory` rebuilds flights and their tracks from the last 30 daily log files, read oldest to
  newest as one stream, every time the list loads (at startup and after every block event). There's no flight
  database in this test. Tracks drop samples within 25 m of the last kept point and are evenly thinned to at most
  2,000 points; distance and highest altitude use every sample first. A block_out while a flight is open closes it
  as interrupted with reason `unclosed`.
- Why: The log is already the record of every flight (D-008), so this needs no new storage format. A day of
  flying loads in 35–41 ms, fast enough for the GUI thread.
- Alternatives considered: A flights table in SQLite (the real app's job if Qt wins).

## D-061 — UI test 2's data layer: SQLite through vcpkg sqlite3, in plain C++
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision: `FleetStore` (in `blocktime-core`) uses the sqlite3 C API from vcpkg (3.54.1, static), not Qt SQL.
  The database is `%LOCALAPPDATA%\BlockTime\blocktime.db` with a **test schema** (`user_version` 1, one
  `airframes` table), seeded on first run with N818SX, N916SX and six made-up airframes covering `design.md` §3's
  types. The Fleet screen adds each tail's flights and block hours from the flight history. `--demo` opens it too.
- Why: Airline data in plain C++, tested with GoogleTest, survives a UI change if QML ever disappoints. SQLite was
  already the plan for the app's database (D-004).
- Alternatives considered: Qt SQL's built-in `QSQLITE` driver (no new dependency, but ties the data layer to Qt).
- Note (2026-11-22, from the merge review): the Fleet screen's flights and block hours count only the last 30 daily
  log files, so they can go down as old files drop out. A real "hours flown" needs a stored total. And
  `blocktime.db` (user_version 1, made-up airframes) is already on Dave's disk: the real airline schema must
  recognise user_version 1 and discard or migrate it, so the made-up tails never mix with real data.

## D-062 — Side menu: Live, Flights, Fleet
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision: A 200 px sidebar runs the full window height with the brand mark at its top and one entry per page
  (Live, Flights, Fleet), the selected one marked by an accent bar. The header above the page shows the page name,
  the connection pill and Open log folder. Icons are Segoe Fluent Icons glyphs (SpeedHigh EC4A, History E81C,
  Airplane E709), a Windows 11 system font. The window opens at 1300 × 760 (minimum 1100 × 600) so the Live page
  keeps test 1's width.
- Why: A side menu between screens is part of what makes it feel like FlyHub / Sim Toolkit Pro. A system icon
  font means nothing to bundle or license.
- Alternatives considered: Tabs along the top; bundled SVG icons.

## D-063 — BlockTime is one native Qt app
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: BlockTime becomes **one native Windows app** in C++20 with Qt 6 and a QML interface. The recorder runs
  on its own thread inside it (D-053), and the app runs the airline too. Supersedes D-001 (two programs), D-004
  (local web app with SQLite) and D-007 (visuals in the web app). What carries over:
  - It runs only on Dave's PC, with no hosting and no login.
  - Airline data lives in one SQLite file, reached through the sqlite3 C API in plain C++ (D-061).
  - Airline logic lives in plain C++ libraries tested with GoogleTest. QML only displays.
  - The event log is still written first and stays the record of every flight (D-008). The app reads it back
    (D-060).
  - The Node/TypeScript web app (`/webapp`) is dropped. Where older decisions say "the web app", read "the app".
- Why: both Qt UI tests passed every criterion (`docs/qt-ui-test-context.md`):
  - **In flight:** at most about 70 MB and 0.3% CPU with the map opened, against limits of 100 MB and 1%.
  - **The look:** Dave: "beautiful and exactly what I was looking for".
  - **Change loop:** working QML screens with about four one-pass fixes per test, and a 1.5 s hot-reload loop.
  - **The route map:** Qt draws a dark post-flight route map.
- Alternatives considered: the recorder plus a local Node/TypeScript web app in a browser (D-001, the previous
  plan); C# + .NET; a native shell with WebView2 + React.
- Still to settle before the merge (D-051): the repo layout for one app, whether the console recorder stays,
  tests and a code review on the branch, and reworking milestones 4–8.

## D-064 — One app, one build, at the repo root
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: build
- Decision: Before `qt-ui-spike` is merged, the project moves from `recorder/` to the repo root: `CMakeLists.txt`,
  `CMakePresets.json`, `vcpkg.json`, `src/`, `qml/` and `tests/` sit next to `docs/`. The empty `webapp/`
  placeholder is removed. The build runs `cmake --preset ...` from the root. The move uses `git mv`, so each file
  keeps its history.
- Why: with one app (D-063) there's one build. A folder named `recorder/` holding the whole app would mislead, and
  moving before the merge means `main` gets the final layout in one step.
- Alternatives considered: merging with `recorder/` and moving later.

## D-065 — The console recorder is dropped
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: `blocktime-recorder.exe` (the console front end, `main.cpp`) is removed. The Qt app is the only program.
  The recording code itself stays as the `Recorder` library (D-053), and the single-instance lock (D-037) stays so
  two copies of the app can't both record.
- Why: one program to maintain. The app already records exactly as the console recorder did (D-053), and it
  passed two sim flights.
- Alternatives considered: keeping it as a fallback and smoke-test tool.

## D-066 — The app is BlockTime.exe, version 0.4.0
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: build
- Decision: With the move to the root (D-064), the CMake project is `blocktime` 0.4.0 (also in `vcpkg.json`). The app
  target stays `blocktime-app`, but its file is `BlockTime.exe` (`OUTPUT_NAME`). The window title is "BlockTime",
  the sidebar shows "v0.4.0", and the map's User-Agent is `BlockTime/0.4.0 (open source)`. `recorder_started`
  events now say `"version": "0.4.0"`. The SimConnect client name stays "BlockTime Recorder", since the recorder is
  still the part that talks to the sim.
- Why: the first release of the one app (D-063) deserves its own version and a plain name. 0.3.0 was the last
  console recorder.
- Alternatives considered: keeping 0.3.0 until a feature milestone; renaming the target as well (more churn in
  CMake, for the same file name).

## D-067 — Last-resort errors go to errors.log
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision: The app sends `std::cerr` to `%LOCALAPPDATA%\BlockTime\errors.log`, or `%TEMP%\BlockTime-errors.log` if
  that can't be opened (`ErrorLog`, append-only, flushed after every write, one `--- BlockTime <version> started
  <UTC>` line per run). `BridgeObserver` writes every error line (`LOST:` events, D-049) there at once on the
  recorder thread, as well as posting it to the event feed. The errors `EventLog` and `SimConnection` print land
  there too. Qt messages pass through a handler that replaces the CARTO key with `<key>`.
- Why: the merge review found that a windowed app has no console. `LOST:` lines are made while the app shuts down,
  after the event loop ends, so they were never shown, and D-049's last resort left no trace. With the console
  recorder dropped (D-065), stderr needed a home.
- Alternatives considered: returning held events from `Recorder::run` to show before exit (the window is already
  closing); `OutputDebugString` only (invisible without a debugger).

## D-068 — The Flights list loads on a worker thread
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision: Amends D-060. `FlightListModel::reload()` runs `loadFlightHistory` on a `std::jthread` and swaps the
  result in on the GUI thread. A reload asked for mid-load runs again when it ends, so the newest block event is
  always included. Destroying the model stops a running load at once (`loadFlightHistory` takes a `std::stop_token`).
  The Flights page says "Loading flights..." until the first load ends. `reloadBlocking()` keeps a synchronous path
  for tests.
- Why: D-060 judged the GUI thread fast enough from one day of log (35–41 ms). The merge review pointed out that 30
  days of 1 Hz telemetry is far more; a synthetic 30 days of 4-hour flights (450,000 lines, 218 MB) took 4.8 s,
  which would freeze the window at startup and after every block event.
- Alternatives considered: caching parsed results for closed daily files (complex, since a flight can span files and
  thinning needs every sample; still worth doing later if the CPU burst matters); a faster telemetry parser.

## D-069 — Milestones work toward the complete flight flow
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: From here on, every milestone works toward the complete flight flow in `design.md` section 13: New
  Flight, planning, the matching check, the live dashboard, the arrival gate, the summary and the turnaround. The
  MVP is that flow working end to end.
- Why: "That's the core of this app and everything else depends on that. If we get that right the rest of it falls
  into place." (Dave)
- Alternatives considered: finishing recorder milestones 4–7 first, then the airline.

## D-070 — Plans come from a pre-filled SimBrief dispatch page, with SayIntentions runways
- Date: 2026-11-22
- Status: Accepted (the polling is superseded by D-101: a button imports the plan)
- Decided by: Dave
- Area: app
- Decision: Before planning, the app fetches both airports' ATIS from SayIntentions (`getWX`) for the active
  runways. It then opens SimBrief's dispatch page pre-filled (origin, destination, airframe, `origrwy`, `destrwy`,
  loads). Dave edits the route if he wants and presses Generate. The app polls the latest OFP and takes the first
  one generated after the page opened that matches origin and destination.
- Why: Dave prefers the dispatch page so he can enter his own route. It needs no SimBrief API key (the popup API
  FlyHub uses needs one, and its key is meant to stay on a server). SayIntentions' runways keep the plan in line with
  what ATC will use.
- Alternatives considered: SimBrief's API popup (needs an approved key); fetching whatever plan Dave last made.

## D-071 — A flight is only recorded when the sim matches the planned flight
- Date: 2026-11-22
- Status: Superseded by D-088 (the registration check became an aircraft type check; the mechanism stays)
- Decided by: Dave (rule), Claude (mechanism)
- Area: recorder, app
- Decision: The aircraft's registration must be the planned airframe's and it must be at the planned origin,
  otherwise the brake release doesn't open a flight and the dashboard shows "Not recording" with the reason.
  Mechanism (Proposed): the app gives the recorder the planned flight; without one, or on a mismatch, the recorder
  logs `block_out_refused` with the reason instead of `block_out`. Origin radius starts at 3 km.
- Why: Dave: "It should absolutely check I am in the correct aircraft type by registration and am loaded at the
  origin airport."
- Alternatives considered: recording every flight and letting the app reject it later (a rejected flight would
  still show up in the log as a flight).

## D-072 — Interrupted flights are discarded
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: Supersedes D-009's "complete" option. A flight that ends without block in is deleted as if it never
  happened: no resume (D-046), no completing it in the background. The aircraft stays where it was, and the flight
  earns no money and adds no fuel, hours or leg-template data. Its GSX services are dropped too. The app shows a
  short notice that it was discarded. The recorder still logs `flight_interrupted`.
- Why: Dave: resuming in the sim is "a horrible experience I simply won't bother to do", and completing it in the
  background would credit flying that didn't happen.
- Alternatives considered: D-009 (discard or complete, user's choice).

## D-073 — GSX service events
- Date: 2026-11-22
- Status: Accepted; the "Turnaround and Boarding" line superseded by D-136, the checklist refined by D-135. State
  values confirmed by the probe flights
- Decided by: Dave + Claude
- Area: recorder, app
- Decision: The recorder reads GSX Pro's service states (catering, refuelling, boarding, deboarding, departure /
  pushback, passenger counts) from L:vars through SimConnect, from the first valid sample in a flight whether or
  not a flight is open, and logs each change as `gsx_service`. Read only. The app shows each used service with
  start and end times; if none was completed before pushback, one "Turnaround and Boarding" line appears before
  "Pushback initiated". "Ready for Shutdown or Turnaround" shows on deboarding complete, with an informational
  checklist (engines, brake, deboarding, beacon/strobe/seatbelt, APU).
- Why: Dave wants start and stop times for each service in the flight log and on the live dashboard.
- Alternatives considered: writing loads to GSX (not needed; loads go through SimBrief).
- To verify: the state values (1 available … 6 completed, from community tools) and which PMDG switches are readable,
  in probe flights.

## D-074 — The app assigns the arrival gate through SayIntentions
- Date: 2026-11-22
- Status: Accepted (fallbacks confirmed by Dave, 2026-11-22); timing and runway source superseded by D-141; the
  closest-to-runway pick superseded by D-155
- Decided by: Dave (rule), Claude (fallbacks)
- Area: app
- Decision: Mid-flight the app picks a gate and asks SayIntentions to assign it (`assignGate`). Order: preferred
  gates for the airport from config; otherwise the right size and type closest to the arrival runway. Passenger
  flights use gates; freighters and charters use ramps. Parking spots come from the sim's facility data.
  Proposed: if no ramp fits, any ramp that fits, then a gate; size is the spot radius against the family's wingspan.
- Why: Dave wants gates assigned for him, and "freighters and charters shouldn't be parked at gates".
- Alternatives considered: a random gate that fits; letting SayIntentions choose.

## D-075 — Rotations are unlocked by flying their legs
- Date: 2026-11-22
- Status: Accepted (fuel scaling confirmed by Dave, 2026-11-22)
- Decided by: Dave (rule, family), Claude (template details)
- Area: design
- Decision: A rotation can be assigned for background flying only after Dave has flown every leg in it, in that
  direction, in the same family (any 737 or any 777). Only completed flights count. Each leg's template comes from
  Dave's flights (first flight sets it, later ones refine it, a little random variation in the background). Times
  carry across the family. Proposed: fuel uses the configuration's learned burn, corrected by Dave's actual-versus-
  plan burn on that leg.
- Why: Dave: "so that you don't build a rotation that you aren't willing to fly yourself and also so that data can
  be collected and used as a template for simulating the background flights on that rotation."
- Alternatives considered: unlocking whole rotations by flying them in order (could mean 40+ hours before
  assigning a long 777 loop); matching by exact configuration or variant (Dave chose family).
- Note: Claude earlier suggested scaling fuel from "that airframe's own SimBrief plan", but background legs have no
  SimBrief plan (plans need Dave to press Generate), so that was replaced by the configuration's learned burn.

## D-076 — MVP setup comes from config files
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: For the MVP, the airline identity, fleets and leased airframes are set in hand-edited config files. A
  settings screen comes later.
- Why: "That makes more sense for an MVP." (Dave)
- Alternatives considered: first-time setup screens in the MVP.

## D-077 — Thin slice first: milestones 4–10 to the MVP
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: Milestones 4–6 make the whole flight flow work in a plain form (airline foundation; New Flight and plan;
  record the planned flight with a plain summary). Milestones 7–10 deepen it (phases and stages; GSX; gates;
  procedure capture and Claude's debrief). The MVP is done after milestone 10. Money, including the monthly lease
  charge, works from milestone 4. Details in `docs/roadmap.md`; replaces the old recorder milestones 4–7.
- Why: "Like in real life it's better to have working software fast than complete software slow." (Dave) It also
  leaves room for new ideas during the deepening. "The money has to work in the MVP." (Dave)
- Alternatives considered: finishing the recorder first; building each step of the flow completely in order.

## D-078 — Database design rules
- Date: 2026-11-22
- Status: Accepted (the time representation is left to milestone 4's plan)
- Decided by: Dave (principles), Claude (SQLite specifics)
- Area: data
- Decision: The airline database follows proper relational design: normalised with atomic columns, primary keys,
  `UNIQUE` natural keys, foreign keys with explicit delete rules (and `PRAGMA foreign_keys = ON`), no
  code-enforced polymorphism, `STRICT` tables with `NOT NULL` and `CHECK` constraints, indexes on foreign keys and
  queried columns, integer cents for money, write-once history, one transaction per multi-table change, and
  numbered migrations. The full list is in `CLAUDE.md` ("Database"); the schema is documented in
  `docs/database.md`.
- Why: Dave: "proper sql database design is important to me. Proper indexing, keys, relations, atomic fields,
  avoiding anti-patterns such as polymorphism that is enforced by code and not by the db."
- Alternatives considered: a looser schema with JSON columns; letting code enforce relations.

## D-079 — Airport data is imported once, filtered
- Date: 2026-11-22
- Status: Accepted
- Decided by: Claude, agreed by Dave
- Area: data
- Decision: OurAirports' airports and runways are imported once into the database, filtered to airports with a
  runway the fleet could plausibly use, and refreshed manually now and then. The app queries them as needed.
  Parking spots and runway layout come from the sim just in time (D-074).
- Why: OurAirports only publishes whole-world CSV files (about 15 MB); filtered, it's a couple of MB on disk and
  nothing in memory. New Flight needs the destination's runways and distance before the sim is involved.
- Alternatives considered: fetching airports just in time (no per-airport source exists).

## D-080 — Money lines are rounded to the nearest $5 when posted
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave + Claude
- Area: data, app
- Decision: Every money line (each income and cost line of a flight, each lease charge, each ledger entry) is
  rounded to the nearest $5 when it's created, and the rounded amount is what's stored (integer cents, always a
  multiple of 500, D-078). Rounding applies to the line, not the unit (150 fares of $127.50 make one line, then
  rounded). Halves round up ($2.50 → $5). Totals are sums of the stored lines, so what's shown always adds up.
  Display: summary tiles and the cash figure use short forms ($1.4M, $382.5k, $12.48M); the ledger and summary
  lines show full amounts.
- Why: Dave wants a cleaner interface as numbers grow. Rounding when posted, not only on screen, keeps "the money
  has to work" true: lines always add up to their totals. Nearest, not always up, keeps the books neutral.
- Alternatives considered: rounding only for display (lines wouldn't add up); always rounding up (small upward
  drift on every line).


## D-081 — Database times are ISO 8601 text in UTC
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: data
- Decision: UTC instants are stored as `TEXT` `YYYY-MM-DDTHH:MM:SSZ` (whole seconds), calendar dates as
  `YYYY-MM-DD`, each with a `CHECK` on the shape. This settles the time representation D-078 left open.
- Why: Readable in a database browser, the same shape as the event log, and sorts correctly as text.
- Alternatives considered: integer Unix seconds (compact, unreadable without converting).

## D-082 — The catalog is a template; the database is the source of truth
- Date: 2026-11-22
- Status: Accepted (the identity exception is Proposed)
- Decided by: Dave (rule), Claude (details)
- Area: data, app
- Decision: When an airframe is leased (from `airline.yaml`, D-076), its type's and configuration's values from the
  catalog are copied into the database with it. (Refined by D-093: types are refreshed from the catalog; the lease's
  rate, seats and holds are what's copied. By D-091 a leased plane is an airplane.) From then on the database owns the airframe: location, fuel and
  status change only through the airline's own processes, and catalog or config edits never change an airframe
  already leased (editing or removing it in config logs a warning to `errors.log`). Proposed: the airline's name,
  ICAO code and callsign follow the config on every start, since they have no history behind them; starting cash
  is posted once.
- Why: Dave: "the config serves as a template for that airframe ... the db remains the source of truth, the
  airframe config just provides initial data."
- Alternatives considered: config owns the airframes and removing one returns its lease; removal ignored but
  config re-read for other fields.

## D-083 — Lease affordability now, fleet slots in milestone 6
- Date: 2026-11-22
- Status: Accepted (the month and catch-up details are Proposed)
- Decided by: Dave (scope), Claude (details)
- Area: app
- Decision: Milestone 4 enforces design.md §5: an airframe whose first month costs more than the cash on hand isn't
  leased (tried again at the next start), and a month that can't be paid returns the airframe (`airframe_return`,
  reason `unpaid`) instead of charging it. Slots (§6) wait for milestone 6, when hours are stored. Proposed: a lease
  month starts on the `leased_on` day each month, or the month's last day if shorter; due months are charged in
  date order across all airframes, each posted at its due date; `UNIQUE (airframe_id, period_start)` makes a
  double charge impossible.
- Why: "The money has to work in the MVP." Slots need stored hours, which milestone 6 adds.
- Alternatives considered: slots now (one airframe until hours exist); both rules after the MVP.

## D-084 — Airport time zones from an airport dataset, joined on ICAO
- Date: 2026-11-22
- Status: Accepted (the dataset and the filter are Proposed)
- Decided by: Dave (approach), Claude (dataset, filter)
- Area: data
- Decision: Supersedes the time zone part of D-017. Each airport's IANA time zone comes from mwgg/Airports
  (`airports.json`, about 29k airports keyed by ICAO, with a `tz` field), joined to OurAirports on the ICAO code.
  Airports without a zone aren't imported. Local times use the C++ standard library's time zone support (MSVC
  reads Windows' own time zone data), so no extra library. Proposed import filter: `large_airport` or
  `medium_airport`, not closed, a 4-character ICAO code, an open paved runway of at least 4,000 ft, and a zone.
- Why: Loads (milestone 5) and the curfew depend on local departure time. A lookup by ICAO is small; deriving zones
  from coordinates needs timezone-boundary-builder's polygons (about 100 MB to process).
- Alternatives considered: boundary polygons; estimating from longitude (wrong across most borders).
- Note: the repository shows an MIT licence at the top level; the import task checks the LICENSE text covers
  `airports.json` before the data is used. OurAirports' terms are checked in the same task.

## D-085 — Schema patterns for the airline database
- Date: 2026-11-22
- Status: Superseded by D-090
- Decided by: Claude
- Area: data
- Decision: Subtypes are exclusive by schema: the supertype has `kind` with a `CHECK` and `UNIQUE (id, kind)`, each
  subtype a fixed `kind` and a composite foreign key to it (airframes: passenger/freighter; ledger lines: opening
  cash/lease charge, later flight lines). Cash is the sum of the ledger, never stored. One airframe row is one lease
  (re-leasing a returned registration isn't in the MVP). Connections use `foreign_keys = ON`, `journal_mode = WAL`
  and `synchronous = FULL`. Full detail in `docs/database.md`.
- Why: D-078 asks for polymorphism the database enforces; a stored balance could disagree with its lines.
- Alternatives considered: triggers to enforce subtypes (harder to read); a balance column updated with each line.

## D-086 — The Qt test's database is set aside, and real migrations start at 2
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: data
- Decision: A `blocktime.db` with `user_version` 1 (D-061's test schema) is renamed to `blocktime.test-schema.db`
  and a new database is built. The real schema's first migration is number 2.
- Why: The made-up tails must never mix with real data (D-061 note); renaming keeps the file in case anything in it
  is wanted, and numbering from 2 means a version can't mean two different schemas.
- Alternatives considered: deleting the file; migrating its rows (they're made up).

## D-087 — Registration and SimBrief airframe are entered per airframe
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: data, app
- Decision: Dave enters each airframe's registration in `airline.yaml` when leasing it, and sets the same
  registration in a custom SimBrief airframe made for it. That SimBrief airframe's ID is entered per airframe too;
  it isn't part of the catalog's configuration template.
- Why: The sim's registration (ATC ID) comes from the livery, and Dave can't make a livery for every registration.
- Alternatives considered: one livery per registration (too much work); one SimBrief airframe per configuration
  (the OFP would carry the wrong registration).
- Note: D-071's registration check couldn't work with shared liveries; D-088 replaces it with an aircraft type check.

## D-088 — The matching check compares aircraft type and origin
- Date: 2026-11-22
- Status: Accepted (how the sim's type is read is open until the probe)
- Decided by: Dave
- Area: recorder, app
- Decision: Supersedes D-071. A flight is only recorded when the sim's aircraft is the planned airframe's **type**
  (737-600, 737-800, 737-800 BBJ2, 777-300ER…) and it's at the planned origin. The registration isn't checked.
  D-071's mechanism stays: the app gives the recorder the planned flight, and a mismatch logs `block_out_refused`
  (reasons `no_planned_flight`, `wrong_aircraft_type`, `wrong_airport`) and shows "Not recording" with the reason.
  Origin radius starts at 3 km.
- Why: The sim's registration comes from the livery, and Dave can't make a livery for every registration (D-087).
  The type check still catches loading into the wrong aircraft.
- Alternatives considered: origin only (nothing would stop a 777 flight being recorded against the BBJ2).
- Open: which SimVar tells the PMDG variants apart (`TITLE`, `ATC MODEL`, `ATC TYPE`, or something else) isn't
  known. A probe logs them on Dave's next flights, at the latest in milestone 6's sim tests, before the check is
  built. The value to match then goes on `aircraft_type` (`database.md`).

## D-089 — Belly cargo is chosen per flight and per rotation
- Date: 2026-11-22
- Status: Accepted (the preview details are Proposed)
- Decided by: Dave (rule), Claude (preview details)
- Area: design
- Decision: Spare belly cargo on passenger flights isn't a fixed property of the configuration. Dave chooses it on
  each free flight in New Flight, and each rotation has its own belly cargo setting for its background legs. A
  configuration only says whether it **can** carry belly cargo; the BBJ2 can't. Built in milestone 5 with the loads.
  Proposed: the cargo is rolled once when the flight is set up, and the option only adds or removes it in the
  preview, so switching it can't re-roll the passengers. The option starts on. A rotation leg Dave flies himself
  uses the rotation's setting.
- Why: Dave: "carrying belly cargo should be an option tied to the rotation for bg flights and as an option when
  creating a free flight." The BBJ2 carries no belly cargo.
- Alternatives considered: a fixed sells-belly-cargo flag per configuration (design.md §3 before this).

## D-090 — Database rules are applied for what they protect
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave (principle), Claude (the schema changes)
- Area: data
- Decision: Supersedes D-085. The database rules (D-078) are applied for their intent: stopping real bugs such as
  relations the database can't check, ambiguous NULLs, contradictory rows and money counted twice. Structure added
  only to satisfy a rule's wording is dropped. In the schema this means: passenger and freighter airframes share one
  `airframe` table, with no subtype tables or composite keys; whether a plane flies passengers, VIP/charter or cargo
  is the `role` of its **type** (`airliner`, `executive`, `freighter`), which also decides belly cargo (D-089);
  cabins hang off `airframe`, and only the lease code and its test keep freighters without them. In the ledger, a
  `lease_charge` is its own fact pointing at its `ledger_line`, with `UNIQUE (airframe_id, period_start)` kept
  because a double charge is a real money bug; categories are a lookup table, since the list grows and SQLite can't
  alter a `CHECK`. Cash is still the sum of the ledger, and the connection settings stay (`foreign_keys`, WAL,
  `synchronous = FULL`). The principle is written into `CLAUDE.md`.
- Why: Dave: the earlier design was "too complicated and designing around a rule rather than questioning the rules
  intent." The flags that differ between passenger planes and freighters are a plain "no" for the other kind, not
  NULLs, and they belong to the type anyway.
- Alternatives considered: D-085's exclusive subtypes (enforces "no cabins on a freighter" in the schema, at the cost
  of three tables and composite keys against a bug one code path can't realistically make).

## D-091 — Airframes are templates; leased planes are airplanes
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: design, data
- Decision: An **airframe** is a template in the catalog: a type in one configuration (737-800 two-class). An
  **airplane** is one leased plane made from an airframe, with its own registration, SimBrief airframe, location and
  fuel. The database's tables are `airplane`, `airplane_cabin`, `airplane_hold` and `airplane_return`, and an
  airplane stores the `airframe_name` it was made from. "SimBrief airframe" keeps SimBrief's own meaning (a saved
  aircraft profile; each airplane has one, D-087). Earlier entries that say "airframe" for a leased plane mean an
  airplane; the specs are updated, and the code's names change when milestone 4 replaces the Qt test's
  `FleetStore`.
- Why: Dave: "They are like a class and the airplane is the object, if talking in software terms."
- Alternatives considered: "airframe" for the leased plane (design.md §3 before this).

## D-092 — Airframes will be managed in the app after the MVP
- Date: 2026-11-22
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: The settings screen that follows the MVP (D-076) also manages the catalog: aircraft types and airframes
  are created and edited in the app rather than in YAML. The catalog then moves into the database; airplanes already
  leased keep the values copied when they were leased (D-082). Until then the catalog stays a hand-edited YAML file.
- Why: Dave: "at some point I will want to be able to manage airframes in the app, not through a config file."
- Alternatives considered: building it before the MVP (the roadmap keeps settings after the flight flow, D-077).

## D-093 — Schema review: four simplifications
- Date: 2026-11-22
- Status: Accepted
- Decided by: Claude, agreed by Dave
- Area: data
- Decision: Applying D-090 to the whole draft: (1) no `airline` table; name, ICAO code and callsign are read from
  `airline.yaml` on every start until the settings screen (D-092), and starting cash is still an `opening_cash`
  ledger line. (2) No `runway` table; `airport.longest_runway_ft` holds the longest open, paved runway, the only
  value the runway check needs. (3) Airports don't point at the import that wrote them; `airport_import` is just a log
  of when imports ran. (4) `aircraft_type` is reference data refreshed from the catalog at every start, instead of
  copied once at the first lease, so a second airplane of an edited type doesn't get stale values. Lease rate, seats
  and holds are still copied onto each airplane (D-082). Also folded in: a lease's end is `returned_on` and
  `return_reason` on `airplane`, not a separate table.
- Why: Each removed piece stopped no bug the code could realistically make; the copy-once types caused one.
- Alternatives considered: keeping runways for later milestones (the sim provides runway layouts there).

## D-094 — Airports come in through a console tool, from files Dave downloads
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: data, build
- Decision: `blocktime-import.exe <folder>` reads OurAirports' `airports.csv` and `runways.csv` and mwgg/Airports'
  `airports.json` from a folder, filters them (D-084) and upserts them into the airline database. Dave downloads
  the files with the README's PowerShell line and runs the tool again to refresh. Columns are found by header name;
  the ICAO code is `icao_code` when the file has it, else `gps_code`. An ICAO code that appears twice keeps the
  airport with the longest runway.
- Why: The app needs no network code or progress screen for a once-in-a-while job, and a console shows the counts.
  The parsing lives in `blocktime-core` with tests.
- Alternatives considered: the app downloading the files itself on first start (Qt Network, an async state on
  screen); bundling filtered data in the repo (stale, and a few MB of data in git).

## D-095 — yaml-cpp reads the config files
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: build
- Decision: `catalog.yaml` and `airline.yaml` are read with yaml-cpp (0.9.0, MIT, from vcpkg) into plain structs in
  `blocktime-core`, validated there. Errors name the file and line ("airline.yaml line 12: ..."); one shared helper
  (`config_yaml`) does that for both readers. Money in the files is in whole dollars, multiples of $5.
- Why: Mature, simple API, and it reports line numbers, which matters for hand-edited files (Review Focus 1 of the
  milestone 4 plan).
- Alternatives considered: rapidyaml (faster, but speed doesn't matter for two small files); JSON config (no
  comments, harder to edit by hand).

## D-096 — Lease settlement details
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app, data
- Decision: `founded_on` in `airline.yaml` dates the opening cash, and no lease may start before it. A new
  airplane's first month is a lease event like any other month, so one pass settles everything in date order
  (ties by registration), one transaction per month. Affordability is the cash as of the month's date (lines
  posted at or before it). Config airplanes whose `leased_on` is still ahead are leased when the day comes, even
  while the app is open. A month that can't be paid returns the airplane on that date, and it's never charged
  again. Rare back-dated cases (adding an airplane leased before lines already posted) aren't rechecked.
- Why: Charging missed months on their real dates keeps the books true when the app was closed (design.md §8's
  rule for money events), and "cash as of the date" is what the airline actually had then.
- Alternatives considered: charging missed months on the day the app opens (books dated wrong); checking
  affordability against today's cash (a later income could pay for an earlier month).

## D-097 — The Fleet table drops its flight and hour counts until milestone 6
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision: The Fleet table shows registration, type, airframe, fleet, location, fuel and status. The flights and
  block hours that D-061 counted from the event log are gone, along with the Qt test's `FleetStore`.
- Why: The count matched the sim's registration (ATC ID) to the airplane's, which no longer holds (D-087), and
  counting the last 30 days of log made totals drop over time (D-061 note). Milestone 6 stores hours per flight.
- Alternatives considered: keeping the counts until milestone 6 (they'd show 0 for every real airplane).

## D-098 — Review fixes: database robustness for milestone 4
- Date: 2026-11-22
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude (from the milestone 4 code review)
- Area: data, app
- Decision:
  - Every connection sets a 5 s busy timeout, so the app's minute tick and `blocktime-import` wait for each other
    instead of failing. A transaction whose `BEGIN` fails writes nothing; the caller reports the error.
  - A `blocktime.db` at `user_version` 1 that has an `airplane` table is a real airline whose version an older
    BlockTime (0.4 and older, D-061's `FleetStore`) overwrote: the version is put back to 2 and a notice says so. Only
    a version-1 file without that table is the Qt test's database and is set aside (D-086).
  - One new airplane that can't be leased (its SimBrief airframe already belongs to a leased airplane, or the insert
    fails) is a notice for that airplane only; every other lease still settles.
  - The minute tick resets the Fleet and Money models only when it changed something, so the views don't jump back
    to the top every minute.
  - The "Can't afford" notice says to set a later `leased_on`, since affordability is the cash as of that date (D-096).
- Why: The review found that a stale older build could hide the real airline, that one bad airplane stopped all
  lease charges, and that a failed `BEGIN` let a lease month be written without its `lease_charge` row (which would
  charge it again).
- Alternatives considered: refusing to open a version-1 file with airline tables (safer, but needs a manual fix);
  retrying a busy transaction in a loop (the busy timeout already waits).

## D-099 — MVP code is for Dave, not production
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: Until the MVP is done, BlockTime only has to work for Dave. Code that reads the hand-edited config files
  (`airline.yaml`, `catalog.yaml`) gets the happy path only: no drift warnings, no extra validation, no edge-case
  fixes. Dave keeps the files correct. Tests and error handling go to what can go wrong in a real flight (the sim,
  network calls, money). Written into `CLAUDE.md`.
- Why: Dave: "there's little point in putting a lot of error handling and edge case fixes for stuff that is just a
  placeholder until later editions." The config files give way to screens after the MVP (D-076, D-092). "It does not
  have to be 'production ready' and 'safe for use by everyone' during the MVP stage."
- Alternatives considered: milestone 4's approach (line-numbered errors, drift warnings). Already built, so it stays,
  but it isn't extended.

## D-100 — Airplane limits: airframe defaults, overridable per airplane
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: data, app
- Decision: The values New Flight needs before any SimBrief plan exists are stored with each catalog airframe as
  defaults: minimum runway, MLW, passenger weight, bag weight and max payload (MZFW − empty weight). An airplane in
  `airline.yaml` may override any of them. They're copied onto the airplane when it's leased, like seats and holds
  (D-082). An airplane leased before milestone 5 gets its airframe's defaults at the next start. Later edits in
  `airline.yaml` are ignored (D-099: no warning). For the settings screen (D-092): leasing pre-fills the airframe's
  defaults, and changing any of them blanks the SimBrief airframe ID, because the SimBrief airframe has to match;
  Dave makes a new one in SimBrief and pastes its ID in.
- Why: Dave: "default values should be stored with the airframe. when leasing, the fields are pre-filled but I have
  the ability to override them specifically for the airframe." SimBrief has no airframe API, and the OFP's weights
  come too late for the preview and the runway check.
- Alternatives considered: typing them per airplane only (retyped for every plane of a configuration); taking them
  from the first OFP (too late for the runway check and the first preview).
- Note: Claude first said passenger weight wasn't needed; it is, to cap cargo at max payload.

## D-101 — The plan comes in with an "Import Active SimBrief Plan" button
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: Supersedes D-070's polling. After pressing Generate on SimBrief, Dave clicks **Import Active SimBrief
  Plan**. The app fetches the latest OFP once per click and stores it if it belongs to the planned flight. The
  dashboard shows "Waiting for plan" until then.
- Why: SimBrief asks apps to fetch the OFP only in response to a user action and may block apps that poll
  (`docs/notes/simbrief-api.md`). Dave: "just have a button I can click."
- Alternatives considered: fetching when the app window regains focus (still automatic, and could fire many times);
  polling on a timer (against SimBrief's rules).

## D-102 — The flight number is typed for each flight
- Date: 2026-11-23
- Status: Superseded by D-106 (a default number per airplane)
- Decided by: Dave
- Area: app
- Decision: New Flight has a flight number field (1–4 digits), empty each time, which Dave fills in. The dispatch
  page gets `airline` (the airline's ICAO code) and `fltnum`; SimBrief builds the callsign from them.
- Why: Dave's choice.
- Alternatives considered: an editable suggested next number; automatic numbers only.

## D-103 — The planned flight is current state in the database
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: data
- Decision: Planning writes a `planned_flight` row with its rolled loads (passengers per cabin class and a cargo
  total), the locked fuel price, the runways from the ATIS, and later its imported SimBrief plan (`simbrief_plan`,
  with the OFP's JSON kept verbatim). Bags and the weight per hold aren't stored: they follow from passengers × bag
  weight and the airplane's holds. One planned flight at a time: New Flight is disabled while one exists, and
  **Cancel flight** deletes it (free, design rule 2). It isn't history; milestone 6 turns a flown one into a `flight`.
  Importing again replaces the stored plan.
- Why: The plan must survive an app restart between planning and flying. Deleting an unflown plan loses no fact.
- Alternatives considered: one planned flight per airplane (the dashboard would have to choose which to show).

## D-104 — Preview and pricing details
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23), except the rates as code constants, superseded by D-107: they go
  in a config file
- Decided by: Claude
- Area: app
- Decision:
  - The cost and fare rates of design.md §10–12 are constants in one place in the code (`pricing.h`). The fuel price
    is `fuel_price_per_100lb` in `airline.yaml` (optional, default $41), read at start and locked on the planned
    flight.
  - Before a plan exists, the preview uses the great-circle distance and an estimated block time (distance ÷ the
    type's cruise speed + 30 min) and shows "+ fuel". Once a plan is imported, the dashboard's estimate uses
    SimBrief's block time and en-route burn instead. Distances stay great-circle for fares and ATC.
  - Day or night loads go by the origin's local time when New Flight opens (7:00–18:59 is day).
  - An executive type (the BBJ2) always pays VIP crew and catering rates. A delivery flight pays fuel, landing fee,
    ATC, pilots and maintenance: no gate, handling, flight attendants or catering.
  - Each money line is computed in cents and rounded to $5 (D-080); totals add the rounded lines.
  - Landing weight near MLW: the dashboard warns when the plan's estimated landing weight is 95% of MLW or more.
- Why: The rates are tuned rarely, and a config section for them would be placeholder code (D-099). The rest fills
  gaps the design leaves open.
- Alternatives considered: rates in a config file (design.md's long-term wish; it can come with the settings screen).

## D-105 — ATIS runways and the import check
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app
- Decision:
  - One `getWX` call for both airports, when Dave presses Plan flight. The origin runway is the first of
    `active_runways_departing`, and the destination's the first of `active_runways_arriving`. Either falls back to
    `active_runway`, and then to none (SimBrief chooses). A single-digit runway gets a leading zero (9R → 09R).
    Without keys, or after an error or an 8-second timeout, planning continues without runways and says so.
  - An imported OFP is accepted only if its origin, destination and airframe (`aircraft.internal_id`) are the
    planned flight's, its units are pounds, and it was generated after Plan flight was pressed (with 5 minutes'
    allowance for clock differences). Otherwise nothing is stored and the dashboard says why ("The latest SimBrief
    plan is KBFI–KSEA; this flight is KBFI–KSAN").
- Why: The live getWX answer has these fields while the documented `active_runway` is missing
  (`docs/notes/sayintentions-api.md`). The import check catches forgetting to press Generate, which would otherwise
  import yesterday's plan for the same route.
- Alternatives considered: comparing the OFP's passenger count with the rolled loads (the app's loads count anyway).
- Note (Dave, 2026-11-23): an airport whose ATIS text is empty still gets SayIntentions' computed runways. That
  guess is likely what its ATC will use, so it's kept rather than leaving the runway to SimBrief.

## D-106 — Each airplane has a default flight number for free flights
- Date: 2026-11-23
- Status: Accepted (storage confirmed by Dave, 2026-11-23)
- Decided by: Dave (rule), Claude (storage)
- Area: app, data
- Decision: Supersedes D-102. Each airplane has a **default flight number** for its free flights, unique across the
  airline, and New Flight pre-fills it (still editable). Rotation legs (after the MVP) each carry their own flight
  number, so KSAN–KSLC on a rotation is always XLY1414, and no airplane default or rotation leg shares a number.
  Proposed storage: `flight_number` per airplane in `airline.yaml`, set into `airplane.free_flight_number` (1–4
  digits, a unique index) at every start rather than copied once, since it's only a default and each planned flight
  keeps its own number. Uniqueness against rotation legs is enforced when rotations are built.
- Why: Dave: "whenever I'm flying the BBJ2 in-sim I know that my flight number will always be, for example, 8,
  unless I'm flying a leg of a rotation."
- Alternatives considered: typing it every time (D-102); copying it once at lease time like the limits (changing it
  would then need the settings screen).

## D-107 — Fare and cost rates live in a config file
- Date: 2026-11-23
- Status: Accepted (layout confirmed by Dave, 2026-11-23; built in milestone 6)
- Decided by: Dave (rule), Claude (layout)
- Area: app
- Decision: Supersedes D-104's "rates are constants in the code". The fare and cost rates of design.md §10–12 (fares,
  cargo rate, charter rate and minimum, landing, gate, handling, GA services, ATC, catering bands, cargo handling,
  crew and maintenance per hour, the VIP multipliers) live in a config file, so Dave can tune them without
  rebuilding. A settings screen may edit them later. Proposed layout: a `rates.yaml` in `%LOCALAPPDATA%\BlockTime\`
  with a commented example in `config/`, holding the fuel price too (moved from `airline.yaml`), and read whenever
  New Flight opens, so a change applies to the next flight without restarting. A planned flight keeps the fuel price
  it was planned with.
- Why: Dave: "I want to be able to fiddle with them without having to rebuild the app every time."
- Alternatives considered: constants in `pricing.cpp` (D-104); a section in `airline.yaml` (mixes the game's prices
  with the airline's identity and fleet).

## D-108 — An interrupted flight keeps its planned flight
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: When a flight is interrupted and discarded (D-072), the planned flight stays: same loads, same SimBrief
  plan. Live goes back to "Waiting for sim" with a notice ("KBFI–KSAN interrupted and discarded"), so Dave can
  reload and fly it again, or Cancel flight.
- Why: Dave's choice. Being forgiving (design rule 2): a crash doesn't cost the planning work.
- Alternatives considered: deleting the planned flight too, so New Flight starts again with new loads.

## D-109 — Settled money: actual block time and fuel added; the rest as previewed
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: At settlement, pilots (and their count), flight attendants and maintenance use the real block time.
  Fuel is the fuel added (block-out fuel − fuel on record, never below 0) at the planned flight's locked price.
  Fares, cargo, charter, landing fee, ATC, gate, handling, GA services and catering are as previewed: the planned
  distance and the plan's block time, even after a diversion. Rates come from `rates.yaml` as it is at settlement
  (D-107).
- Why: Dave's choice. The preview stays honest (design rule 1), and the costs that depend on how the flight went
  follow what happened.
- Alternatives considered: every line locked to the preview except fuel.

## D-110 — The fuel check: 300 lb of slack, at the brake release
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Dave + Claude
- Area: recorder, app
- Decision: A release is refused (`fuel_below_record`) when the sim's fuel is below the airplane's fuel on record
  minus **300 lb**. The fuel on record after a flight is the block-in event's `shutdown_fuel_lb` (when the last engine
  stopped), not the fuel at the brake set. A delivery (0 on record) is never refused.
- Why: design.md §11 says fuel can't be removed. 300 lb covers about an hour of APU at the gate and rounding when
  loading fuel through the PMDG CDU or EFB (Dave). The shutdown fuel is what's actually left in the tanks.
- Alternatives considered: 100 lb (a long turnaround on the APU would be refused); 1,000 lb; checking at block-out
  confirmation instead of at the release.

## D-111 — The Flights screen lists the database's settled flights
- Date: 2026-11-23
- Status: Accepted (supersedes D-060 for the list; the tracks still come from the log)
- Decided by: Dave
- Area: app
- Decision: Flights lists the settled flights from the `flight` table, newest first. Each opens the same summary
  as Live shows after block in: money lines, total and route map. The track is still rebuilt from the event log by
  `flight_id`. Interrupted flights don't appear.
- Why: Dave's choice. Discarded means "as if it never happened" (D-072), and the money belongs with the flight.
- Alternatives considered: leaving the log-based list until later.

## D-112 — The aircraft type is matched by its sim folder
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23; BBJ2 still to see in the sim)
- Decided by: Claude
- Area: recorder, data
- Decision: Resolves D-088's open question. Each catalog type has a `sim_folder` (`PMDG 737-800BBJ2`), copied to
  `aircraft_type.sim_folder`. The sim's aircraft is the folder that holds the loaded `aircraft.cfg` (the
  `AircraftLoaded` path). A type matches if the folder equals its `sim_folder` or starts with it plus a space,
  ignoring case. So `PMDG 777-200ER GE` is a 777-200ER, and `PMDG 737-800BBJ2` is not a 737-800.
- Why: In Dave's Community folder, each PMDG variant has its own SimObjects folder (`PMDG 737-600`, `PMDG 737-800`,
  `PMDG 737-800BBJ2`, `PMDG 737-800BCF`, `PMDG 777-200ER` and its GE/PW/RR folders, `PMDG 777-300ER`, `PMDG 777F`).
  Every Soxxly livery has a `base_container` pointing at its variant's folder. The 2026-11-22 logs show MSFS reporting
  the base folder's `aircraft.cfg` for the Soxxly 737-800 livery. `TITLE` comes from the livery, and `ATC MODEL` is
  `B738` for the BBJ2 too, so neither tells the variants apart. Still to confirm in the sim: the same holds for the
  BBJ2 (the first milestone 6 test).
- Alternatives considered: `TITLE` patterns (livery-dependent); `ATC MODEL` (same for the 737-800 and the BBJ2);
  a list of folders per type (a table for what one prefix rule covers).

## D-113 — Settling reads the flown flight back from the log
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: app, recorder
- Decision:
  - `block_out` carries the `planned_flight_id` it was matched against.
  - The app settles the planned flight when the log has a `block_out` with that id, logged after the flight was
    planned, followed by its `block_in`. It checks on every block event and once at startup.
  - `flight.log_flight_id` is `UNIQUE`, and pairs already settled are skipped, so a flight is never settled twice.
  - The arrival airport is the imported airport nearest the block-in position, at any distance.
- Why: The log is written first and is the record (D-008, D-060). Reading it back means a crash between block in and
  settling loses nothing. The "after it was planned" rule matters because SQLite can reuse a deleted planned flight's
  row id. Diversions to an airport outside the import are rare (4,265 airports kept), so a radius would only add a
  failure case.
- Alternatives considered: settling from the observer's in-memory event (lost if the app dies before settling);
  a radius around the airport with an error when none is found.

## D-114 — Hours are counted from the flights; slots are enforced
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: data, app
- Decision: Dave's hours are the sum of block times over the `flight` table; no stored total. The sketch's
  `pilot_hours` (and D-061's note about stored totals) meant "stored in the database, not counted from 30 days of
  log". Flights are kept forever, so the sum is that. Every flight is Dave's until background flying exists. Fleet
  slots (D-083) are enforced at lease time from these hours: a new airplane waits, with a notice, while the fleet
  (airplanes not returned) fills every slot.
- Why: A stored total would be derived data that could disagree with the flights (D-078).
- Alternatives considered: a `pilot_hours` running total updated at each settlement.

## D-115 — Milestone 6's flight tables
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: data
- Decision: Schema 4 adds:
  - `flight`: one settled flight, written once. It holds the planned and arrival airports, the block times, three
    fuel figures (on record, out, in) and the locked fuel price.
  - `flight_cabin`: passengers per class.
  - `flight_plan`: a copy of the imported `simbrief_plan` row.
  - `flight_money_line`: each line points at its `ledger_line`, like `lease_charge`.
  - 17 new `ledger_category` rows.

  The planned flight is deleted in the same transaction. Block time and fuel added aren't stored; they follow from
  the columns.
- Why: The planned flight is current state (D-103) and the flight is history. Copying the plan keeps each table
  simple. One shared plan table pointed at by both would need either nullable keys or a manual delete on Cancel.
- Alternatives considered: a `simbrief_plan` row referenced by both `planned_flight` and `flight`; storing block
  seconds.

## D-116 — A --probe mode for milestone 6's test flights
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude
- Area: recorder
- Decision: Started with `--probe`, the recorder also logs:
  - GSX's L:vars (`probe_gsx`);
  - PMDG SDK data for the switches in `docs/notes/pmdg-variables-from-spad.md` (`probe_pmdg_737`, `probe_pmdg_777`),
    and the matching SimVars (`probe_simvars`). This needs `BLOCKTIME_PMDG_NG3_SDK_DIR` and
    `BLOCKTIME_PMDG_777X_SDK_DIR` at build time, pointing at the headers in each PMDG package (never committed); each
    half is a stub without its header. The two headers are compiled in separate files, since both define the same
    macros;
  - the origin's and destination's runways and parking spots from the sim's facility data (`probe_facility`).

  Without the flag nothing extra is requested.
- Why: The roadmap puts the probe in milestone 6's sim tests. A flag keeps it off Dave's normal flights (the
  resource budget) and can come out once milestones 8–10 read what they need.
- Alternatives considered: a separate probe build; logging the PMDG data block as raw bytes to decode later.

## D-117 — Review fixes for milestone 6
- Date: 2026-11-23
- Status: Accepted (confirmed by Dave, 2026-11-23)
- Decided by: Claude (from the milestone 6 code review)
- Area: app
- Decision:
  - A brake release refused before the sim matched leaves "Not recording yet: set the parking brake, then release
    it" on Live until the brake is set again, since only a new release can open the flight.
  - A flight isn't settled while `rates.yaml` can't be read. Live says why, and the flight settles at the next block
    event or start once the file is fixed. (The Live estimate still falls back to the built-in rates.)
  - Cancel flight is hidden, and refused, while a flight is being recorded.
  - "No flight is planned" isn't shown over a flight's summary.
- Why: The review found that the first could lose a whole flight with no warning, the second could post permanent
  money at the wrong rates, the third could drop a flight in progress silently, and the fourth showed a warning after
  every good landing.
- Alternatives considered: the recorder reporting whether a release is armed (more plumbing for the same answer);
  settling at the built-in rates with a notice (the lines could never be corrected).

## D-118 — A pre-departure view on Live, after the MVP
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: After the MVP, Live gets a view before block out showing everything needed to set up the FMC and get
  ready for pushback, taken from the SimBrief plan and SayIntentions, and changes to the in-flight view at block
  out. Takeoff performance comes from the PMDG EFB; BlockTime shows its inputs but has no performance calculator.
  The ATIS is polled (around every 5 minutes) so the weather isn't the plan's. Charts stay in Navigraph. The view's
  name and contents are settled in a brainstorm when it is built.
- Why: Today FMC setup needs a laptop with SimBrief and Navigraph beside the sim; most of what's read off it is
  already in the plan or SayIntentions. A calculator would need Boeing performance data that isn't public, and the
  EFB already does it for the airplane being flown. Pulling charts in goes against keeping the app simple.
- Alternatives considered: reproducing SimBrief's takeoff calculator (needs unpublished data, weeks of tuning); reading
  the takeoff report from the plan (fixed at planning time, so stale when conditions change); showing charts.

## D-119 — Fuel burn estimate per airframe: entered, or learned from 3 flights
- Date: 2026-11-23
- Status: Accepted (the 10% padding, averaging all qualifying flights and counting delivery flights are Claude's,
  Proposed)
- Decided by: Dave + Claude
- Area: design
- Decision: Each catalog airframe has an optional fuel burn in lb per great-circle NM, entered by Dave (catalog,
  later the settings screen). BlockTime also learns one: the average burn (block-out fuel − shutdown fuel) per
  great-circle NM over all completed flights longer than 200 NM on airplanes of that airframe, delivery flights
  included, once there are at least 3. The entered value is used unless it's empty or 0; then the learned one; with
  neither, the preview shows "+ fuel cost". The settings screen shows the learned value next to the field. The
  estimate is padded 10%. Previews only: once a plan is imported, SimBrief's burn is used.
- Why: New Flight can't preview fuel cost before a plan exists. Per airframe (not per airplane), so an airplane Dave
  never flies still gets an estimate from its siblings or the entered value, and Dave doesn't have to fly every
  airframe to get one. Great-circle NM because that's the distance a preview has. 200 NM keeps out short legs,
  where taxi and climb dominate. It's only an estimate, so lb/NM plus padding is enough.
- Alternatives considered: per airplane (more to enter; a never-flown plane only gets the entered value); a fixed
  amount per flight plus lb/NM (more accurate on short legs, more machinery than a preview needs); learned only, as
  design.md section 3 first said (no estimate until 3 flights).

## D-120 — Flight predictions on Live, after the MVP
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave (idea), Claude (approach)
- Area: design
- Decision: After the MVP, Live predicts what's ahead (top of climb, top of descent, landing, gate arrival) from the
  SimBrief navlog and the recorder's position, shifting the plan's remaining times by how far ahead or behind plan
  the flight is at each waypoint passed. Taxi-in comes from the leg template. Reading the PMDG FMC's own predictions
  is checked in a probe flight first.
- Why: Dave wants to know roughly when descent starts and when he'll be at the gate. The plan and the recorder
  already have what's needed; the ahead/behind method is how airline ops does it and copes with winds.
- Alternatives considered: computing descent from altitude and distance (the 3:1 rule), which ignores the planned
  route and winds; relying on the PMDG FMC (better, if readable; unknown).

## D-121 — One Claude Code session per milestone, pushed to main at its end
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: process
- Decision: Each milestone gets its own Claude Code session. That session plans, builds, gets Dave's sim checks and
  closes the milestone. Closing it includes pushing `main` to `origin`. The next milestone starts in a fresh session
  from the latest `docs/progress.md` entry. Work still goes straight to `main` (D-038).
- Why: When Dave questions a decision later, he can find the conversation that made it by milestone.
- Alternatives considered: one long session across milestones, which runs through several context compactions and
  is hard to search; pushing after every commit, which publishes half-finished milestones.

## D-122 — Runway and gate positions name things; they never score
- Date: 2026-11-23
- Status: Accepted (including the 40 m gate tolerance: Dave, 2026-11-23)
- Decided by: Dave (rule), Claude (gate tolerance, confirmed by Dave)
- Area: design
- Decision:
  - **Runways:** BlockTime uses the sim's runway geometry only to *name* the departure and arrival runway. It may
    log how far the roll was from the centreline, off the runway heading, or how far down the runway the touchdown
    was. That stays information for the logs and the summary. It never counts toward the flight score (D-040),
    Claude's evaluation, or anything in the airline.
  - **Gates:** the gate Dave parked at is the nearest parking spot to the block-in position, accepted up to 40 m
    away. Beyond that the summary says no gate was matched.
  - **Precision:** neither uses tight tolerances.
- Why:
  - **The sim's positions often don't match the scenery Dave sees.** An ILS CAT III autoland puts the aircraft on the
    sim's centreline, but on many airports that's visibly left or right of the painted one. A visual landing on the
    painted centreline then reads several metres off.
  - **The same applies to parking spots against the gate textures.** FlyHub penalises these deviations and gets them
    wrong, which Dave finds irritating.
  - **Naming has wide margins.** On the first probe flights the next-best runway was at least 115 m off, and parking
    spots were about 45 m apart (`docs/notes/probe-flight.md`).
- Alternatives considered:
  - scoring centreline and touchdown-zone accuracy, as FlyHub does: rejected, because it penalises flying that looks
    right in the sim;
  - a 5–10 m gate tolerance, which a scenery offset would break.

## D-123 — Milestone 7 starts logging the ATC log, the ATIS and the altimeter settings
- Date: 2026-11-23
- Status: Accepted. Dave chose what to log. Claude proposed the fetch timing and the dedupe method, and Dave confirmed
  them on 2026-11-23.
- Decided by: Dave + Claude
- Area: recorder, design
- Decision: From milestone 7 on, every recorded flight collects what milestone 10's analysis (D-040) needs, so real
  flights build up material for it.
  - **The ATC log.** SayIntentions' `getCommsHistory` serves only the current flight, so it's polled while the
    flight is open: about once a minute, and once more at block in.
    - Each call fetches the whole current flight and logs only exchanges not logged yet, because a reply can be
      written a few seconds late (`docs/notes/sayintentions-api.md`).
    - A failed call is retried at the next poll and never blocks the flight.
  - **The ATIS for the origin and the destination,** from `getWX`, during the phases where a pilot uses it, logged
    whenever it changes:
    - the origin's every 10 minutes from block out to takeoff;
    - the destination's every 10 minutes from top of descent to landing.
  - **The altimeter settings,** change-based: the captain's and first officer's setting and whether each is on STD.
    - PMDG's SDK has no baro value (only the hPa/inHg selector, and the 777's STD button), so they come from the
      sim's `KOHLSMAN SETTING MB:n` and `KOHLSMAN SETTING STD:n`.
    - Whether PMDG drives those isn't known yet. The probe logs them (`probe_altimeter`) from 2026-11-23, and
      milestone 7's first sim check confirms they follow the knob and the STD button.
  - **Scoring:** a wrong altimeter setting in the wrong phase counts in the score (design.md §14). Unlike runway
    geometry (D-122), the setting is read from the cockpit itself, not compared against the scenery.
- Why: the analysis checks the altimeter against the ATIS and the transition altitude and level, and reads the
  ATC exchanges. None of it can be fetched after the flight, so it has to be logged from the start.
- Alternatives considered: waiting for milestone 10, which would leave its analysis with no real flights to build
  and test on.

## D-124 — The phase rules' numbers, tuned on two real flights
- Date: 2026-11-23
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: recorder
- Decision: recorder.md's phase rules get these starting values (constants in `phase_detector.h`):
  - **Pushback → Taxi out:** the brake set after block out (the push's end), then 3 kt with the brake off; or 6 kt at
    any time, for a departure without a push. GSX pushes ran at about 4 kt and taxiing at up to 23 kt.
  - **Takeoff** is confirmed at 40 kt on the ground, so a fast taxi never counts. It's timed from where the
    acceleration began: walk back from the 40 kt sample while each earlier sample is at least 0.5 kt slower than
    the next. The sim has no takeoff-roll signal, and on Dave's rolling takeoffs 40 kt came 7-8 s after the roll
    began (Dave asked, 2026-11-23).
  - **Climb** once airborne at 1,000 ft AGL.
  - **Level:** indicated altitude within ±100 ft for 20 s of active time (D-058). It's reported at the nearest
    100 ft of the samples' mean, and ends more than 200 ft away.
  - **Cruise:** a level within 1,000 ft of the plan's cruise altitude. With no plan, a level above 10,000 ft held
    5 minutes.
  - **Top of descent:** VS of −500 fpm or less for 60 s, within 2 × the 3:1 distance (3 nm per 1,000 ft) of the
    destination. A descent farther out is a step down and stays Cruise. A sustained descent during Climb is Descent.
  - **Approach:** within 30 nm and below 10,000 ft, or below 3,000 ft AGL (which covers a diversion).
  - **Go-around:** +500 fpm for 10 s, 200 ft above the approach's lowest point. The next level-off returns it to
    Approach. It's logged as `phase_changed` to `go_around`.
  - **Landing** at the first sample on the ground; **Taxi in** below 30 kt.
  - **Indicated altitude** everywhere: it's what the pilot flies, and ATC's levels show as round numbers in it.
- Why: Replayed on Dave's KBFI→KPDX and KPDX→KSFO flights of 2026-11-23 (the fixtures in `tests/data`), these give
  every phase and level-off he flew. The KBFI leg's cruise at FL200 lasted only about 75 s, and an ATC level-off at
  11,000 ft lasted about 25 s, so recorder.md's suggested 30–60 s hold would miss both. 20 s gave no false
  level-offs on either flight.
- Alternatives considered: a 30 s hold (misses the 11,000 ft level-off); a vertical-speed threshold for level (noisier
  than an altitude band); true or pressure altitude (needs a new SimVar; true altitude was 300–800 ft off the
  levels flown).

## D-125 — Touchdown from sim-frame samples near the ground
- Date: 2026-11-23
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: recorder
- Decision: A second small data definition (vertical speed, on ground, AGL, G force, IAS, position) is requested
  every sim frame, only from 300 ft AGL on the way down until the touchdown is captured, or above 500 ft after a
  go-around. The touchdown rate is the vertical speed of the last airborne frame. The g is the peak in the second
  after contact. A bounce isn't a second touchdown.
- Why: The once-a-second telemetry read −118 and −182 fpm on the probe flights, but that was the last sample before
  contact, up to a second early. Sim-frame sampling for about 20 s per flight is the "fast only briefly" that
  recorder.md asks for.
- Alternatives considered: `PLANE TOUCHDOWN NORMAL VELOCITY`, not yet seen working in MSFS 2020 with the PMDG, and an
  unknown name would drop the whole request; the 1 Hz sample (too coarse).

## D-126 — The recorder names the runways, from the sim's facility data
- Date: 2026-11-23
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: recorder
- Decision:
  - **The data:** the recorder requests the runways of the planned origin and destination at connect, and when the
    planned flight changes. That's a runway-only facility request in the normal build. It logs them as
    `airport_runways`.
  - **The naming:** it names the takeoff runway at liftoff (`liftoff`) and the landing runway at Taxi in
    (`landing_runway`). The roll is the samples on the ground at 40 kt or more. A runway end fits when the roll's
    track is within 20° of its heading and its mean distance from the centreline is 75 m or less. The nearest fit
    wins; with none, the runway is `""`.
  - **The offsets** are logged as information and never scored (D-122).
  - **This replaces recorder.md's line** that matching positions to runways is the app's job. Gates stay open for
    milestone 9.
- Why: On the probe flights the four rolls were named right, 1–4 m off the sim's centreline. The next-best runway was
  at least 115 m away, so 75 m can't pick a parallel. Naming in the recorder puts the answer in the log next to the
  roll, so the analysis and the stages read one file.
- Alternatives considered: OurAirports runway ends from the database (not the sim's geometry); the app naming from
  logged runways (a second place that has to know the roll).

## D-127 — The app's web results reach the log through the recorder thread
- Date: 2026-11-23
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: app, recorder
- Decision:
  - **One writer:** the GUI thread pushes `atis` and `atc_exchange` events onto an `AppEventQueue`, and the recorder
    thread drains it into the event log on each loop, so the log keeps one writer.
  - **The events:** both carry the recorder's `flight_id`. `atc_exchange` holds SayIntentions' entry verbatim, plus
    `si_flight_id`.
  - **The dedupe:** an exchange is keyed by its `id`. It's logged when new, and again when its content has changed (a
    reply written late, D-123). A reader keeps the last line per `id`. ATIS lines are logged when the letter or the
    text changes.
- Why: Two `std::ofstream`s appending to one file from two threads can interleave lines. The queue is a few lines and
  keeps D-008's "the log first" for the web results too. Logging the entry verbatim keeps fields BlockTime doesn't use
  yet, such as `ident` and `is_acars`.
- Alternatives considered: a mutex inside `EventLog` shared by both threads; a second log file for web results (two
  files for the analysis to join); `since_id` polling (skips late replies).

## D-128 — The six stages are built from the log's events
- Date: 2026-11-23
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: app
- Decision: `FlightStages` (plain C++) builds design.md §13.5's six stages from the recorder's events. Live shows them
  in a Flight progress card while a flight is open. The phase with its sub-state ("Climb — level at 11,000 ft") is
  shown in the Flight card.
  - Altitudes read as flight levels from the plan's transition altitude (climb and cruise) or transition level
    (descent).
  - "Landed" carries the runway and the touchdown rate.
  - Pre-Departure stays empty until milestone 8's GSX services, and Arrival shows "Parked" until milestone 9's gate.
  - The summary is unchanged in this milestone.
- Why: Building from the same events the log holds means Live and a later debrief or summary read the same thing.
- Alternatives considered: a separate stage model fed by the observer (a second interpretation of the same facts).

## D-129 — Telemetry logs the true heading, for later milestones
- Date: 2026-11-23
- Status: Accepted (Dave; true rather than magnetic confirmed 2026-11-24)
- Decided by: Dave + Claude
- Area: recorder
- Decision: From milestone 7, the `telemetry` event includes `heading_true_deg`, the sim's `PLANE HEADING DEGREES TRUE`,
  with a 0.5° change threshold. It's logged only; nothing uses it in milestone 7.
- Why: Dave wants sample data in hand before the milestones that need heading. Milestone 9 could compare how he parked
  with the parking spot's heading, and milestone 10's analysis could see crab angle and pushback direction. The track
  from positions can't give these: it differs from heading in a crosswind and during pushback, and doesn't exist
  while stopped.
  - **True, not magnetic:** the sim's runway and parking headings are true, so they compare directly.
  - **No extra log lines in flight:** a sample is already written every second while the position changes.
- Alternatives considered: waiting until a milestone needs it (no real data to design with); magnetic heading (needs
  the variation to compare with the facility data).

## D-130 — Milestone 7's execution calls
- Date: 2026-11-23
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: recorder, app
- Decision: Calls made while building milestone 7 that change what Dave sees or what's logged:
  - **Approach can start from Cruise** as well as Descent. Otherwise a flight whose descent never held −500 fpm for
    60 s would go from Cruise to Landing with no Approach. The destination ATIS then starts at Approach too.
  - **Go-around:** a level confirmed during a go-around returns the phase to Approach and is shown as a level-off.
  - **Level-offs:** a level-off line on Live closes at any phase change except Descent → Approach, which is the same
    stage. The detector drops a level without an end event on a go-around or a landing.
  - **After a step climb,** a new "Cruising at FL410" line starts, so the Cruise stage always shows the current level.
  - **"Landed"** appears at the landing phase. The touchdown rate and the runway fill it in when they arrive, which
    can be a second later, or never if the fast samples are refused.
  - **A pause adds nothing to phase holds (refines D-058).** The sim keeps answering the 5 s heartbeat while paused.
    Counting those replies would let a long pause mid-climb confirm a level-off, or an early cruise, that never
    happened.
  - **The altimeter settings are two SimConnect requests** (millibars and STD), as in the probe, so a refused STD
    name can't take the millibar values with it.
  - **The Live card is `FlightProgress.qml`.** The plan's name, `FlightStages`, is already the C++ class.
- Why: each came up while building or in the whole-milestone review. They're recorded so Dave can confirm or change
  them, and so nobody undoes them by accident.
- Alternatives considered: following the plan to the letter on each point, with the gaps described above.

## D-131 — Start with two fleet slots
- Date: 2026-11-23
- Status: Accepted
- Decided by: Dave
- Area: design
- Decision: The airline starts with two aircraft slots instead of one. The first earned slot still comes at 10 hours
  flown, and each one after costs 10 more hours than the last: slot 3 at 10 h, slot 4 at 30 h, slot 5 at 60 h, and so
  on (design.md §6). Supersedes the "start with 1 slot" part of D-083's slot rule; the rest stands.
- Why: Dave wants a second airplane, a 777-300ER, for test flights now, at about 2.6 hours flown. The game shouldn't
  make him fly 10 hours in one airplane before he can try the other family.
- Alternatives considered: an `extra_slots` line in airline.yaml (a config exception rather than a rule change);
  waiting until 10 hours.

## D-132 — Lease rates cut to about a sixth
- Date: 2026-11-23
- Status: Accepted (the values are Claude's, confirmed by Dave 2026-11-24)
- Decided by: Dave (lower the leases), Claude (the values)
- Area: design
- Decision: Every type's monthly lease is cut to about a sixth (design.md §5): 737-600 $125k, BBJ2 $150k, 737-800
  $235k, 777-200ER $315k, 777-300ER $385k, 737-800 BCF $85k, 777F $500k. The aim is that a plane Dave flies about 15
  legs a month roughly covers its lease.
  - **N814SX** is already leased at $900k, and catalog edits never change a leased airplane (D-082). So its stored rate
    is set to $150k once, by hand, from its next month on. The lease months already charged stay in the ledger:
    history is facts.
  - The starting cash stays $5M.
- Why:
  - The first rates were set before fleet slots existed. They assumed background flights earning from the start:
    "a lease takes roughly half of a busy plane's monthly flight profit".
  - In the MVP only Dave's own legs earn. A BBJ2 VIP leg made about $10.6k (KPDX→KSFO). A 777-300ER leg is estimated
    at about $23.5k (750 nm, 70% full). At the old rates those took about 85 and 98 legs a month to cover.
- Alternatives considered: raising the starting cash (only buys time, about 8 months at $25M with both planes); both.

## D-133 — A level close to a thousand is that thousand
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave (round it when it's really close; within 90 ft), Claude (the rule)
- Area: recorder
- Decision: A level-off's altitude, the cruise level included, is its samples' mean snapped to the nearest 1,000 ft
  when within 90 ft of it (`kLevelSnapFt`), else rounded to the nearest 100 ft as before (refines D-124). The snapped
  value is also the reference the level is left from (200 ft away).
- Why: N916SX's cruise read "Cruising at FL369" while the altimeter showed 36,994 and then 37,001. The 777 crept up
  the last 100 ft at about 150 fpm, and the 20 s window's mean (36,926) held that creep. The same rule makes the KSFO
  replay's approach level 3,000 (it read 3,100 while ending at 3,000).
  - **Why 90 ft, not 150:** Claude first proposed 150 ft. On the same flight ATC assigned 8,100 ft for KSLC's missed
    approach, and Dave held it; the window's mean was 8,095. At 150 ft that would have read 8,000.
- Alternatives considered: 150 ft (turns real 100-ft levels into thousands); the window's last sample instead of its
  mean (fixed FL369 but read the 4,500 ft level as 4,600); snapping to 500s; the plan's cruise altitude when near it
  (cruise only, not ATC levels).

## D-134 — The Flight progress card spans the gate
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: Live's Flight progress card no longer shows only between block out and block in. Before block out it
  shows the next leg (Pre-Departure current, GSX services as they start) whenever a flight is planned or a service has
  run. After block in it stays under the summary with the flight's Arrival stage: Parked, Deboarding, and "Ready for
  shutdown or turnaround". It switches to the next leg at At Gate Turnaround, Done for now, or a new plan.
- Why: GSX's catering, refuelling and boarding happen before block out and deboarding after block in, when the card
  was hidden.
- Alternatives considered: the Arrival lines and checklist inside the summary card.

## D-135 — The shutdown checklist reads plain SimVars only
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app, recorder
- Decision: "Ready for shutdown or turnaround" lists engines off, parking brake set, deboarding complete (only when GSX
  deboarding was used), beacon off and strobes off. No seat belts and no APU on either family. Beacon and strobe come
  from `LIGHT BEACON` and `LIGHT STROBE`, added to the main telemetry. Refines D-073's checklist.
- Why: The probe flights showed both SimVars follow the 737's and the 777's switches. The 737's seat belt sign and APU
  are only in PMDG's SDK data, and the 777's APU is unknown; Dave chose not to depend on PMDG's SDK for this.
- Alternatives considered: reading PMDG's SDK broadcast in normal running for seat belts and APU (milestone 10 may
  still need it for procedure capture).

## D-136 — No "Turnaround and Boarding" line
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: If no GSX service was completed before pushback, Pre-Departure simply shows as done with no lines.
  Supersedes D-073's "Turnaround and Boarding" line (design.md §13.5).
- Why: Dave: "Simply mark pre-departure complete, no other lines needed."
- Alternatives considered: the line with the gate time as its start and end; the line with no times.

## D-137 — Each flight stores its phase and GSX service times
- Date: 2026-11-24
- Status: Accepted (tables Proposed)
- Decided by: Dave (store them now), Claude (the tables)
- Area: data
- Decision: Migration 5 adds `flight_phase` (each phase's start, in order) and `flight_service` (each completed GSX
  catering, refuelling, boarding and deboarding, with start and end). Phases and pre-departure services are written
  when the flight is settled. Deboarding comes after block in, so its row is added when it completes (and at startup
  if the app missed it). Flights settled before 0.9.0 get none.
- Why: Leg templates (D-075) need taxi and GSX turnaround times, and the event log is deleted after 30 days. Taxi out
  and in follow from the phases (taxi_out → takeoff, taxi_in → arrived).
- Alternatives considered: deferring with a note in the roadmap; keeping the event logs forever. Columns for taxi
  times on `flight` (would leave the other phases for a later migration). Backfilling old flights from the log
  (only one flight, N916SX's, has phase events).

## D-138 — Milestone 8's GSX rules
- Date: 2026-11-24
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude, confirmed by Dave
- Area: recorder, app
- Decision:
  - **The recorder** reads the five GSX states and the boarding and deboarding passenger totals always (not only with
    `--probe`), once a second on change. It logs `gsx_service` (`service`: catering, refuelling, boarding, deboarding,
    pushback; `state`: available, not_available, bypassed, requested, performing, completed; `time`; `passengers` for
    boarding and deboarding) on each state change. A 0 (GSX not running) is ignored, and so is a service's first
    state when it is "available".
  - **A service** starts when it reaches performing (5) and is complete when it then goes to completed (6) or back to
    available (1): boarding and pushback skip 6. Bypassed (3) or not available (2) while performing abandons it: it
    isn't shown or stored. Requested alone is not a line, and a performing service that goes back to requested stays
    running (never seen).
  - **Which leg:** catering, refuelling and boarding belong to the next block out, if they started after the later of
    the last block in and the last `sim_stopped`/`sim_disconnected`. BlockTime's own exit (`sim_disconnected`
    `recorder_exit`) doesn't count: the sim session carries on, so a restart between legs keeps the services done. Deboarding belongs to the last block in, until
    the next block out. An interrupted flight drops its services (D-072).
  - **Pushback:** the Departure line reads "Pushback (GSX)" when GSX's pushback was requested or performing at block
    out. It isn't stored as a service: the phases already time it.
  - **Ready after a late deboarding:** if GSX deboarding starts after the shutdown checks passed (the beacon goes off
    before Dave calls GSX), Ready disappears while it runs and comes back when it completes (design.md §13.5).
  - **The `arrived` phase** is stored at the block-in time: the recorder logs it just after `block_in`, which
    settling runs on.
  - **Passenger counts** are logged, not shown or stored: GSX's count is its own (285 on N916SX, which SimBrief loaded
    with 370).
- Why: Matches what the probe flights showed (`docs/notes/probe-flight.md`). Logging each state keeps the log the one
  record; the app derives spans the same way for Live and for settling.
- Alternatives considered: the requested time as the start (catering sat at requested for minutes waiting for its
  truck); logging every passenger count change (boarding logs one line every few seconds).

## D-139 — Live in three columns
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: In milestone 9, the Flight progress card moves out of the full width into Live's row, so the row has
  three columns: Flight progress on the left, the flight panel in the middle, the running event log on the right.
  The planned flight header stays full width above it; the flight summary is unchanged.
- Why: The progress card only ran full width because milestone 8 put it under the header (D-134); nothing needs it
  that wide, and side by side the stages, the live data and the log all fit on screen during a flight.
- Alternatives considered: today's layout (progress full width under the header, then data and log in a row).

## D-140 — Live's flight panel: fewer cards, status tiles, Mach at altitude
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: In milestone 9, Live's flight panel changes:
  - the Aircraft card (title and registration) and the Position tile (GROUND / AIR) go;
  - Parking brake becomes a status tile: green when set, grey when released;
  - "Engines running" becomes one tile per engine, 1 on the left and 2 on the right: green when running, grey when
    not;
  - the airspeed tile switches to Mach (the sim's `AIRSPEED MACH`, shown as M.78) once Mach reaches 0.70, and back
    to knots once it drops below 0.66. The gap stops it flicking at the line in turbulence. At a 290-300 kt climb
    0.70 comes around FL220, between a 0.65 line (about FL200) and the airplane's own speed window (about FL280).
- Why: Ideas from Dave's first 0.9.0 test flight (KSLC to KLAS, in cruise): the dropped cards repeat what's shown
  elsewhere, and status reads faster as colour than as text.
- Alternatives considered: keeping today's text tiles ("SET" / "OFF", "2 / 2"). For Mach: following PMDG's MCP
  speed window (`MCP_IASMach`; ties the tile to PMDG's broadcast), a single 0.65 line (flickers at the line), 0.65
  with a gap to 0.63 (Dave chose 0.70 / 0.66 as a middle point).

## D-141 — Ask for the gate after SayIntentions picks its own
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: Supersedes D-074's timing and runway source. BlockTime waits until SayIntentions has chosen its own
  arrival gate (`assigned_gate` appears in the local flightJSON, usually in cruise). By then SayIntentions has also
  set the landing runway (`flight_plan_arriving_runway`, `atc.approach.runway`) and the runway transition, so
  BlockTime picks its gate with D-074's rules against that runway, sends it with `assignGate`, and reads the result
  back (`getParking` or flightJSON `assigned_gate`). If SayIntentions already chose the same gate, nothing is sent.
- Why: SayIntentions decides the gate, runway and transition together, well before top of descent, and only says
  the gate on the ground. Waiting for that decision gives the runway ATC will actually use. Since the gate hasn't
  been broadcast yet, Dave expects `assignGate` simply to replace it; he'll confirm on SayIntentions' status screen
  in milestone 9's sim test.
- Alternatives considered: D-074's "around top of descent or 200 nm out" with the runway from a fresh ATIS or the
  plan (the runway could differ from SayIntentions' own choice, and SayIntentions has already picked by then).

## D-142 — Approach starts at the first flap extension
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: In milestone 9, Cruise/Descent → Approach becomes the first flap extension below 10,000 ft, with
  "below 3,000 ft AGL" kept as the fallback. The "within 30 nm and below 10,000 ft" rule goes. Which flap value to
  read is checked first with a probe: whether the sim's flap SimVars (for example `FLAPS HANDLE INDEX`,
  `TRAILING EDGE FLAPS LEFT PERCENT`) follow the PMDG 737 and 777 flap levers.
- Why: On 2026-11-24's KSLC-KLAS flight, Approach started at 17:49Z, 30.2 nm from KLAS in a straight line and just
  under 10,000 ft, while Dave was still flying the arrival, not the approach. (Correction, same day: the FMC also
  showed 30 nm to go then, so the route wasn't much longer than the straight line; the trouble is that 30 nm and
  10,000 ft is a distance, not the start of the approach.) The first flap setting is when the approach actually
  starts, on any route, without PMDG's data.
- Alternatives considered: the FMC's distance to destination along the route (PMDG's `FMC_DistanceToDest_nm`; ties
  the phase to PMDG's broadcast); keeping the 30 nm rule with a smaller distance.

## D-143 — "Final approach" when lined up with the runway
- Date: 2026-11-24
- Status: Accepted (limits accepted by Dave, 2026-11-24); runway source superseded by D-145, the 1,000 ft line by D-146
- Decided by: Dave (the line), Claude (limits, accepted)
- Area: recorder, app
- Decision: In milestone 9, Live's "Final approach" line under Approach comes when the aircraft is lined up with
  the landing runway, not at 1,000 ft AGL. Proposed limits, to tune in the plan: within 10 nm of the runway
  threshold, track within 10 degrees of the runway's heading, and within 0.5 nm of its extended centreline, held
  for 10 seconds. The runway is SayIntentions' landing runway (D-141), else the plan's; its threshold and heading
  come from the airport data. Proposed: the 1,000 ft AGL `final_approach` marker stays in the log (the stabilised
  approach gate) but no longer adds a Live line, so there is one "Final approach" line, not two.
- Why: Dave wants Live to show when he's on final, which is well before 1,000 ft (about 3,000 ft AGL at 10 nm).
- Alternatives considered: keeping only the 1,000 ft line; the localizer capture (PMDG's annunciators, and RNAV
  approaches have none).

## D-144 — Flight Board and Journeys, after the MVP
- Date: 2026-11-24
- Status: Accepted (marked parts Proposed)
- Decided by: Dave + Claude
- Area: design
- Decision:
  - A split-flap style **Flight Board** lists background flights in the air, planes in a turnaround and each
    journey's next leg; greyed out when there's nothing. It replaces section 8's two boards.
  - Clicking a flight takes it over: a parked plane's next leg; in the air, **reset** (back to origin, Dave flies that
    leg) only more than 40 nm from the destination, or **claim**. After an At Gate Turnaround the rotation's next leg
    is queued for Dave.
  - **Journeys:** a one-way sequence of legs only Dave flies. An inactive airplane at the first origin starts it and is
    locked to it. **Pausing** keeps the journey's place and returns the airplane to Inactive; **any** airplane parked
    at that airport, of any type, resumes it. As many journeys as there are airplanes for them.
  - A planning form (Origin, Destination, Direct distance; five rows, "Add more legs..."), and a journey page with a
    map: actual tracks for flown legs, dotted great-circle lines for the rest.
  - Proposed (Claude): the airplane's state is "On journey"; each row's origin is the previous destination; airport
    names, a total, row removal and range/runway warnings on the form; rows flip only on change and never while
    recording; journey legs count toward leg templates and rotation unlocks; the unlock rule and curfew don't apply.
- Why: Dave wants to see the airline at work and step into any flight, and to fly long multi-leg trips that need
  aircraft positioned along the way (a 737 across America, a 777 over the Atlantic). Planning leg lengths in
  Navigraph is slow; the airport import already has the coordinates.
- Alternatives considered: reset only, without claim; a journey tied to one airplane or one type (any airplane makes
  positioning part of the trip); leaving the journey airplane Inactive between legs (could be put on a rotation and
  lose its place).

## D-145 — "Final approach" names whichever destination runway Dave is lined up with
- Date: 2026-11-24
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Dave + Claude
- Area: recorder
- Decision: Supersedes D-143's runway source. The recorder checks every runway end at the destination (the sim's
  runways, which it already loads for D-126) against D-143's limits and names the one Dave is lined up with: "Final
  approach, runway 26L". Close parallels go to the nearer centreline. SayIntentions' and the plan's runway aren't
  used for this.
- Why: Dave asked whether to use SayIntentions' runway (else the plan's), then check which runway he's lined up
  with, and go with that one when they differ. In every case the lined-up runway wins, and when he's lined up with
  none there is no line either way, so the expected runway never changes the answer. Checking every runway gives
  the same result with one input fewer, and it's right when ATC changes the runway late. SayIntentions' runway is
  still used where it decides something: picking the gate (D-141).
- Alternatives considered: D-143's SayIntentions-or-plan runway only (no line after a late runway change); the
  expected runway first with the lined-up one as an override (same answer, more inputs).

## D-146 — Keep a Live line at 1,000 ft
- Date: 2026-11-24
- Status: Accepted; the text superseded by D-149
- Decided by: Dave (keep it), Claude (the text, proposed)
- Area: app
- Decision: Replaces D-143's proposal to drop it. The 1,000 ft AGL `final_approach` marker keeps its Live line under
  Descent, renamed "1,000 ft above ground", next to D-143's "Final approach, runway 26L".
- Why: Dave wants both. The old text "Final approach" now belongs to the lined-up line; "stabilised gate" would be
  confusing in a milestone about parking gates.
- Alternatives considered: dropping the line (D-143's proposal); "Stabilised approach gate".

## D-147 — Milestone 9's gate rules
- Date: 2026-11-24
- Status: Accepted (Dave, 2026-11-24); kinds, preferred gates and closest to the runway superseded by D-155
- Decided by: Claude
- Area: app, recorder
- Decision:
  - **Names.** MSFS's parking name codes: 0 none, 1 Parking, 2–9 N/NE/E/SE/S/SW/W/NW Parking, 10 Gate, 11 Dock,
    12–37 Gate A–Z. Shown as "Gate E7", "Gate 46", "Parking 70". Sent to SayIntentions in short form, letters and
    digits only: "E7", "46", "70". The suffix is ignored (always 0 at KLAS and almost everywhere else).
  - **Size.** A spot fits when twice its radius is at least the type's wingspan. The wingspan is a per-type
    `wingspan_m` in catalog.yaml, not per family (a 777-300ER is 4 m wider than a 777-200ER). The BBJ2 parked at
    KSFO's D14, radius 18 m, with a 35.8 m wingspan.
  - **Kinds.** Passenger and VIP flights use gates (types 8–10). Charters and deliveries use GA ramps (2–4),
    freighters cargo ramps (5); then any ramp (2–5), then a gate. Military spots (6–7) are never picked.
  - **Preferred gates** (`preferred_gates` in airline.yaml, by ICAO, short names) apply only to gate flights, so a
    charter never lands on the airline's gates.
  - **Closest to the runway** means the shortest distance to SayIntentions' landing runway as a line from end to end;
    with no such runway, to the airport's reference point.
  - **SayIntentions' own gate** is matched to an MSFS spot by position (within 40 m, like the parked spot). If it's
    the picked spot, nothing is sent.
  - **One try per flight.** If `assignGate` answers "Unknown gate" or fails, SayIntentions keeps its own gate and
    Live says so. SayIntentions' names aren't always MSFS's: its KLAS "Terminal 3 Gate E7A" is MSFS's Gate E7, 16.6 m
    away. Milestone 9's first sim leg shows whether "E7" is accepted there.
  - **The spot list isn't logged.** It's the scenery's static data (250+ spots); the picked spot and the parked spot
    are logged. The recorder passes the list to the app through its observer.
- Why: D-074 and D-141 set the order; these are the details the plan needs, from the probe flights' spot data.
- Alternatives considered: wingspan per family (the 777s differ too much); preferred gates for every kind;
  retrying `assignGate` with SayIntentions' own spelling (no endpoint lists its names: `getAirport`'s answer is
  undocumented, so milestone 9's probe logs it once).

## D-148 — Milestone 9's recorder calls
- Date: 2026-11-24
- Status: Accepted (Dave, 2026-11-24)
- Decided by: Claude
- Area: recorder
- Decision:
  - **Flaps (D-142).** "Flaps out" is `TRAILING EDGE FLAPS LEFT PERCENT` above 1% (flaps 1 is 2.5% on the 737).
    `FLAPS HANDLE INDEX` is logged too, so the sim flights show whether either follows PMDG's lever. Telemetry also
    gains `AIRSPEED MACH` for D-140.
  - **The parked spot is a fact of the block event.** `block_out` and `block_in` carry `parking` ("Gate E7"): the
    nearest spot of the loaded airports within 40 m, or "" (D-122). The flight table stores both, NULL for "".
  - **The lined-up check (D-143, D-145)** uses the track over the ground from positions at least 150 m apart, not
    the heading, so a crab in a crosswind doesn't stop it. It runs in Descent and Approach, airborne, and once per
    approach: a go-around lets it fire again.
- Why: Each is the simplest rule the existing data supports; the sim flights check them.
- Alternatives considered: `FLAPS HANDLE INDEX` as the rule (unknown on PMDG); the parked spot worked out later from
  the block position (the app doesn't keep the spot list); the true heading for the lined-up check (off by the crab
  angle, up to 10°).

## D-149 — "Final approach" becomes "Short final" at 1,000 ft
- Date: 2026-11-24
- Status: Accepted (the form accepted by Dave, 2026-11-24)
- Decided by: Dave (the idea), Claude (the form)
- Area: app
- Decision: Supersedes D-146's text. Under Descent, "Final approach, runway 26L" runs from the lined-up check (D-143,
  D-145) until the 1,000 ft AGL `final_approach` marker, which ends it and starts "Short final", ending at touchdown.
  Without a lined-up check, "Short final" still starts at 1,000 ft. A go-around closes both. Each approach gets its own
  pair: a go-around re-arms the 1,000 ft marker and the lined-up check (fixed after the milestone 9 review).
- Why: Dave: "the 1000 ft point could change it from Final Approach to Short Final". Start and end times show both
  stretches of the approach, as the other Live lines do.
- Alternatives considered: D-146's separate "1,000 ft above ground" line.

## D-150 — The Flight Summary and the dashboard; Pre-Departure becomes Pre-Flight
- Date: 2026-11-24
- Status: Accepted (after the MVP)
- Decided by: Dave
- Area: design
- Decision: Two screens with fixed names: the **Flight Summary** (a finished flight, from Flights or at block in) and
  the **dashboard** (the Live Flight Screen, used while flying). The first stage is renamed **Pre-Flight**. The
  dashboard changes by itself with the phase of flight; once in the sim, Dave never clicks BlockTime.
- Why: The brainstorm kept mixing the two up. Dave flies with BlockTime beside the sim and doesn't want to switch to it.
- Alternatives considered: one Live page with fixed columns (D-139, today's layout).

## D-151 — The dashboard header: identity row, route arc, phase pill
- Date: 2026-11-24
- Status: Accepted (after the MVP); the parts marked Proposed in `docs/dashboard.md` are Claude's calls
- Decided by: Dave + Claude
- Area: design
- Decision: A card across the top (`docs/dashboard.md`): a three-column identity row (ICAO code and flight number;
  airline name, flight, cities; block clock with block out and block in in local time, block in an estimate until
  known), a thin line, a large arc from origin to destination that acts as a progress bar coloured by altitude, and a
  full-width pill with one section per stage (grey, yellow in progress, green done). Claude's calls (Proposed): the
  marker moves by distance along the planned route; sections about equal in width; the pill's small divisions are the
  recorder's phases, never GSX or level-offs; the line sits between the identity row and the arc.
- Why: Readable at a glance while flying; the arc gives the flight a shape; equal sections keep short stages visible.
- Alternatives considered: the block clock at the pill's right end (it would shift the pill at block out; moved into the
  header instead); a two-line identity block centred above the arc; the callsign (SOCKS) in the header (left out).

## D-152 — Three status lines under the phase pill
- Date: 2026-11-24
- Status: Accepted (after the MVP)
- Decided by: Dave + Claude
- Area: design
- Decision: Three lines centred under the pill: the current line in white, the previous one smaller and light grey, the
  one before smaller and darker grey; each new line pushes the others down. Pre-Flight starts with "Preparing aircraft
  for flight", which a GSX request replaces with one shared line for every running GSX service ("Catering complete ·
  Boarding in progress"); it stays current until block out. Level-offs briefly replace the climb line. Local times.
- Why: One glance tells Dave what's happening and what just happened, without an event log on screen. One GSX line
  avoids flicker, and GSX stays optional.
- Alternatives considered: a single status line; separate lines for each GSX service; the running event log (D-139).

## D-153 — The Pre-Flight panel
- Date: 2026-11-24
- Status: Accepted (after the MVP); the clearance card depends on what SayIntentions exposes (to probe)
- Decided by: Dave + Claude
- Area: design
- Decision: Builds on D-118. At the gate the dashboard shows a load sheet (block fuel as the total, ZFW, passengers by
  class, cargo by hold at the configuration's ratios; pounds; no CG), the departure conditions (runway from the plan,
  then the ATIS; heading, condition, wind, OAT, altimeter), the clearance (the plan's full SID matched to
  SayIntentions' SID name, initial altitude, squawk, departure frequency, taxi route, a 30-minute expiry countdown
  that disappears at block out) and the current and next frequency, which stays on the dashboard all flight.
  Squawk, departure frequency and the countdown's colours were Claude's additions.
- Why: Everything needed to set up the FMC and EFB in one place, so BlockTime is the only window open beside the sim.
- Alternatives considered: the frequencies only from OurAirports (no departure or center frequencies); showing CG
  (Dave doesn't need it).

## D-154 — At block in the header stays and the Flight Summary fills the space below
- Date: 2026-11-24
- Status: Accepted (after the MVP); supersedes part of D-134 and D-135 when built
- Decided by: Dave
- Area: design
- Decision: At block in the dashboard keeps its header, filled in, and only the panel below changes to the Flight
  Summary. The status line always becomes "Parked at gate", changes to "Deboarding in progress" only if Dave starts GSX
  deboarding, then to "Ready for next flight".
- Why: The flight's identity and timeline stay in view while the summary appears; deboarding is optional like the
  other GSX services.
- Alternatives considered: today's switch of the whole page to the summary at block in (D-134, `design.md` 13.7).

## D-150 — Milestone 9's smaller calls, and every QML size in Theme.qml
- Date: 2026-11-24
- Status: Accepted
- Decided by: Claude (calls), accepted by Dave; Dave (all sizes in Theme), Claude (the tokens)
- Area: app
- Decision:
  - Accepted as built in milestone 9: the load-in text keeps the airport name ("Load in at KLAS, Harry Reid
    International Airport, Gate E7"); status tiles read SET/OFF and ON/OFF; Live's side columns are 340 and 300 px;
    the app's events are told back to Live after they're logged; wingspans live only in catalog.yaml; an
    `assignGate` success without `assigned_gate_name` counts as an error.
  - Every size in the QML now comes from Theme.qml (D-054 applied fully): a spacing scale `gap2` to `gap14` below
    `Theme.spacing` (16), and named sizes for lines, controls, dots, tiles, tables, columns and the window. Each token
    has the value the literal had, so nothing moved. Only `spacing: 0` stays a literal.
- Why: Dave approved the calls after milestone 9's build; he asked for the sizes in Theme.qml for consistency, after
  the review found new literals.
- Alternatives considered: semantic names only, no scale (dozens of one-off names for 4, 8 and 12); leaving the
  older files' literals (inconsistent).

## D-155 — Keep SayIntentions' arrival gate unless Dave prefers another or it's too small
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave (keep SayIntentions' gate, favourite spots), Claude (the details)
- Area: app
- Decision:
  - BlockTime no longer picks from the whole airport. **SayIntentions' gate stands** unless:
    - **a preferred spot** for that airport (`preferred_gates` in airline.yaml) fits: the first one listed, unless
      SayIntentions' gate is already one of them. Preferred spots now work for every flight kind: gates for passenger
      and VIP flights, ramps (types 2–5) for charters, deliveries and freighters, so Dave can list favourite ramps too;
    - **SayIntentions' gate is too small** for the type's wingspan: then the nearest spot of the same kind (gate or
      ramp) that fits, measured from SayIntentions' gate. If none fits, SayIntentions' gate stands.
  - **A send counts only when the read-back moves.** `assignGate` can answer without an error and change nothing, so
    the result is "assigned" when `getParking` reads back a spot more than 40 m from SayIntentions' own gate and
    "ignored" when it reads back SayIntentions' own; "unconfirmed" with no read-back. Live names the gate ATC will
    give: "Arrival gate: SayIntentions' Terminal 3 Gate E7A", "Arrival gate: Gate D14 (preferred)", or "Arrival gate:
    SayIntentions' Terminal 3 Gate E7A (it didn't take Gate D14)".
  - **Where Dave parked is still the fact** (D-148): `block_in` and the flight's `arrival_parking` keep the spot he
    actually stopped at, whatever was assigned.
  - The `getAirport` probe is removed: it answered `[]` at KLAS and KSAN.
  - The 3,000 ft AGL Approach fallback stays as it is: over a ridge it starts Approach early (16 nm out at KLAS), which
    Dave accepts. Final approach and Short final are what matter.
- Why: Milestone 9's sim test. Closest to the landing runway picked KLAS "Gate 239", a mid-field stand, and KSAN
  "Parking 70", which the scenery shows as a construction zone: MSFS's parking data lists spots the scenery doesn't
  have. SayIntentions sends GA flights to charter ramps and freighters to cargo ramps, so its gate is the better
  default. At KLAS, `assignGate("239")` gave no error but SayIntentions kept E7A while Live showed Gate 239.
- Alternatives considered: keep picking but only near SayIntentions' gate (still blind to the scenery); no
  assignment at all (loses Dave's favourite spots); comparing the read-back by name (SayIntentions' names aren't
  MSFS's: "106" for Parking 70); measuring the AGL fallback from the field's elevation (Dave doesn't need it precise).

## D-156 — The debrief: BlockTime's code scores, Claude Opus 5.5 explains
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app, design
- Decision: At settling, BlockTime's own code runs the procedure checks and computes both scores (the landing's stars
  and the 0–100). Claude (`claude-opus-5-5`, through the Messages API) gets the checks, a timeline of the flight and the
  ATC log, and writes the review: what went well, what to work on, the landing explained, and ATC compliance. Claude
  never sets a score. Supersedes D-040's "Claude returns a 0–100 score". Design: `docs/debrief.md`.
- Why: A spike on the KLAS → KSAN log (2026-11-24) wrote both versions. Code gives the same score for the same flight,
  so the running average means something, and the rules can be tested. Claude's value is in the text: reading the ATC
  log and connecting cause and effect. Dave: this also lets the scoring be tuned without paying for more Claude calls.
  Opus 5.5 over Sonnet 5.5: about $0.12–0.16 a flight against $0.06–0.08, a couple of dollars a month, for better
  reasoning.
- Alternatives considered: Claude setting the 0–100 (D-040 as written; the number drifts between runs); no Claude
  (free, but no ATC reading or insight); Sonnet 5.5 or Haiku 5.5.

## D-157 — The procedure checks; ATC compliance is information only
- Date: 2026-11-24
- Status: Accepted (Claude proposed the list, Dave accepted it)
- Decided by: Dave + Claude
- Area: app, design
- Decision: Twelve scored checks (`docs/debrief.md` section 2): beacon before pushback, seat belt sign from taxi to
  10,000 ft, taxi speed, origin altimeter, lights for takeoff, 250 kt below 10,000 ft, STD by the transition altitude,
  seat belt sign for the descent, destination altimeter, landing lights below 10,000 ft, stable at 1,000 ft AGL, strobes
  off after landing. Plus a landing section with its own rating. **ATC compliance is never scored for now**: Claude
  reports it as information. A go-around is never marked down. The score never touches money or anything else in the
  airline.
- Why: Dave: SayIntentions sometimes clears him to something wrong that he has to ignore, so compliance can't count yet.
  No penalties follows design rule 2 (D-072).
- Alternatives considered: scoring ATC compliance; letting the score affect the airline.

## D-158 — The landing gets 0 to 5 stars
- Date: 2026-11-24
- Status: Accepted (stars: Dave); the bands are Proposed (Claude)
- Decided by: Dave + Claude
- Area: app
- Decision: The landing is rated in whole stars, 0 to 5, by fixed rules: bands by sink rate (240, 360, 600, 900,
  1,200 fpm), one star back at 15 kt or more of crosswind, minus one for each bounce, bank above 5°, pitch outside 0° to
  8° (737) or 7.5° (777), and speed outside VREF ±10 kt; 1.7 G caps it at 2 stars and 2.0 G at 1. Each star below 5
  takes points off the overall score (0, 4, 8, 14, 20, 30).
- Why: Dave: "the landing score doesn't need to be 0-100 … even something silly like a 0-5 star rating." The bands put
  the spike's 591 fpm, 1.47 G landing at 3 stars.
- Alternatives considered: 0–100 or 0–10 for the landing; Claude rating the landing.

## D-159 — PMDG's SDK broadcast is read on every flight
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: recorder, build
- Decision: The recorder reads PMDG's broadcast in normal running, not only with `--probe`, for the values the checks
  need: the 737's seat belt selector and retractable landing lights, the FMC's VREF and landing flaps on both families,
  and the autopilot. A build without PMDG's SDK headers leaves those values out of the log and the checks that need them
  show as not checked. Refines D-135, which kept the shutdown checklist to plain SimVars (that checklist doesn't change).
- Why: Those values aren't in any plain SimVar (probe flights, `docs/notes/probe-flight.md`). Dave is fine with a public
  build lacking them.
- Alternatives considered: plain SimVars only, dropping the 737's seat belt check and every VREF check.

## D-160 — Measurements are stored; scores are computed when shown
- Date: 2026-11-24
- Status: Proposed
- Decided by: Claude (following Dave's point about tuning)
- Area: app, data
- Decision:
  - Each flight stores what each check measured (seconds late, knots, fpm), or why it couldn't be measured, and the
    landing's values, written once. The rules that turn them into passes, stars and points live in one code file and
    run whenever a score or an average is shown, so a changed rule re-scores every flight.
  - Score: 100, minus 6 per missed check, minus the landing's points, never below 0.
  - The tolerances in `docs/debrief.md` section 2 (beacon within 10 s, taxi ≤ 30 kt, altimeters within 1 hPa,
    260 kt for more than 10 s, STD within 60 s, strobes off within 180 s, the stable approach's VREF −5/+20 and
    1,000 fpm).
  - A changed *measurement* applies to flights settled after it; stored rows are never rewritten. The replay tool
    (`blocktime-debrief.exe`) shows a change on saved logs first.
- Why: Dave wants to play with the scoring without more Claude costs; storing results would freeze each flight's score
  at the rules of the day it flew. Claude's text gives no numbers, so it stays true after a re-score.
- Alternatives considered: storing pass/fail and the score; recomputing everything from the event log (lost after 30
  days).

## D-161 — How the review is written
- Date: 2026-11-24
- Status: Proposed
- Decided by: Claude
- Area: app
- Decision:
  - One automatic try in the background at settling. If it fails, a Retry button; nothing retries on its own across
    restarts, so a failing call never spends money unseen.
  - Input: the flight's timeline, the checks and landing, the ATIS texts, the ATC log's `COM` lines labelled by side
    (`incoming_message` is Dave's side, `outgoing_message` the station's), and the missed checks of the last five
    flights for habits.
  - Output: structured JSON with four parts (`summary`, `work_on`, `landing`, `atc`), stored once with the model id.
  - The request: raw HTTPS from Qt (no official C++ SDK), adaptive thinking, effort set explicitly (medium to start),
    server-side refusal fallbacks on, `claude_api_key` in settings.json as a secret.
  - Flights flown before milestone 10 get no debrief.
- Why: Each call costs money, so retries are Dave's choice. Four fixed parts let the summary lay them out. The habit
  memory is what code can't see across flights.
- Alternatives considered: retrying at every startup; free text; regenerating a review.

## D-162 — Milestone 10's build calls
- Date: 2026-11-24
- Status: Proposed
- Decided by: Claude
- Area: app, recorder, data
- Decision:
  - **The debrief is a second view of the Flight Summary**, behind a Debrief / Flight button in its header; the score
    and stars are always in the header. The summary is about 520 px wide on Flights: too narrow for the debrief beside
    the money and the map.
  - **Bounces are their own event**, 15 s after touchdown, so the touchdown line on Live isn't delayed.
  - **Claude's timeline opens at the engine start (if before block out) or block out**, with one line giving the
    cockpit state then; the 30-minute lead-in only sets that state, because it can reach the previous leg's landing.
    The same "flight start" is the beacon check's deadline.
  - **The measurements are taken at settling, once the arrival is known**, so "diverted" is decided where the money is.
  - **A pre-0.11.0 flight settled after the upgrade is still measured** from whatever its log holds; the missing feeds
    show as not checked.
  - **The real-flight fixture** (`tests/data/flight-klas-ksan/`) is Dave's KLAS → KSAN leg in the 0.11.0 format, with
    cabin announcements and audio links removed; `tests/data/README.md` lists what the script added.
  - From the final review:
    - **a debrief that can't be measured or written never blocks settling**: the measurement runs in a try and the
      write behind a savepoint, so the money settles and the flight shows no debrief;
    - **the 737's landing lights are not checked without PMDG data** (checks 5 and 10), like its seat belt sign;
    - **check 10's window starts again at a go-around**;
    - **lights for takeoff are read 10 s into the roll**, so lights switched on as thrust comes up count;
    - **the pitch is stored raw** (`sim_pitch_tenths`) and flipped in the scoring, so a wrong guess about the sim's
      sign is one constant and every flight re-scores;
    - **a `cockpit` snapshot is written at block out**, so a long turnaround doesn't leave the push's state unknown;
    - **Retry reads 30 days of logs**, as long as they're kept.
- Why: Each settles something `docs/debrief.md` left to the builder.
- Alternatives considered: the debrief beside the money on Live only (Flights would lack it); bounces inside the
  touchdown event (a 15 s delay on Live); the timeline from 30 minutes before block out (the previous leg's events read
  as this flight's).

## D-163 — After the MVP: replay first, then the dashboard in two milestones
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave + Claude
- Area: design
- Decision: Milestone 11 is **flight replay** (`docs/replay.md`). Milestone 12 is the dashboard's header, status lines
  and block in (D-151, D-152, D-154), with the plan's block-in time until the predictions exist. Milestone 13 is the
  Pre-Flight panel (D-153). After that, a brainstorm for the in-flight panels, the predictions (TOD, block-in
  estimate), developer mode and the short final view. **The clearance card needs no probe flight:** SayIntentions'
  clearance is already in the logged `atc_exchange` lines ("Cleared to San Diego via the RADYR2 departure… Climb via
  the SID… Departure on 125.9. Squawk 2546."), so milestone 13 reads it from there.
- Why: Not every milestone needs the sim (Dave). With replay first, every dashboard change is checked against real
  flights, on the desktop or the laptop.
- Alternatives considered: the dashboard header first, checked only in the sim and with `--demo`'s made-up values.

## D-164 — Replay from the Flight Summary, at 1×, 2×, 3× or 10×
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave
- Area: app
- Decision: Every Flight Summary of a settled flight has a **Replay** button; the replay plays on Live at 1×, 2×, 3× or
  10×, starting at 1× and changeable at any time, and can be stopped whenever. It works as long as the flight's log
  files still exist. No jumping ahead in the app: speed covers long legs. It replays today's dashboard code, "so any
  changes underneath are replayed correctly".
- Why: A test and debugging tool for the dashboard; 10× turns a 10-hour leg into an hour.
- Alternatives considered: a command-line replay only; jumping ahead to a phase.

## D-165 — The replay feeds a second RecorderBridge from the log
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave + Claude
- Area: app
- Decision: A replay makes a new `RecorderBridge` (the same class, a second instance) and feeds it the flight's log
  lines through the same `apply*` methods `BridgeObserver` uses; Live binds to it while the replay runs. Live's planned
  flight comes from a separate read-only class built from the settled flight and its stored plan, presenting the
  properties Live reads. Nothing that writes or calls out (`FlightWatcher`, `PlannedFlightModel`, `ReviewWriter`, the
  gate assigner) is connected to the replay bridge; a test replays a whole flight and checks the database, the log and
  the network fakes are untouched. A real block out stops the replay, and Replay is disabled while a flight is open.
- Why: Everything from the bridge up is today's code, and the replay is safe by its wiring, not by checks that a later
  milestone could forget. One bridge class, because the bridge holds logic (stages, the Mach switch, the feed) that a
  second class would copy.
- Alternatives considered: re-running the recorder over the logged telemetry (the log lacks the frame samples, PMDG
  broadcast, GSX variables and facility data it reads, so touchdown, GSX and the landing would replay wrong); one
  bridge with a replay switch (every writer must check it); two bridge classes with one interface (Dave's idea, kept for
  the planned flight model only).

## D-166 — Live reads the time from its bridge, never the wall clock
- Date: 2026-11-24
- Status: Accepted
- Decided by: Claude
- Area: app
- Decision: The bridge has a `now`: the wall clock on the real bridge, the replay time on a replay bridge.
  `FlightPanel.qml`'s `Date.now()` and `EventFeedModel`'s wall-clock stamps move to it. From milestone 11 on, nothing
  on Live reads the wall clock directly.
- Why: At 10× the block clock, the feed and every later countdown (the clearance's 30 minutes, TOD) must run at the
  replay's speed.
- Alternatives considered: faking the system time for the app (affects everything, including the real recorder).

## D-167 — Replay's smaller calls
- Date: 2026-11-24
- Status: Accepted (in `docs/replay.md`, approved by Dave)
- Decided by: Claude
- Area: app
- Decision:
  - It starts at the lead-in `loadFlightEvents` reads (up to 30 minutes before block out), so Pre-Flight and GSX replay.
  - Pause / Resume next to the speeds; Stop goes back to the flight's summary on Flights.
  - Stretches where the logged sim was paused are skipped.
  - Times show as they happened that day; the block clock runs at the replay's speed.
  - At block in the summary shows **Replay again** and **Back to Flights**; At Gate Turnaround, Done for now and the
    debrief's Retry are hidden. The connection reads `Replay`.
  - The log is found from the flight id (the block-out time): the day files from the lead-in to block in, not only
    the last 7 or 30.
  - Fields an older log lacks are left at zero or empty.
  - `--replay <flight id> [--speed N] [--at <phase or mm:ss>] [--grab <file>]` for Claude Code; `--at` is the only
    place a replay jumps ahead.
  - The replay's tests use a full-format slice of the KLAS → KSAN leg (the debrief fixture strips position, fuel and
    the title).
  - A timer about every 50 ms, only while a replay runs.
- Why: Each settles something the brainstorm left open.
- Alternatives considered: starting at block out (Pre-Flight wouldn't replay); playing paused stretches in real time.

## D-168 — Dashboard code reads ATC through one plain form
- Date: 2026-11-24
- Status: Accepted
- Decided by: Dave + Claude
- Area: app
- Decision: `atc_exchange` keeps SayIntentions' entry verbatim; dashboard code reads it through one small function
  that gives time, station, frequency and ATC's words, never SayIntentions' fields directly.
- Why: Dave may connect another ATC system later (BeyondATC). ATC comes through `FlightWatcher` and the log, not the
  bridge, so a second source would only have to fill that form.
- Alternatives considered: reading SayIntentions' fields wherever they're needed.

## D-169 — The app icon
- Date: 2026-11-25
- Status: Accepted
- Decided by: Dave + Claude
- Area: app
- Decision: Dave's logo (a blue arc with a white pointer on a dark rounded square) is the app icon. It lives in
  `icons/` as `blocktime.ico` and `blocktime.svg`. `src/blocktime.rc` puts the .ico in `BlockTime.exe` as
  `IDI_ICON1`, which Qt's Windows plugin also uses for every window's title bar and taskbar icon, so no
  `setWindowIcon` call. The sidebar's brand mark draws the .svg (a resource of the QML module) in place of the
  "B" placeholder. The console tools (`blocktime-import`, `blocktime-debrief`) get no icon.
- Why: One file per format, from Dave; the .rc is the only way to give the .exe itself an icon, and Qt's default
  covers the windows. Qt's svg image plugin is already in the Qt kit and deployed by windeployqt.
- Alternatives considered: a PNG for the sidebar (blurry at some scales); `QGuiApplication::setWindowIcon`
  (redundant with IDI_ICON1).

## D-170 — No parking-spot check at block out
- Date: 2026-11-25
- Status: Accepted
- Decided by: Dave
- Area: recorder
- Decision: A block out stays accepted wherever the airplane is (aircraft, origin airport and fuel matching, D-088,
  D-110). BlockTime doesn't refuse one because no parking spot is within 40 m.
- Why: The only case it would catch is the sim loading the airplane somewhere other than the chosen gate, seen once
  (2026-11-25, the 777 on a KLAS runway, `docs/notes/msfs-observed-behaviour.md`). Dave: "too rare". The recovery is
  to close BlockTime, which discards the open flight and keeps the planned one. A check would also refuse gates the
  scenery's parking data lacks.
- Alternatives considered: refusing a block out with no parking spot within 40 m (a `block_out_refused` reason);
  refusing one after a position jump.

## D-171 — The review's instructions follow Dave's corrections
- Date: 2026-11-25
- Status: Accepted
- Decided by: Dave (the corrections), Claude (the wording)
- Area: app
- Decision: Claude's review instructions add: the checks are the verdict (a passed check is never a fault); ATC
  compliance appears only in the ATC part, never in the summary or "work on" (D-157); a level-off on the approach is
  often the FMC flying a published altitude constraint, not a fault; no remarks on radio frequencies or phraseology;
  touching down up to about 10 kt below VREF is normal flare bleed-off; advice only for missed checks or the landing's
  facts, with no technique preferences; consistent advice. The timeline gives the flaps in degrees, not the handle's
  detent.
- Why: Dave's notes on the first in-app review (`docs/notes/claude-debrief.md`): it blamed an FMC constraint, called
  "point seven" an incomplete frequency, gave contradicting flare advice, and read "flaps handle 8" as short of
  flaps 40. A replay with the new instructions answered all four.
- Alternatives considered: leaving ATC remarks in "work on"; judging touchdown speed against VREF.

## D-172 — Two APU checks
- Date: 2026-11-25
- Status: Accepted (the checks and the 2 points: Dave); the 737's 300 °C running threshold is Claude's, from the probe
  data. The gate check is refined by D-173 (switched on by engine shutdown, not running at the brake set)
- Decided by: Dave + Claude
- Area: app, recorder
- Decision: Two more checks, both from PMDG's data (the plain `APU SWITCH` never moves on either family):
  - **APU off by the transition altitude**: the selector off at the `transition_altitude` marker; a miss costs the
    usual 6 points;
  - **APU running at the gate**: running when the parking brake is set at block in; a miss costs **2 points**.
  The `cockpit` event gains `apu_switch` and `apu_running` (777: `APURunning`; 737: the selector on and EGT at 300 °C
  or more). Migration 8 adds the two check parts; flights measured before them show both as not checked.
- Why: Dave's SOP: "APU switched off by transition altitude and APU running before parking at the gate. That last one
  should only take off like 2 points if it's not met." On the probe flights a running 737 APU settled at 390–470 °C
  after a ~750 °C start peak, so 400 °C flickered; 300 °C is passed about 25 s into the start.
- Alternatives considered: the APU off before taxi or the takeoff roll; judging the 737 by the selector alone (it
  would count a start that never lit).

## D-173 — The APU at the gate: switched on by engine shutdown
- Date: 2026-11-25
- Status: Accepted
- Decided by: Dave + Claude
- Area: app
- Decision: The gate check (D-172) passes when the APU selector is on when the last engine stops at block in (the
  block_in line's `shutdown_time`), not when it is running at the brake set. Its part is `apu_on_at_shutdown`
  (migration 8 renamed before any database took it). Still 2 points.
- Why: Dave: "There are many times when the gate is close enough that I might not switch the APU on till I'm
  parked", hence the low points. He suggested checking the switch when the brake is set; Claude proposed the engine
  shutdown instead, since switching it on after setting the brake but before shutting down is still on time, and a
  just-started APU isn't running yet.
- Alternatives considered: running at the brake set (D-172 as first built); switched on at the brake set (Dave's
  suggestion).
