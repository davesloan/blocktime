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
- Status: Proposed
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
- Status: Proposed
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
- Status: Proposed
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
- Status: Proposed
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
- Status: Proposed
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
- Status: Proposed
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
- Status: Accepted
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
- Status: Accepted (state values unverified)
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
- Status: Accepted (fallbacks confirmed by Dave, 2026-11-22)
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
- Status: Proposed
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
- Status: Proposed
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
- Status: Proposed
- Decided by: Claude
- Area: build
- Decision: `catalog.yaml` and `airline.yaml` are read with yaml-cpp (0.9.0, MIT, from vcpkg) into plain structs in
  `blocktime-core`, validated there. Errors name the file and line ("airline.yaml line 12: ..."); one shared helper
  (`config_yaml`) does that for both readers. Money in the files is in whole dollars, multiples of $5.
- Why: Mature, simple API, and it reports line numbers, which matters for hand-edited files (Review Focus 1 of the
  milestone 4 plan).
- Alternatives considered: rapidyaml (faster, but speed doesn't matter for two small files); JSON config (no
  comments, harder to edit by hand).
