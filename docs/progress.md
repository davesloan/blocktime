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

## 2026-11-23 — Milestone 4 verified by Dave

**Done:**
- Ran the real airport import into a scratch database first: 4,265 of 86,383 airports kept, 512 KB (results in
  `docs/notes/airport-data.md`).
- Dave confirmed D-094..D-098 and the seven older Proposed decisions (D-060, D-061, D-062, D-066, D-067, D-068,
  D-086). No decision is Proposed now.

**Working / verified:**
- Dave ran all five checks of the milestone 4 entry below, and they passed: the import into his own database (the
  Qt test's file set aside), his airline in the sidebar, N814SX awaiting delivery at KBFI on Fleet, $4.1M and two
  ledger lines on Money, no second charge after a reopen, a line-numbered notice for a broken `airline.yaml`, and the
  Money icon and minimum window size. Milestone 4's "done when" (roadmap) is met.

**Unverified or broken:**
- The deferred minors from milestone 4's review are still open (listed in the entry below). None blocks milestone 5.

**Next step:**
- Milestone 5 (New Flight and plan): write its plan, starting with where SimBrief's airframe weights are stored
  (`docs/notes/simbrief-airframes.md`) and belly cargo per flight (D-089).

## 2026-11-22 — PMDG variables from Dave's SPAD.neXt profiles (docs only)

**Done:**
- Extracted the PMDG data values and events from Dave's SPAD.neXt profiles for the 737 and the 777 into
  `docs/notes/pmdg-variables-from-spad.md`, with a target list for milestone 6's probe flight. The profiles
  themselves aren't committed.

**Working / verified:**
- No code changed. The meanings in the note are inferred from what each value lights on Dave's panels.

**Unverified or broken:**
- Every value still needs confirming in the probe flight. SPAD's addresses (`PMDGNG3:0000:018F`) have to be
  matched to field names in the PMDG SDK headers. The 777 profile reads almost nothing from the overhead.
- Dave's 777 profile still uses 737 commands for several Multi Panel buttons (ALT, APR, VNAV, FLCH, VS, CRS).

**Next step:**
- Unchanged: milestone 4's plan, starting with the schema in `docs/database.md`.

---

## 2026-11-22 — Milestone 4 built: the airline foundation (version 0.5.0)

**Done:**
- Plan `docs/plans/2026-11-22-milestone-4-airline-foundation.md`, all 9 tasks, executed inline (D-033):
  - **The airline database:** schema version 2 as in `docs/database.md`. The Qt test's file is set aside as
    `blocktime.test-schema.db`.
  - **Money and dates:** $5 rounding, the short and full forms, lease months.
  - **`blocktime-import.exe`:** OurAirports plus mwgg/Airports time zones, filtered (D-094).
  - **Config readers:** `catalog.yaml` and `airline.yaml` via yaml-cpp, with file-and-line errors (D-095).
  - **Leasing:** first months and monthly charges in date order, unpaid returns, drift warnings (D-096).
  - **`AirlineSession`**, the app's one entry point.
  - **Screens:** the Fleet table on real airplanes (D-097), a Money page, and the airline's name in the sidebar.
    The minute tick charges months that fall due while the app is open.
- Whole-milestone review by a fresh reviewer: 0 Critical, 4 Important, 8 Minor. All Important fixed, plus one Minor
  re-graded (D-098):
  - a busy timeout, and a checked BEGIN;
  - one bad airplane no longer stops the other leases;
  - a real airline is no longer mistaken for the Qt test's file;
  - the models reset only on change;
  - the "Can't afford" notice says how to retry.
- Docs: the README (set up your airline, airport data, screens, files), `CLAUDE.md`, `database.md` (built),
  `docs/notes/airport-data.md`, D-094..D-098.

**Working / verified (without the sim):**
- Release and debug builds have no warnings. **176 tests**: blocktime-tests 150 (one skipped timing check),
  blocktime-qt-tests 26. qmllint reports nothing.
- Dave's real `airline.yaml`, run against a scratch data folder with a one-airport fixture, leased N814SX (fin 1001,
  48 seats, holds 7,842 / 9,803 lb), posted $5,000,000 and -$900,000 (cash $4.1M), and wrote only the "started"
  line to errors.log. Private memory was 30 MB after startup.

**Unverified or broken:**
- **The real airport import ran on 2026-11-23** into a scratch database: 4,265 of 86,383 airports kept, 512 KB, about
  a second, and a second run updated in place. 33 large airports are left out, mostly ones missing from mwgg's zone
  file (YBSU, HSSK, and others; listed in `docs/notes/airport-data.md`). Dave's own `blocktime.db` hasn't been
  imported into yet: that's check 1.
- **The screens haven't been seen:** the layout, the Money icon (Segoe Fluent E8C7) and the minimum window size.
- **Deferred minors from the review:**
  - an airframe typo names catalog.yaml, not airline.yaml's line;
  - back-dated leases can leave cash negative;
  - leased airplanes aren't charged while a config file is invalid (caught up after);
  - no drift warning for starting_cash or founded_on;
  - ANSI paths in messages;
  - "UNPAVED" counts as paved;
  - a new airplane's id is re-read with a SELECT.
- D-094..D-098 were confirmed by Dave on 2026-11-23.

**Dave's checks (no sim needed; milestone 4 doesn't touch the recorder):**
1. Download the airport files and run the import (README, "Airport data"). Expected: "The Qt test's database was set aside as
   blocktime.test-schema.db.", then "Read 86383 airports, kept 4265" (the numbers drift a little as OurAirports
   updates daily). The old `blocktime.db` is now `blocktime.test-schema.db` next to the new one.
2. Start `build\x64-release\BlockTime.exe`. Expected:
   - **Sidebar:** "Soxxly Airlines" under BlockTime.
   - **Fleet:** N814SX, 737-800 BBJ2, 737-800 BBJ2 VIP, Executive, KBFI, 0 lb, a blue "Awaiting delivery" pill. No
     notices (the import already set the old file aside).
   - **Money:** $4.1M ($4,100,000), with "Lease N814SX, 22 Nov – 21 Dec -$900,000" above "Opening cash $5,000,000".
3. Close and reopen. Expected: the same cash and no second lease line.
4. Break `airline.yaml`'s indentation and reopen. Expected: the same Fleet and Money, plus a notice starting
   "airline.yaml line". Then fix it.
5. Check that the Money icon looks like money and that nothing clips at the smallest window size.

**Next step:**
- Dave's checks above (and the OK to download the airport files for the recorded counts). Then milestone 5 (New
  Flight and plan): its plan, starting with where SimBrief's airframe weights are stored
  (`docs/notes/simbrief-airframes.md`).

---

## 2026-11-22 — Milestone 4 planning: the schema draft (docs only)

**Done:**
- Settled with Dave:
  - times are ISO text in UTC (D-081);
  - the catalog is a template copied into the database at lease time, and the database is the source of truth
    (D-082);
  - lease affordability now, slots in milestone 6 (D-083);
  - airport time zones from an airport dataset joined on ICAO (D-084).
- Registration and SimBrief airframe are entered per airframe (D-087), so the matching check now compares aircraft
  type and origin, not registration (D-088, supersedes D-071).
- Belly cargo is chosen per free flight and per rotation; a configuration only says whether it can carry it, and
  the BBJ2 can't (D-089, built in milestone 5).
- Drafted `docs/database.md`: conventions, entity diagram, every milestone 4 table, the leasing and monthly-charge
  processes, and a sketch of milestones 5–6.
- Dave rejected the first draft's subtype tables as designing around the rules rather than their intent. The
  schema now has one `airframe` table, a `role` on the aircraft type, and lease charges pointing at their ledger
  lines (D-090, supersedes D-085). The rules' intent is now in `CLAUDE.md`.
- Leased planes are **airplanes**; an **airframe** is the catalog template they're made from (D-091). Specs and
  the schema are renamed; the code follows in milestone 4.
- Captured for after the MVP: managing airframes (the catalog) in the app, with the settings screen (D-092).
- A review pass simplified the schema further (D-093): no airline or runway tables, airports don't point at their
  import, types refresh from the catalog, and a lease's end is two columns on `airplane`. Dave approved the schema;
  the milestone 4 plan is next.
- Claude's calls, all Proposed: setting the test database aside with migrations from 2
  (D-086), the airport import filter and the identity exception (in D-084 and D-082).

**Working / verified:**
- No code changed.

**Unverified or broken:**
- Which SimVar tells the PMDG variants apart (`TITLE`, `ATC MODEL`, `ATC TYPE`…) for D-088: probe before
  milestone 6's matching check is built.
- The licence text of mwgg/Airports (MIT at the repo level) and OurAirports' terms. ourairports.com's certificate
  had expired when fetched.

- Config files written: `config/catalog.yaml`, `config/airline.example.yaml`, and Dave's own `airline.yaml` and
  `catalog.yaml` in `%LOCALAPPDATA%\BlockTime\` (Soxxly Airlines, XLY, SOCKS; BBJ2 N814SX, fin 1001, at KBFI).
  The BBJ2's SimBrief values are in `docs/notes/simbrief-airframes.md`.
- Wrote the milestone 4 plan: `docs/plans/2026-11-22-milestone-4-airline-foundation.md` (9 tasks).

**Next step:**
- Dave reviews the plan. Then execute it (executing-plans, D-033), starting with Task 1 (the database).

---

## 2026-11-22 — Design: the complete flight flow (docs only)

**Done:**
- Brainstormed the flight flow with Dave and wrote it into `design.md` as the new section 13 (old 13–16 are now
  14–17). It covers New Flight, planning, before departure, the matching check, stages and sub-events, the arrival
  gate, after the flight, and interrupted flights.
- Decisions D-069..D-076: milestones aim at the flight flow, SimBrief dispatch page with SayIntentions runways, the
  matching check, interrupted flights discarded (supersedes D-009), GSX events, arrival gates through SayIntentions,
  rotations unlocked by flying their legs, MVP setup from config files.
- `recorder.md`: crash handling now discards, new sections on the matching check, GSX and parking spots, and a note
  that milestones 4–7 are being replanned.

**Working / verified:**
- No code changed.

**Unverified or broken:**
- Probe flights still needed: GSX state values, PMDG switch readability, SayIntentions gate names.
- Dave reviewed and confirmed everything above, including D-071, D-074, D-075's details, the roadmap's calls,
  the database rules (D-078), the airport import (D-079) and $5 rounding (D-080).

- Agreed the thin-slice roadmap (D-077) and wrote `docs/roadmap.md` (milestones 4–10).
- Database design rules (D-078, `CLAUDE.md`), the one-time airport import (D-079), money lines rounded to the
  nearest $5 (D-080).

**Next step:**
- Dave requests a SayIntentions developer key and sends his SPAD.neXt profiles. Then milestone 4's plan
  (writing-plans), starting with the schema in `docs/database.md`.

---

## 2026-11-22 — One Qt app, merged to main (version 0.4.0)

**Done:**
- **Restructure (D-064..D-066):**
  - `recorder/` moved to the repo root with `git mv`, keeping history. The build is now
    `cmake --preset ...` from the root.
  - `webapp/` and the console recorder are removed (D-065).
  - The app is `BlockTime.exe`, version 0.4.0.
- **Missing tests (D-051):** `blocktime-qt-tests` covers the event feed, the bridge, and the Flights and Fleet
  models. Each test was checked by breaking the code on purpose.
- **Whole-branch code review**, by a fresh reviewer: no Critical findings, 2 Important, 13 Minor. Fixed:
  - **Important:** `LOST:` events and stderr errors now go to `errors.log` (D-067).
  - **Important:** the Flights list loads on a worker thread (D-068). A synthetic 30-day log took 4.8 s, which
    would have frozen the window.
  - **Minor:**
    - Qt messages have the CARTO key replaced by `<key>`.
    - A guard stops and joins the recorder even if `aboutToQuit` is skipped.
    - An odd file name in the log folder can't crash the app on a non-UTF-8 code page.
    - A defensive check in the history reader, a cross-thread test, and stale comments.
  - **Documented in D-061:** Fleet hours count only 30 days of log, and the test `blocktime.db` already on disk.
- **Docs:** the root README (build, test, run, map key, files), CLAUDE.md (one app, library split, Qt/QML
  conventions, repo map), `design.md`, `recorder.md` (milestone 8 dropped; airline milestones to plan), and the index.
- **Merged** `qt-ui-spike` into `main`. Not pushed.

**Working / verified (without the sim):**
- Release and debug build from the root with no warnings. **117/117 tests** (blocktime-tests 93, of which 1 is a
  skipped timing check; blocktime-qt-tests 24). qmllint reports nothing.
- `BlockTime.exe --demo` shows v0.4.0 and the real flights. `errors.log` gets its "started" line. On the synthetic
  30-day log, the page says "Loading flights..." and stays responsive, and 30 flights appear after about 5 s.

**Unverified or broken:**
- No sim flight with 0.4.0 yet. The recording code is unchanged, but the shutdown guard, the error log and the
  async list are new. On the next flight, expect the same results as Qt UI test 2's Part B, and `errors.log` holding
  only "started" lines.
- **Deferred minors from the review:**
  - #3: no hint when the key is wrong.
  - #9: `seedIfEmpty` ignores SQLite errors.
  - #12: tracks across the 180° meridian.
  - #15: the Fleet card title binding.
  - A held block event (D-049) refreshes the Flights list only at the next block event.
  - A long, high-latitude synthetic track (37°N → 67°N) didn't fit entirely in the map view.
- D-066..D-068 are Proposed.

**Next step:**
- Dave flies one leg with `build\x64-release\BlockTime.exe` (Part B checks), then milestone 4 (phases) with the
  sim-freeze rule (D-058). Later ideas: a map style picker, and an airframe page with all its legs on one map.

## 2026-11-22 — Decision: Qt wins (D-063)

**Done:**
- Dave chose one native Qt app (D-063). D-001, D-004 and D-007 are marked superseded.

**Next step:**
- Before the merge (D-051): settle the repo layout and the console recorder's future, add the missing tests, run a
  code review on the branch, then rework the docs (CLAUDE.md, `design.md`, `recorder.md`, milestones 4–8) and
  merge to `main`.

## 2026-11-22 — Qt UI test 2 flown: everything worked

**Done:**
- Dave ran Part A (the app in `--demo`) and Part B (one MSFS flight, KSAN → KSAN in the PMDG 737-800, N818SX)
  with the release `blocktime-app.exe`.

**Working / verified:**
- Part A (Dave): "everything works... surprisingly perfect". That covers the map's dragging, zooming and
  tiles, the flight list, and Fleet sorting.
- Part B (Dave): every step worked, including the new flight appearing at the top of Flights after block in
  without a restart, and Fleet's N818SX count going up.
- **Task Manager during the flight: memory peaked around 70 MB, CPU 0.3% at most and mostly 0%.** Both are inside
  the limits (100 MB, 1%).
- Log (`events-2026-11-23.jsonl`):
  - `block_out` 00:39:04Z (12,128 lb) and `block_in` 01:04:38Z (9,622 lb), 25:30 block, 2,506 lb used.
  - The Flights reader gives 63.0 nm and 8,020 ft highest. The 7 flights load in 50 ms.
  - The KSAN scenery freeze came back (50 s, about 6 min after block out) and was logged as
    `sim_unresponsive`/`sim_responsive`, with no effect on the flight.
  - Dave quit the sim (`sim_disconnected` `sim_quit`), then closed the app (`recorder_stopped`). No warnings.
- All of UI test 2's pass criteria are met (see `docs/qt-ui-test-context.md`).

**Ideas from Dave for later (not MVP):** a map style picker (e.g. a dark map with terrain), and clicking an airframe
in Fleet to see all its flights, every leg on one map. Details in the context doc.

**Next step:**
- Dave's Qt decision (D-050, `docs/qt-ui-test-context.md` "Deciding afterwards"). If Qt wins: tests and a code
  review on the branch, merge, and a decision superseding D-001 and D-004 (D-051).

## 2026-11-22 — Qt UI test 2 built (branch qt-ui-spike)

**Done:**
- Plan: `docs/plans/2026-11-22-qt-ui-test-2.md`, all 7 tasks. `main` was merged into the branch first (D-058).
- Side menu with three pages: **Live** (test 1's screen, unchanged), **Flights** and **Fleet** (D-062).
- **Flights:** the flights rebuilt from the event log (D-060), newest first, with block times, fuel used, distance
  and highest altitude, and the flown track on a dark **CARTO Dark Matter** map (D-059). The key is in
  `%LOCALAPPDATA%\BlockTime\settings.json`. The list reloads after every block event.
- **Fleet:** a sortable table from SQLite (`%LOCALAPPDATA%\BlockTime\blocktime.db`, test schema, 8 seeded airframes)
  with each tail's flights and block hours from the log (D-061). sqlite3 3.54.1 comes from vcpkg.
- Docs: D-059..D-062, the map probe results in `docs/notes/qt-setup.md`, the README's Qt section, and the context
  doc.

**Working / verified (without the sim):**
- Release and debug build with no warnings. **83/83 tests** (31 new: flight history, settings and tile cache,
  fleet store). qmllint reports nothing. The console recorder smoke run is unchanged.
- Dave's real log loads in 35–41 ms (6 flights, 2,220 track points). Every flight matches the log.
- Map: Dave's key shows the dark map with CARTO's credit line (debug and release). With no key, the track is drawn
  on the plain background with a note. A wrong key shows CARTO's "API KEY REQUIRED" tiles and the app keeps
  working. Ramp-only flights show at airport scale.
- Fleet: 8 rows; N818SX 2 flights / 0.8 h; every column sorts both ways; an unusable data folder shows SQLite's
  message instead of the table.
- **Release app in `--demo`** (10 s each, no sim). Task Manager's Memory column (private working set): **31 MB**
  on Live, **44–46 MB** after five flights on the map and the Fleet table. Working set 95 → 110 MB and private bytes
  100 → 117 MB, which also count shared Qt DLLs and committed memory. CPU 0–0.02% on every page.

**Unverified or broken:**
- Not seen in the sim: the list updating after a block in while the app is open, and memory/CPU during a flight
  with the map opened once.
- Dragging and zooming the map: not tested by script.
- Decisions D-060..D-062, and D-059's details, are Proposed (D-059's provider is Dave's choice).

**Sim checks for Dave** (one MSFS load; release `blocktime-app.exe`, not the console recorder):
1. Before loading the flight, open **Flights**. Expect today's flights on the dark map; select a few. Drag and
   scroll-zoom the map.
2. Open **Fleet**. Expect 8 rows. Note N818SX's flights and hours. Click a header twice to sort both ways.
3. Back on **Live**, fly a leg as usual. Now and then check Task Manager's Memory for `blocktime-app.exe`. Expect
   it under 100 MB (about 45 MB) and CPU under 1%.
4. After block in, open **Flights** without restarting. Expect the new flight at the top, selected, with its track
   matching the route flown.
5. Open **Fleet**. Expect N818SX's flights up by one and its hours up by the block time.
6. Your verdict on the map and the two data screens, and the Qt decision (`docs/qt-ui-test-context.md`, Deciding
   afterwards).

**Next step:**
- Dave's sim checks above, then the Qt decision. If Qt wins: tests and a code review on the branch, merge, and a
  decision superseding D-001 and D-004 (D-051).

## 2026-11-22 — Qt UI test 1 flown: passed

**Done:**
- Dave flew KSAN → KSAN in the PMDG 737-800 (N818SX) with the release `blocktime-app.exe` in place of the console
  recorder.

**Working / verified (in the sim):**
- Log (`events-2026-11-22.jsonl`, 21:02–21:55Z): `recorder_started`, `sim_connected`, `block_out` 21:28:23Z
  (15,316 lb), `block_in` 21:55:15Z (12,141 lb, 26 min 52 s), then `sim_disconnected` (`recorder_exit`) and
  `recorder_stopped` when the window closed. No warnings, no lost events.
- Dave's Task Manager readings during the flight: **CPU never above 0.2%, memory never above 30 MB.** Both are well
  inside the limits (1%, 100 MB). Task Manager's Memory column is the private working set, so it reads lower than
  the 87–92 MB working set / private bytes measured at idle without the sim; the two aren't the same number.
- Dave on the look: "beautiful and exactly what I was looking for."
- All four test 1 pass criteria are met (see `docs/qt-ui-test-context.md`).

**Unverified or broken:**
- Nothing new. The stderr-only errors from `EventLog` / `SimConnection` are still not shown in the app (accepted).

**Next step:**
- UI test 2 on `qt-ui-spike`: post-flight route map, fleet list from SQLite, side menu (D-051). Write its plan
  first. The Qt decision comes after test 2.

## 2026-11-22 — Qt UI test 1 built (branch qt-ui-spike)

**Done:**
- Plan: `docs/plans/2026-11-22-qt-ui-test-1.md`. All work is on the branch **`qt-ui-spike`** (Dave's name; D-051 said
  `spike/qt-ui`), built inside the recorder project so it can merge as it is if Qt wins (D-052).
- The recorder loop moved out of `main.cpp` into `Recorder` (library `blocktime-sim`, D-053). The console recorder
  is a thin front end and behaves as before.
- New `blocktime-app.exe` (Qt 6.12, Qt Quick, Basic style): it runs the recorder on its own thread and has a dark
  theme with four building blocks. The screen shows a header with the connection pill and Open log folder,
  aircraft, eight live tiles, a flight card with a running block clock, and an event feed (D-054).
- QML hot reload in debug builds (D-055), windeployqt after each build (D-056), and a `--demo` mode (D-057).
- Docs: D-052..D-057, `docs/notes/qt-setup.md`, the README's Qt section, and the status and findings in
  `docs/qt-ui-test-context.md`.

**Working / verified (without the sim):**
- Release and debug build with no warnings. 52/52 tests pass in both. qmllint reports nothing.
- Console recorder smoke run (lone `block_out`): same console lines and log lines as 0.3.0. A second copy exits
  with code 3.
- App, without the sim:
  - Closing the window writes `recorder_stopped` and the process is gone in about 0.2 s.
  - A left-open flight is closed as `recorder_crash`.
  - With the console recorder running, the app shows "Recorder already running" and writes nothing.
- Hot reload: a Theme colour change shows in about 1.5 s with no rebuild. A QML typo shows an error window and the
  app keeps running. The fix brings the window back, and closing still writes `recorder_stopped`.
- Release app at idle: about **87–92 MB** (working set and private bytes) and **0% CPU** over 10 s. That's close to
  the 100 MB limit before any sim connection.
- QML back-and-forth for the first full screen: 4 fixes, each found and fixed in one pass (listed in the context
  doc).

**Unverified or broken:**
- Nothing has been seen with the sim yet. The live values, the block events in the app and the in-flight
  memory/CPU are all unverified.
- The windowed app doesn't show the few errors `EventLog` / `SimConnection` print straight to stderr (accepted for
  now, see `docs/notes/qt-setup.md`).
- Decisions: Dave confirmed D-053..D-057 (Accepted).

**Dave's sim test (Qt UI test 1):** one MSFS load, PMDG 737 at a KOAK gate. Build the branch first
(`git switch qt-ui-spike`, then the usual release configure and build). Use
`recorder\build\x64-release\blocktime-app.exe`, and **don't** start the console recorder.
1. Start the app before MSFS. Expect the pill to go "Waiting for sim" → "Sim starting" → "Connected".
2. In the cockpit at the gate, expect the tiles to fill in: brake SET (amber), GROUND, fuel matching the FMC,
   engines 0 / 2, and the aircraft title and tail in the top card.
3. Push and taxi ≥ 10 m. Expect a BLOCK OUT line in the event feed, and the flight card to show the block out time
   with the clock running.
4. Fly the leg as normal. Now and then, check Task Manager → Details for `blocktime-app.exe`: Memory (expect under
   100 MB) and CPU (expect under 1%). Note the highest values you see.
5. Park and set the brake with the engines off. Expect BLOCK IN in the feed, and the flight card to show "Last
   flight: Block in H:MM · <fuel out> → <fuel in> lb".
6. Close the app window. Expect the newest lines in `events-*.jsonl` to be `sim_disconnected` (`recorder_exit`) and
   `recorder_stopped`.
7. Your verdict: does it look like FlyHub / Sim Toolkit Pro rather than an "Intro to Programming" app? What would
   you change first?

**Next step:**
- Dave reacts to the screenshots and flies the test above. Then UI test 2 (route map, fleet list from SQLite, side
  menu). If memory in flight goes over 100 MB, look at where Qt's baseline goes (the renderer, loaded QML modules)
  before test 2.

---

## 2026-11-22 — Recorder 0.3.0 sim test at KOAK

PMDG 737-800 at a KOAK gate, one MSFS load (Dave reloaded the flight once at the start). Log:
`events-2026-11-22.jsonl`, 19:15–19:51 UTC.

**Results (all pass):**
1. Push and taxi: `block_out` at the 19:35:25 release, `moved_m` 11.1, fuel 15,517 lb.
2. Escape menu (3 s) and active pause (5 s): no `sim_stopped`, no `flight_interrupted`.
3. Exit to the main menu: `sim_stopped` and `flight_interrupted` (`sim_stopped`) for that flight, about 1.3 s
   after the `full` pause flag.
4. Second flight: `block_out` at the 19:48:04 release, `moved_m` 10.4.
5. Second recorder: nothing in the log; the first kept recording.
6. Recorder killed and restarted: `recorder_started`, then `flight_interrupted` with flight
   `2026-11-22T19:48:04.035Z` and reason `recorder_crash`.
   - The brake had been set at 19:48:34, before the kill. So after the restart, the release at 19:50:16 and 11 m
     of taxi opened a new flight. This is the accepted D-048 gap; its wording now also covers a brake that is
     already set at the restart.
7. Exit to the main menu: `sim_stopped`, and the gap flight was interrupted (`sim_stopped`). That shows the gap
   flight is cleaned up the normal way. MSFS wasn't quit (the recorder was stopped with Ctrl+C), so `sim_quit`
   wasn't seen this time. That code didn't change since the 0.1.1 test, where it was seen.

**Noted for the first time:** the `Sim` state flaps on the way to the main menu (stopped, running, stopped within
about 3 s). Only the first stop interrupts. Details are in `docs/notes/msfs-observed-behaviour.md`.

**Decisions:** Dave confirmed D-047, D-048 and D-049 (Accepted) after this test.

**Next step:** milestone 4 (phases).

## 2026-11-22 — Planning: Qt UI test builds (docs only)

**Done:**
- Brainstormed the app stack with Dave. Decided to run two throwaway Qt 6 + QML test builds between milestones 3
  and 4 to decide whether BlockTime becomes one native app (D-050), on the branch `spike/qt-ui` (D-051, amends
  D-038).
- Wrote `docs/qt-ui-test-context.md`: how we got here, what Dave wants, what each build answers and how to decide.
  It also notes side topics from the same chat (recorder→app link ideas, GSX, Navigraph dropped).
- Added the test builds to the milestone list in `recorder.md` and the docs index.

**Working / verified:**
- No code changed.

**Unverified or broken:**
- Nothing new. Milestone 3's in-sim checks below are still in progress.

**Next step:**
- Dave finishes milestone 3's in-sim checks. Then Dave installs Qt 6 (official online installer, MSVC 64-bit kit)
  and we plan UI test 1 on `spike/qt-ui`.

---

## 2026-11-22 — Recorder milestone 3: flights always end (0.3.0)

**Done:**
- Plan: `docs/plans/2026-11-22-recorder-milestone-3.md`. 52 tests (19 new).
- **Main menu (D-047):** when the sim's `Sim` state changes to stopped with a flight open, `flight_interrupted`
  is logged with reason `sim_stopped`. This covers the main menu, restarting a flight and Escape → Quit.
- **Killed recorder (D-048):** at the next start, a flight left open in the log is closed with reason
  `recorder_crash`, and the console says so.
- **Failed writes (D-049):** a block event whose write fails is held, shown on the console, retried, and printed
  as `LOST:` at exit if it still couldn't be written.
- Code review: no Critical findings and one Important one. A sim kill logs `sim_stopped` about 5 s before the
  process is gone, so a killed MSFS now reports `sim_stopped` rather than `sim_process_gone`. The behaviour was
  kept and the docs corrected (D-047). The Minor doc slips were fixed too.

**Working / verified (without the sim):**
- Release and debug builds have no warnings, and 52/52 tests pass in both.
- Smoke run with a log folder holding a lone `block_out`: the first start logged `flight_interrupted`
  (`recorder_crash`) and printed the console line. The second start logged nothing new.

**Changed since 0.2.0:**
- Force-terminating MSFS during a flight now gives `flight_interrupted` with `sim_stopped`, not
  `sim_process_gone`. The 0.2.0 test's step 8 would read differently today.

**Decisions for Dave to confirm:** D-047, D-048 and D-049 are Proposed.

**Deferred review items:**
- A file in the log folder with a name outside the Windows code page could make the startup scan throw, so the
  recorder would fail at every start until the file is removed.
- Mixing an elevated ("Run as administrator") recorder with a normal one gets past the single-instance check. The
  second copy could then close the first one's flight as `recorder_crash`. This is pre-existing D-037 code.
- No test pins that a sim stop with no flight open also resets the brake baseline.

**Unverified (Dave to test in the sim with 0.3.0):** one MSFS load, two flights from the main menu. Use the PMDG 737
at a KOAK GSX gate. Start the recorder first.

*Flight 1: the main menu ends it.*
1. Push and taxi ≥ 10 m. Expect `block_out`.
2. Open and close the Escape menu, then turn active pause on and off. Expect no `sim_stopped` and no
   `flight_interrupted`.
3. Escape → exit to the main menu. Expect `sim_stopped`, then `flight_interrupted` with reason `sim_stopped`,
   within a few seconds.

*Flight 2: a killed recorder. Load the 737 at a KOAK gate again from the main menu.*

4. Push and taxi ≥ 10 m. Expect `block_out`.
5. Start a second recorder. Expect it to say it's already running and close. The first keeps recording, and there's
   no `flight_interrupted`.
6. In PowerShell, run `taskkill /F /IM blocktime-recorder.exe`, then start the recorder again. Expect:
   - the console line `Previous flight <id> was left open ...`
   - in the log, `recorder_started`, then `flight_interrupted` with that `flight_id` and reason `recorder_crash`
   - no new `block_out` while you keep taxiing. Don't stop and set the brake: that opens a new flight (the
     accepted D-048 gap).
7. Last: exit to the main menu, then quit MSFS. Expect `sim_stopped` with no `flight_interrupted` (no flight is
   open), then `sim_disconnected` (`sim_quit`).

**Next step:**
- Dave runs the checklist and confirms D-047 to D-049. Then milestone 4 (phases).

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
