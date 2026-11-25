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

## 2026-11-24 — Milestone 10 built: procedure checks, landing stars and Claude's debrief (version 0.11.0)

**Done:**
- **Brainstorm with Dave** (this session): `docs/debrief.md`, D-156 to D-161. A spike wrote two versions of a debrief
  from the real KLAS → KSAN log. Dave chose approach A with Claude Opus 5.5: BlockTime's code measures and scores,
  Claude explains. ATC compliance is information only. The landing gets 0–5 stars. PMDG's broadcast is read on every
  flight. Dave's point: A also lets the scoring be tuned without paying for Claude calls.
- **Plan** `docs/plans/2026-11-24-milestone-10-debrief.md`, all 11 tasks, executed inline (D-033), TDD throughout.
  - **Recorder:**
    - a change-based `cockpit` event (landing lights, seat belts, gear, PMDG's VREF, landing flaps and autopilot), with
      PMDG's broadcast now subscribed in normal running and forwarded to `--probe`;
    - a cockpit snapshot at block out;
    - `touchdown` gains raw pitch, bank, heading and wind;
    - a `bounces` event 15 s after touchdown.
  - **Core:**
    - `flight_events` reads one flight back;
    - `debrief_measure` gives 17 measured parts or skip reasons, plus the landing;
    - `debrief_score` turns them into 12 checks, stars, score and averages, computed when shown (D-160);
    - `debrief_store` and migration 7 write them at settling, in the money's transaction;
    - `debrief_review` builds the Messages API request and parses the answer.
  - **App:**
    - `ClaudeReviewer` (HTTPS from Qt) and `ReviewWriter` (one automatic try after settling, then only Retry);
    - the summary's header shows the score and stars, with a Debrief / Flight toggle;
    - Flights gets an averages strip;
    - `claude_api_key` in settings.json is a secret.
  - **Tool:** `blocktime-debrief.exe` replays a saved log: checks, score, `--prompt`, `--claude`.
  - **Fixture:** `tests/data/flight-klas-ksan/` is the real leg in the 0.11.0 format (cabin messages removed).
- **On the real KLAS → KSAN leg the debrief scores 62, landing ★3.** Missed:
  - beacon on 2 min 43 s late;
  - seat belt sign off from taxi to 10,000 ft;
  - **17 s above 260 kt below 10,000 ft** in the descent (275 kt at 10,000 ft; the brainstorm's spike missed this);
  - STD 1 min 51 s late;
  - landing lights on 6 min after 10,000 ft.
- **Whole-milestone review** by a fresh reviewer: 0 Critical, 5 Important, 9 Minor. Fixed, each with a test that
  failed first:
  - a debrief that can't be measured or written no longer rolls back the money (and crashes at every start);
  - the 737's landing lights aren't judged without PMDG data;
  - the descent's lights window restarts at a go-around;
  - SayIntentions' nulls no longer throw while building the review;
  - pitch is stored raw and flipped in the scoring;
  - takeoff lights get 10 s of grace;
  - the cockpit snapshot at block out;
  - Retry reads 30 days of logs.
  - The deferred minors are listed under "Unverified or broken".

**Working / verified (without the sim):**
- Clean release and debug builds, no warnings; **635 tests pass**; qmllint reports nothing.
- The debug build's `--demo --grab` on a scratch copy of Dave's data shows the strip, the header and the Debrief view.
  It ran with `LOCALAPPDATA` pointed at the copy; the real `blocktime.db` is still schema 6.

**Unverified or broken:**
- **The real API works** (two calls with Dave's OK, `docs/notes/claude-debrief.md`): about 6,000 tokens in, 20 s,
  **about $0.05 a flight**, and Dave's Max plan includes $100 of API credit a month. The first review guessed the
  autopilot's height and put the 250 kt spell in the climb, so the timeline now gives heights for cockpit changes in
  the air and a line per spell above 260 kt; the second review got both right.
- **Sim checks** (form run `m10-debrief`):
  - the BBJ2's landing lights reach `landing_lights` and its runway turnoff lights don't (with PMDG's LED lights
    package, which Dave flies, there are no retractable landing lights);
  - the seat belt selector on both families;
  - the gear value while paused;
  - **the sign of `PLANE PITCH DEGREES` at touchdown** (`kSimPitchNoseUp` in `debrief_score.cpp` assumes nose up is
    negative);
  - a real bounce counted;
  - the 777's VREF and landing flaps in normal running;
  - Retry with the network off, then on.
- **Deferred minors:**
  - the log is parsed twice on the UI thread at block in;
  - the answer parser takes the first text block (a fallback response's layout is unconfirmed);
  - some skip reasons are labelled loosely;
  - the review's header lacks the planned cruise level;
  - `flight_landing`'s `off_*` columns have no both-or-neither CHECK.
- **For Dave to confirm:**
  - D-160: the tolerances, the star bands and the points;
  - D-161: the review's input, its JSON parts, one try then Retry;
  - D-162: the build's calls, including the Debrief / Flight toggle.

**Next step:**
- Dave flies the `m10-debrief` run (steps in the form and below). Then Claude reads the results and the log, tunes
  anything that's off, and the MVP is done. After that comes the Live brainstorm (`docs/roadmap.md`, "Live ideas").

## 2026-11-24 — Milestone 9 sim test: two BBJ2 legs, KSFO → KLAS → KSAN

**Done:**
- Dave flew both legs with the sim checks form (run `m9-gates`): 16 of 18 steps passed, 2 failed (s04, s16). Results
  read back from the form and checked against the event log. Findings are in `docs/notes/sayintentions-api.md` and
  `docs/notes/msfs-observed-behaviour.md`.

**Working / verified:**
- Status tiles (brake, engines), the Mach tile (on at M.70 at FL229, back to knots in the descent), parking names at
  block out and in ("Gate D6", "Gate E7", "Gate 5"), the summary's "Parked at", and the next load-in spot at KLAS
  and KSAN.
- Approach at flaps 1 on leg 2 (within 2 s). Both flap SimVars follow the PMDG 737 lever.
- `lined_up`: KLAS 08R at 4.8 nm (Dave's tap: within 5 s), KSAN 27 at 10.0 nm. Short final at 1,000 ft to touchdown
  both legs.
- The arrival gate flow end to end: SayIntentions' gate read in cruise, BlockTime's pick sent, read back.

**Unverified or broken:**
- **Gate assignment picks unusable spots (s16 failed).** "Closest to the landing runway" chose KLAS "Gate 239", a
  mid-field stand, and KSAN "Parking 70", which the scenery shows as a construction zone. Dave wants to talk about
  gate assignment before it changes.
- **A send that SayIntentions ignores still says "assigned".** At KLAS, `assignGate("239")` gave no error but
  SayIntentions kept E7A; Live said "Arrival gate: Gate 239" while ATC sent Dave to E7A.
- **Leg 1's Approach started 82 s before flaps 1**, 16 nm out, on the 3,000 ft AGL fallback over the ridge south-west
  of KLAS.
- s04: no `assigned_gate` in flightJSON before pushback (it appears once SayIntentions picks, in cruise). Not a bug.
- `getAirport` returns `[]`: the probe can go.

**Next step:**
- Done in the same session: D-155 (SayIntentions' gate stands unless a preferred or larger spot is needed; a send
  counts only when the read-back moves; the AGL fallback stays). 568 tests pass. In the next flight, check the
  arrival gate line: "Arrival gate: SayIntentions' …" with no preference set, and "Gate D14 (preferred)" at KPDX.
  Then milestone 10 in a new session (D-121).

## 2026-11-24 — Dashboard brainstorm (after the MVP), no code

**Done:**
- Brainstorm with Dave in the claude.ai chat (no Claude Code session) on the Live screen's post-MVP redesign, while
  milestone 9 waits for its sim test. Written up in `docs/dashboard.md` with two sketches (`docs/images/`), and
  decisions D-150 to D-154: the Flight Summary and the dashboard as names, Pre-Departure renamed Pre-Flight, the
  header (identity row, route arc, phase pill), three status lines, the Pre-Flight panel, and block in keeping the
  header.
- `roadmap.md` and `design.md` section 16 point to the new doc. Nothing in the code or the specs of built features
  changed.

**Working / verified:**
- Docs only.

**Unverified or broken:**
- Whether SayIntentions exposes the clearance, taxi route and frequencies (`docs/dashboard.md`, "Open before
  building"). Needs a probe flight before the clearance card is planned.
- D-151's parts marked Proposed (Claude's calls) wait for Dave's confirmation.

**Next step:**
- Unchanged: Dave's milestone 9 sim test (entry below). The departure, in-flight and arrival panels are for later
  brainstorms.

## 2026-11-24 — Milestone 9 built: gates (version 0.10.0)

**Done:**
- Plan `docs/plans/2026-11-24-milestone-9-gates.md`, all 8 tasks, executed inline (D-033). Dave's calls are D-145
  (accepted Claude's proposal), D-146/D-149 (Short final at 1,000 ft), D-147 and D-148 (approved), and two BBJ2 test
  legs.
  - **Recorder:**
    - the facility request reads each airport's parking spots, and `block_out`/`block_in` carry `parking`: the
      nearest spot within 40 m ("Gate D14");
    - telemetry gains `mach`, `flaps_te_pct` and `flaps_handle`;
    - Approach starts at the first flap extension below 10,000 ft, falling back to 3,000 ft AGL (D-142);
    - `lined_up` names the destination runway Dave is lined up with (D-143, D-145).
  - **The arrival gate (D-141, D-147):**
    - `pickGate` chooses preferred gates first, then the closest fitting spot of the right type to SayIntentions'
      landing runway;
    - FlightWatcher polls SayIntentions' local flightJSON from block out. Once SayIntentions' gate is at the
      destination, it sends `assignGate` (unless SayIntentions already chose that spot), reads it back with
      `getParking` and logs one `arrival_gate` per flight;
    - with `--probe` it logs `getAirport`'s answer once, keys removed.
  - **Database:** migration 6 adds `flight.origin_parking` and `arrival_parking`.
  - **Live:**
    - three columns (D-139);
    - the brake and each engine as green/grey tiles, and Mach from M.70 (D-140);
    - "Parked at Gate E7", the gate line, and "Final approach, runway 26L" turning into "Short final" at 1,000 ft;
    - the planned flight says where to load in, and the summary says where the flight parked.
  - **Config:** `wingspan_m` per type in catalog.yaml (required), and optional `preferred_gates` in airline.yaml.
- **After the build (Dave's asks):**
  - fixed the review's minor: a go-around now re-arms the 1,000 ft marker, so the next approach shows Short final
    again (tests `PhaseDetector.FinalApproachMarkerAgainAfterGoAround`, `FlightStages.SecondApproachAfterGoAround`);
  - every QML size now comes from Theme.qml (D-150), with no visible change; the Live, Fleet and Money grabs match;
  - Dave approved every call from the build (D-150).
- **Whole-milestone review** by a fresh reviewer: 0 Critical, 1 Important, 5 Minor.
  - Fixed, with a test that failed first: SayIntentions' gate was taken as its arrival pick as soon as it was set. At
    the gate it may be the departure gate, so it would have been answered at pushback. It now counts only within
    5 km of the destination.
- **Dave's config:**
  - `catalog.yaml` now has the wingspans; the old one is `catalog.before-m9.yaml`.
  - `blocktime.before-m9.db` is a schema 5 backup.
  - My `--demo` screenshot runs used the real `%LOCALAPPDATA%` files and migrated `blocktime.db` to schema 6 early.
    That only added the two empty columns; no other writes, nothing in `errors.log`. The backup had the columns
    dropped again, so it's a true schema 5 copy (4 flights, 44 ledger lines).

**Working / verified (without the sim):**
- Clean release and debug builds, no warnings. **565 tests pass.** qmllint reports nothing.
- `--demo --grab` shows the three columns, green engine tiles, the grey brake and "M.78" (at 150% scaling).

**Unverified or broken:**
- Everything in the sim: Dave's checks below.
- **Open questions the flight answers:**
  - what flightJSON's `assigned_gate` holds before departure;
  - whether `assignGate` takes MSFS's short names ("E7" where SayIntentions says "E7A"; "70" for a ramp) and
    replaces SayIntentions' gate on its status screen;
  - whether `TRAILING EDGE FLAPS LEFT PERCENT` (or `FLAPS HANDLE INDEX`) follows the PMDG 737's lever;
  - what `getAirport` returns.
- **Not covered by the BBJ2:** the 777's flaps (D-142). Its Approach uses the 3,000 ft AGL fallback until checked.
- **Deferred minors from the review:**
  - an `assignGate` answer arriving after an interrupt or a new block out is logged with the wrong or an empty
    `flight_id`;
  - `lined_up` has no height limit and fires once. Overflying the field along a runway axis in Descent could name
    the reciprocal runway and miss the real final. A late sidestep keeps the first name;
  - parking spots are placed assuming the airport record comes first (true on every probe flight).
- **Still unseen from milestone 8:** the banner fix (step 11), Ready with four items without GSX deboarding (step
  15), and Done for now (step 16).

**Dave's checks** are also loaded, as 19 steps with Pass/Fail, time buttons and notes, in the sim checks form
(https://claude.ai/artifact/1F8FD6v2mPrwNF2C8ai5vY, run `m9-gates`); Claude reads the results back from its database.
Two legs in N814SX (the BBJ2) in one sim session: **KSFO → KLAS as VIP airliner, then KLAS → KSAN as Charter.** N814SX is at KSFO with 6,434 lb on record, so load at least 6,134 lb.

1. **No sim.** Start `build\x64-release\BlockTime.exe --probe`. Expected:
   - v0.10.0 in the sidebar;
   - nothing new in `errors.log` (catalog.yaml now has `wingspan_m`);
   - Live in three columns (Flight progress, the panel, Events).
2. **New Flight:** N814SX, KSFO → KLAS, **VIP airliner**, then Generate and Import. Expected: "Load in at KSFO, San
   Francisco International Airport" with no gate (the last flight is from before 0.10.0).
3. **Load in at a KSFO gate** with at least 6,134 lb, and start SayIntentions with the flight. Expected:
   - Parking brake: green SET;
   - Engine 1 and Engine 2: grey OFF, each turning green ON as it starts.
4. **Before pushback**, open `http://localhost:43117/flightJSON` in a browser and note `assigned_gate` under
   `current_flight` (empty, a KSFO gate, or a KLAS gate). Don't paste the page anywhere: it has your key.
5. **Fly to KLAS.**
   - In the climb, around FL220: the airspeed tile turns into "MACH M.70".
   - In the descent: back to knots below M.66.
6. **In cruise, when SayIntentions' status window shows its arrival gate.** Expected within 30 s:
   - an "Arrival gate: …" line under Cruise, and "ARRIVAL GATE … (assigned)" in Events;
   - note the line, and whether SayIntentions' status screen changed to that gate.
   - If the line says "(it doesn't know Gate E7)", note SayIntentions' own name for the gate.
7. **Approach** starts when you set flaps 1 below 10,000 ft, not about 30 nm out. Note the time you set flaps 1.
8. **On final:** "Final approach, runway …" appears around 10 nm out once you're lined up. At 1,000 ft AGL it ends
   and "Short final" starts, ending at touchdown.
9. **Taxi to the gate ATC gives you and block in.** Expected:
   - Arrival: "Parked at Gate …";
   - the summary's top line ends "· Parked at Gate …".
   - Note which gate ATC gave and where you parked.
10. **GSX turnaround at KLAS**, then At Gate Turnaround.
11. **New Flight:** N814SX, KLAS → KSAN, **Charter**. Plan it at the gate without touching anything. Expected:
    - "Load in at KLAS, Harry Reid International Airport, Gate …" (the gate from step 9);
    - any "Not recording" banner clears within a second.
12. **Fly to KSAN.** In cruise, the arrival gate line should name a ramp ("Arrival gate: Parking …" or "… Parking
    …"). Note what SayIntentions' status screen shows.
13. **Lined up and Short final** as in step 8.
14. **Block in at the spot ATC gives.** Expected: "Parked at …".
15. **Don't use GSX deboarding.** Turn the beacon and strobes off. Expected: "Ready for shutdown or turnaround" with
    four ticks.
16. **Done for now**, then New Flight from KSAN to anywhere. Expected: "Load in at KSAN, …, Parking …". Then Cancel
    the flight.
17. Tell me when it's done. I'll read the log for the flap values, the `lined_up` distances and the `getAirport`
    answer.

**Next step:** Dave's sim test above, then fixes, then push `main` (D-121). Milestone 10 (procedure capture and
Claude's debrief) starts in a new session.

## 2026-11-24 — Milestone 8 sim test (KSLC → KLAS), banner fix

**Done:**
- **Dave's sim test** in N916SX with `--probe`: KSLC → KLAS flown and settled, then a GSX turnaround at KLAS. The second
  leg (KLAS → KSAN) was abandoned at the gate: GSX's pushback kept opening and closing the 777's cargo doors, so Dave
  quit to the menu. Dave saw on Live: the deboarding line and "Ready for shutdown or turnaround" with five items
  including "Deboarding complete ✓"; on the second leg Refuelling and Boarding under Pre-Departure with no Catering.
  He thinks the first leg showed "Pushback (GSX)" and the three KSLC services, but doesn't remember for sure.
- **Database:** flight 4 (KSLC → KLAS) has all ten phases, `pushback` 16:53:17 to `arrived` 18:05:02 (the block in),
  and boarding 16:31-16:50, refuelling 16:32-16:38, catering 16:33-16:36, deboarding 18:07-18:12. Schema version 5.
- **777 probe results** in `docs/notes/probe-flight.md`: every listed PMDG 777 value moves; the plain parking brake
  agrees with PMDG's; the FMC's distance to TOD and to destination behave.
- **Bug fixed: the "Not recording" banner went stale after a turnaround.** Live said "no flight is planned" although
  KLAS → KSAN was planned and imported. The match was rechecked only on a telemetry sample, and a parked airplane sends
  none. New `MatchReporter` (`src/match_reporter.h`) keeps the last sample, and the recorder loop rechecks it every
  pass while no flight is open, reporting only a change. Tests `MatchReporter.*` (3 failed first). Toggling the beacon
  had cleared it in the sim.
- **Ideas for milestone 9, from the flight:** D-140 (Live panel: drop Aircraft and Position, status tiles for the
  brake and each engine, Mach from M0.70 back below M0.66), D-141 (ask for the gate once SayIntentions has picked
  its own, using its landing runway), D-142 (Approach at the first flap extension), D-143 ("Final approach" when
  lined up with the runway). SayIntentions' local flightJSON has the arrival gate with its position, the STAR and the
  runway (`docs/notes/sayintentions-api.md`); there's no API access to TaxiTuner.

**Working / verified (without the sim):** clean release and debug builds, no warnings; **483 tests pass**; qmllint
reports nothing.

**Unverified or broken:**
- The KLAS → KSAN plan is gone from the database: Dave cancelled it after quitting to the menu (not a bug).
  N916SX is at KLAS with 19,920 lb.
- Not seen in the sim: Ready with four items without GSX deboarding, and "Done for now" (steps 13-14).
- The banner fix itself isn't checked in the sim: plan a flight at the gate without touching anything, and the banner
  should clear within a second.
- D-143's limits (10 nm, 10°, 0.5 nm) and dropping the 1,000 ft Live line are proposed.

**Next step:** `main` is pushed (D-121). Milestone 9 (gates) starts in a new session: its roadmap entry lists the work
and D-139..D-143; the gate fields and the open `assignGate` questions are in `docs/notes/sayintentions-api.md`. At
the end of any flight, check the three items above that the sim hasn't shown yet.

## 2026-11-24 — Milestone 8 built: GSX (version 0.9.0)

**Done:**
- Plan `docs/plans/2026-11-24-milestone-8-gsx.md`, all 7 tasks, executed inline (D-033). Dave's calls are D-134..D-137;
  Claude's GSX rules are D-138 (accepted by Dave).
  - **Recorder:** `GsxWatcher` reads GSX's five service states and the passenger totals on every flight (not only with
    `--probe`) and logs `gsx_service` on each state change. Telemetry gains `beacon` and `strobe`.
  - **`GsxTimeline`:** turns the log's GSX, block and sim session events into services with start and end, and gives
    each its leg. Live and settling both use it.
  - **Live (D-134..D-136):**
    - the Flight progress card spans the gate: the next leg's services before block out, the flight with its
      deboarding under the summary after block in;
    - "Pushback (GSX)";
    - "Ready for shutdown or turnaround" with a live checklist: engines, brake, deboarding, beacon, strobes;
    - no "Turnaround and Boarding" line.
  - **Database (D-137):** migration 5 adds `flight_phase` and `flight_service`. Settling stores the phases and the
    pre-departure services; deboarding is added when it ends (and at startup).
- **Whole-milestone review** by a fresh reviewer: 0 Critical, 1 Important, 9 Minor. Fixed, each with a test that
  failed first:
  - the `arrived` phase was never stored (it's logged just after `block_in`), so taxi-in had no end;
  - re-graded from Minor: a BlockTime restart between legs dropped the services already done from the stored flight;
  - re-graded from Minor: Ready showed at shutdown, before a GSX deboarding called after the beacon went off.
- **Backup:** `blocktime.before-m8.db` next to `blocktime.db`, made before 0.9.0 migrates it.

**Working / verified (without the sim):**
- Clean release and debug builds have no warnings. **479 tests pass.** qmllint reports nothing.
- Migration 5 ran on a scratch copy of Dave's database (3 flights) with nothing in `errors.log`.
- `--demo --grab` shows Refuelling and Boarding under Pre-Departure and "Pushback (GSX)".

**Unverified or broken:**
- Everything in the sim: Dave's checks below.
- **The checklist rows haven't been seen on screen** (the demo is in cruise); their data is tested.
- **Deferred minors from the review:**
  - after block in, if settling fails (a rates.yaml error), the card hides and the next leg's lines only show after
    the next block out;
  - after a BlockTime restart mid-turnaround, Live doesn't show the services already done (it doesn't read them back
    from the log), though they're still stored;
  - a late-settled flight's deboarding row is added at the next block in, not at once;
  - a timing row that fails its CHECK (a clock step) would block settling;
  - a new plan during the summary switching the card to the next leg has no test;
  - `INSERT OR IGNORE` also hides a CHECK failure on a deboarding row.
- **D-138 (the GSX rules):** accepted by Dave.

- **Bug found in the sim test (2026-11-24, at KLAS), to fix after the flights:** after At Gate Turnaround, Live
  showed "Not recording: no flight is planned" although KLAS → KSAN was planned (18:13:39Z) and imported. The
  recorder rechecks the match only when a telemetry sample arrives, and telemetry is requested on change, so a
  parked airplane sends none (the last came at 18:12:41Z). The banner kept the answer from before the plan; the
  summary had hidden it until the turnaround. Recording wasn't affected (block out checks the plan fresh).
  Toggling the beacon cleared it. Fix: recheck the match when the planned flight changes, with a test that fails
  first.

**Dave's checks.** Two legs in N916SX (777-300ER) in one sim session, with a GSX turnaround at KLAS: **KSLC → KLAS →
KSAN**. `--probe` also collects the 777's PMDG data, which the last flight missed (the broadcast is on now).
N916SX is at KSLC with 11,102 lb on record, so load at least 10,802 lb.

1. **No sim.** Start `build\x64-release\BlockTime.exe --probe`. Expected:
   - v0.9.0 in the sidebar and "Probe on" in the feed;
   - nothing new in `%LOCALAPPDATA%\BlockTime\errors.log` (the database moved to schema 5);
   - Fleet shows N916SX at KSLC with 11,102 lb.
2. **New Flight** for N916SX, KSLC → KLAS, then Generate in SimBrief and Import. Expected: Live shows the planned
   flight, and the Flight progress card with Pre-Departure current and no lines.
3. **Load in** at KSLC at the gate, with at least 10,802 lb, and start GSX. Expected: no GSX lines yet.
4. **GSX refuelling, catering and boarding.** Expected:
   - each line appears under Pre-Departure when its service starts working (not when you request it), shown as
     "05:34Z –" while it runs and "05:34Z – 05:39Z" when done;
   - they may overlap;
   - the feed shows "GSX refuelling performing", then "completed".
5. **Push with GSX.** Expected: Departure shows "Pushback (GSX)"; Pre-Departure turns done and keeps its three lines.
6. **Fly to KLAS.** The stages work as in 0.8.0.
7. **Block in at KLAS** (brake set, engines off). Expected: the summary, with the Flight progress card under it
   showing Arrival "Parked". If the beacon and strobes are already off, "Ready for shutdown or turnaround" appears
   with four ticks.
8. **GSX deboarding.** Expected:
   - while it runs, "Deboarding 21:05Z –" shows and any Ready line goes away;
   - when it completes, the line gets its end time and Ready comes back at that time with five items, including
     "Deboarding complete ✓";
   - switching the beacon on and off flips its tick.
9. ~~Still on the summary, request GSX refuelling for the next leg.~~ Dropped during the test (Dave, 2026-11-24):
   GSX refuels and boards from the SimBrief plan, so it can't refuel for the next leg until there's a new plan and
   GSX has reloaded it. That's what At Gate Turnaround leads to anyway.
10. **At Gate Turnaround** (after deboarding completes). Expected: the summary and the Flight progress card go away
    (no plan and no next-leg services yet), and New Flight opens for N916SX at KLAS.
11. **Plan KLAS → KSAN,** Generate in SimBrief, and Import. Expected: the card comes back with Pre-Departure current
    and no lines.
12. **Reload the plan in GSX, refuel and board with GSX, skip catering, and push with GSX.** Expected: Refuelling and
    Boarding appear under Pre-Departure as each starts; no Catering line; "Pushback (GSX)" under Departure.
13. **Fly to KSAN, and don't use GSX deboarding.** At the gate, set the brake and switch off the engines, beacon and
    strobes. Expected: Ready appears when the last of the four goes off, with four items, all ticked.
14. **Done for now.** Expected: the card goes away (no plan, no services yet).
15. **Tell Claude** the flights are done. Claude reads the database and the log:
    - each flight's phases, ending with `arrived` at its block in;
    - KSLC → KLAS: refuelling, catering, boarding and deboarding;
    - KLAS → KSAN: refuelling and boarding;
    - the 777's PMDG switch data from `--probe`.

**Next step:**
- Dave's checks above. Once they pass, `main` is pushed (D-121) and milestone 9 (gates) starts in a new session.

## 2026-11-24 — Milestone 7 sim checks passed; levels snap to the thousand

**Done:**
- **Dave flew the sim check:** N916SX (777-300ER) KSAN → KSLC with a go-around at KSLC, `--probe`, 1 h 59 min block.
  Dave: "Tests complete."
- **The FL369 finding is fixed (D-133):** a level within 90 ft of a thousand is that thousand, else the nearest
  100 ft. Dave chose 90 ft: ATC gave him 8,100 for the missed approach, which 150 ft would have read as 8,000. Three
  new tests. The KSFO replay's approach level now reads 3,000 and ends 8 s later (200 ft below 3,000, not below
  3,100).
- **Dave accepted the Proposed decisions** D-124 to D-130 and D-132's lease values.
- **The 777 data broadcast is on:** `[SDK]` `EnableDataBroadcast=1` added to `777_Options.ini` for the 77w, 77er and 77f
  (Dave's OK). Each old file is kept beside it as `777_Options.ini.before-sdk`.
- **The fuel check deferred from milestone 6 is dropped,** Dave's call: it's an edge case, tested if it ever happens
  in normal flying.
- The flight's findings are written up in `notes/msfs-observed-behaviour.md` and `notes/probe-flight.md`.

**Working / verified (in the sim, 2026-11-24, from the log):**
- Block out on the 777's brake release, matched by folder `pmdg 777-300er`. Pushback, then taxi out 3 min later.
- **Takeoff walked back** to 06:00:47 (40 kt at 06:00:54). Liftoff named KSAN 27, 1.6 m off the centreline.
- Climb at 1,050 ft. Above 10,000 and transition altitude at the right altitudes. No level-off was logged in the
  climb, pauses included.
- Cruise confirmed at 06:20:51 (shown as FL369, now FL370). The 200 nm marker, then top of descent at 07:03:58.
- **ATIS:** KSAN V at block out, W ten minutes later, KSLC B at top of descent. **ATC:** 100 exchanges, radio and
  intercom.
- Transition level, below 10,000 / approach, level-offs at 9,000 and 7,000, final approach at about 1,000 ft AGL.
- **Go-around** confirmed 10 s after it began (07:33:28), with the first approach ended there. Back to Approach at
  the missed approach level, 8,100 as ATC assigned. No touchdown from the go-around.
- **Landing:** −172 fpm, 1.14 g, runway 16R named 2.3 m off the centreline. Taxi in, then block in at the gate.
- The `altimeter` lines follow the baro: 1021.7 at KSAN, 1013.2 at 18,000 ft, 1028.1 at KSLC. `heading_true_deg` is
  in telemetry. `G FORCE` and `KOHLSMAN SETTING MB` were accepted, with no new SimConnect exceptions.
- Release build has no warnings; **416 tests pass** (one timing check skipped).

**Unverified or broken:**
- **No `probe_pmdg_777` data on this flight:** the 777's options files had no `[SDK]` section (now added). The 777's
  switches, doors, parking brake lever and FMC figures (D-120) wait for the next 777 flight with `--probe`. Nothing
  in milestones 7–8 needs them.
- **The 777 never sets `KOHLSMAN SETTING STD`;** STD shows as 1013.2. Whoever uses the `altimeter` lines (milestone
  10's analysis) treats 1013.2 as STD. The 777 has one baro setting: Dave set both knobs, only the captain's display
  shows it, and the first officer's knob changes the captain's number.
- **The 737 altimeter check is dropped** (Dave): the 737 also locks both baro knobs to one value, so there are no
  separate sides to tell apart.

**Next step:**
- **Milestone 7 is closed; `main` is pushed (D-121).** Milestone 8 (GSX) starts in a new Claude Code session from
  this entry.

## 2026-11-23 — Milestone 7 built: phases and the six stages (version 0.8.0)

**Done:**
- Plan `docs/plans/2026-11-23-milestone-7-phases-and-stages.md`, all 11 tasks, executed inline (D-033):
  - **Phases** (`PhaseDetector`, D-124): pushback to arrived, plus level-offs, step climbs and go-arounds. Tuned and
    tested on Dave's KBFI→KPDX and KPDX→KSFO flights, replayed from the event log. Holds count only active time: a
    freeze or a pause adds nothing (D-058, D-130).
  - **Markers:** 10,000 ft, the transition altitude and level, 200 nm out, final approach.
  - **Runways** (D-126): named by the recorder from the ground roll and the sim's runways, at liftoff and at taxi in.
  - **Touchdown** (D-125): from sim-frame samples below 300 ft. The rate is the last airborne frame's; the g is the
    peak in the second after contact.
  - **`FlightTracker`:** the recorder's per-flight glue, which turns all of the above into log events.
  - **Altimeter settings** (D-123): logged at block out and on every change.
  - **True heading** in telemetry, logged only (D-129).
  - **SayIntentions** (D-123, D-127): the ATC log every minute and at block in, and the origin's and destination's
    ATIS, through a queue the recorder thread writes. An exchange is logged again if its reply arrives late.
  - **Live:** a Flight progress card with the six stages, and the phase in the Flight card (D-128). `--demo` shows a
    made-up departure.
- **Whole-milestone review** by a fresh reviewer: 0 Critical, 1 Important, 7 Minor. Fixed, each with a test that failed
  first:
  - a long pause mid-climb could log a level-off, or an early cruise, that never happened: the sim answers the
    heartbeat while paused;
  - the destination ATIS was skipped on a flight that went from Cruise straight to Approach (re-graded from Minor).
- **Docs:** recorder.md (the phase rules as built, every new event, runway naming, the app's log lines), design.md
  §13.5, README, the roadmap. D-130 records the calls made while building.

- **After the build, at Dave's request:**
  - **Two starting fleet slots** (D-131): slot 3 comes at 10 h.
  - **Lease rates cut to about a sixth** (D-132): BBJ2 $150k, 777-300ER $385k. Dave's data was changed to match:
    - the new catalog was copied in;
    - N814SX's stored rate was set to $150k from its next month (past months stay as charged);
    - backups were made first (`blocktime.before-777.db`, `airline.before-777.yaml`, `catalog.before-777.yaml`).
  - **N916SX, a 777-300ER** (SimBrief 730097_1791150052995, fin 713), was added to `airline.yaml`: delivered, at KSAN,
    leased 2026-11-23, free-flight number 713, in a new "Widebody" fleet. It's leased at the next start. A rehearsal
    on a scratch copy showed it Parked at KSAN with 0 lb, "2 of 2 slots in use · slot 3 at 10 h", and cash
    $3,719,840 after its first $385,000 month.
  - The catalog's 777-300ER Standard airframe takes N916SX's SimBrief limits: 7,000 ft minimum runway, MLW 554,000 lb,
    max payload 152,320 lb. Its 42 + 328 seat split and its holds stay estimates, sized to SimBrief's 370 passengers
    and 82,020 lb of cargo.

**Working / verified (without the sim):**
- Clean release and debug builds have no warnings. **414 tests pass** (one timing check skipped). qmllint reports
  nothing.
- The app started on a scratch copy of the data folder, with nothing in `errors.log`. `--demo --grab` showed the
  Flight progress card as designed.

**Unverified or broken:**
- **Sim check findings (N916SX KSAN → KSLC, 2026-11-24, in progress):**
  - The Flight progress card appeared at block out and tracked every phase and marker correctly so far.
  - **To fix after the flight:** the cruise read "Cruising at FL369" while Dave held FL370 (36,994, then 37,001). The
    level is the mean of its 20 s window, which still held the end of the climb (logged `alt_ft` 36,900). Fix: snap a
    level to the nearest 1,000 ft within 150 ft of it, else round to 100 as now. It also turns the KSFO replay's
    3,100 ft approach level into 3,000.
- Everything that needs the sim: Dave's checks below.
- **SimVar names not seen working yet:** `G FORCE` (touchdown), `KOHLSMAN SETTING MB/STD` driven by PMDG, and `PLANE
  HEADING DEGREES TRUE`. Heading is the only addition to the main telemetry: if the sim refused it, telemetry would
  stop, which the first minute in the sim would show.
- **Proposed decisions for Dave:** D-124 (phase numbers), D-125 (touchdown), D-126 (runway naming in the recorder),
  D-127 (web results through the recorder thread), D-128 (stages), D-129's true-not-magnetic heading, and D-130 (the
  calls made while building).
- **Deferred minors from the review:**
  - Flight events are shown even when their log write fails (a failing disk only).
  - With no plan imported, "Level at FL240" and "Cruising at FL240" overlap on Live: the level is ended at the time
    cruise is confirmed, which the cruise line backdates.
  - An empty ATIS (no letter, no text) is logged once per airport.
  - `FlightTracker.InterruptForgetsTheFlight` doesn't interrupt with fast sampling on.
  - Touchdown time and the g window are ±0.1 s (frames are stamped when the loop reads them).
- **Not checked:** the Live page at the smallest window size.

**Dave's checks.** One recorded flight, Dave's choice: **N916SX, the 777-300ER, KSAN → KSLC** (about 550 nm, so the
200 nm marker fires). It also covers the 777 half of the next probe flight (end of `docs/notes/probe-flight.md`).
Left for a later BBJ2 flight:
- **the fuel check** deferred from milestone 6: N916SX has 0 lb on record, so it can't be refused for fuel;
- **the 737 side of the altimeter check** (D-123 asks for both families).

N916SX starts with 0 lb on record, so its first flight is charged for all the fuel loaded.

1. **No sim.** Start `build\x64-release\BlockTime.exe --probe`. Expected:
   - v0.8.0 in the sidebar and "Probe on" in the feed;
   - Fleet shows N916SX at KSAN, Parked, and "2 of 2 slots in use · slot 3 at 10 h";
   - Money shows "Lease N916SX" for $385,000.

   The old plan XLY8 KSFO–KLAS (from the discarded leg) is still waiting: **cancel it on Live** first. Then select
   N916SX on Fleet, New Flight to KSLC, generate on SimBrief and import the plan.

   While BlockTime is open, make the window as small as it goes and look at Live: nothing should be cut off.
2. **Load in.** Start SayIntentions, then MSFS, and load the PMDG 777-300ER at KSAN. Expected:
   - no "Not recording" banner (the type is matched by its folder, `PMDG 777-300ER`, D-112);
   - telemetry flowing (the live values move): this proves `PLANE HEADING DEGREES TRUE` was accepted;
   - no `simconnect_exception` naming `G FORCE`, `KOHLSMAN` or `runways request` in the log.
3. **At the gate.**
   - **Altimeter:** set a different baro on the captain's and first officer's side once, then press STD on each and
     back. Expected: `probe_altimeter` lines following each knob and STD button.
   - **Switches:** work the lights, seatbelt signs and APU. Expected: `probe_pmdg_777` values moving with each.
   - **GSX turnaround** (catering, refuelling, boarding). Expected: `DOOR_state` values in `probe_pmdg_777`.
4. **Push and taxi with GSX.** Expected:
   - block out after the push release, which also shows how `BRAKES_ParkingBrakeLeverOn` and the plain
     `BRAKE PARKING POSITION` behave on a 777 release;
   - in Flight progress, Departure shows "Pushback" from the brake release, then "Taxiing to runway" from when you
     start moving after the push;
   - the Flight card shows "Pushback", then "Taxi out";
   - feed lines PUSHBACK and TAXI OUT.
5. **Takeoff.** Expected:
   - "Takeoff roll" starting when you added power, not when you passed 40 kt;
   - "Takeoff" and, in Climb, "Departed runway 27" (or whichever runway you used);
   - the feed: TAKEOFF, LIFTOFF 27, CLIMB.
6. **Climb.** Expected:
   - "Above 10,000 ft" and "Passed transition altitude";
   - for any ATC level-off, "Level at …" with the phase reading "Climb — level at …";
   - **pause test:** pause for 2 minutes mid-climb (Escape or active pause), then carry on. Expected: no "Level at"
     line from the pause.
7. **Cruise.** Expected: "Cruising at FL3xx" when you level at the planned altitude, and "200 nm from destination".
   A step climb, if ATC gives one, shows "Step climb to …".
8. **Descent and the first approach.** Expected, in order:
   - "Top of descent";
   - "Passed transition level" (set the QNH there, so the `altimeter` lines show it);
   - "Below 10,000 ft", "Approach", "Final approach" (at 1,000 ft above the ground; it shows once per flight).
9. **Go-around (Dave wants the practice).** Go around from short final, ideally below 300 ft, fly the missed
   approach and level at its altitude. Expected:
   - about 10 s into the climb, a "Go-around" line, and the Flight card reads "Go-around";
   - the first "Approach" line ends where the go-around began;
   - about 20 s after you level off, a new "Approach" line and "Level at …" (the missed approach altitude), with the
     Flight card reading "Approach — level at …";
   - no touchdown and no "Landed" yet.
10. **Second approach and landing.** Expected:
    - "Landed on 34R, −NNN fpm" (or the runway you used) with a believable rate (compare with what you felt);
    - a TOUCHDOWN feed line with the rate and g (one only, from this landing);
    - "Taxiing to gate".
11. **Gate.** Note the gate SayIntentions gives you and exactly how it writes its name ("B14" or "B 14"). Park, set
   the brake and shut down. Expected:
   - the summary as in milestone 6;
   - Arrival shows "Parked" in Flight progress until the summary replaces it.

   Then **deboard with GSX** (probe: the deboarding states and counts, and `DOOR_state`).
12. **No sim.** Send the day's log files (`events-YYYY-MM-DD.jsonl`). I'll check:
    - `atis` lines for KSAN before takeoff and KSLC after top of descent;
    - `atc_exchange` lines matching SayIntentions' own history;
    - `altimeter` lines against the knobs;
    - `heading_true_deg` in telemetry;
    - the FMC figures (`FMC_DistanceToTOD_nm` counting down to 0, D-120);
    - the touchdown, and the 777's switches, doors and parking brake;
    - the go-around: `phase_changed` to `go_around` and back to `approach`, and the touchdown samples turning off when
      you climbed through 500 ft and back on below 300 ft.

**Next step:**
- Dave's checks above and his answers on D-124..D-130. Then close milestone 7: push `main` (D-121). Milestone 8 (GSX)
  starts in a new session.

## 2026-11-23 — Milestone 7 planning (docs and test fixtures only)

**Done:**
- **Plan:** `docs/plans/2026-11-23-milestone-7-phases-and-stages.md`, 11 tasks, version 0.8.0. It covers:
  - the phase rules;
  - markers, runways and touchdown;
  - `FlightTracker`, the recorder's per-flight glue;
  - the ATIS and ATC logging (D-123);
  - the six stages on Live.
- **Tuned on real data:**
  - The phase rules were prototyped on Dave's KBFI→KPDX and KPDX→KSFO flights from the event log.
  - Those flights are now replay fixtures (`tests/data/replay-*.csv`). The plan's expected times come from that run.
  - The runway method was checked on the same rolls: 14R, 10R, 10R and 28R, each 1–4 m off the sim's centreline. The
    runways are fixtures too (`tests/data/runways-*.json`).
- **SayIntentions docs checked:** `getCommsHistory` entries carry an integer `id`, so the dedupe is by `id` (D-127).
- **Claude's calls, all Proposed:**
  - D-124: the phase numbers, including a 20 s level hold instead of recorder.md's suggested 30–60 s;
  - D-125: touchdown from sim-frame samples near the ground;
  - D-126: the recorder names the runways;
  - D-127: web results reach the log through the recorder thread;
  - D-128: the six stages built from the log's events.

**Working / verified:**
- No code changed.

**Unverified or broken:**
- Whether PMDG drives `KOHLSMAN SETTING MB/STD` (D-123): milestone 7's sim checks.
- Whether the sim accepts `G FORCE` in the touchdown definition. If it doesn't, only that request fails.

**Next step:**
- Dave reviews the plan and D-124..D-128. Then execute it (executing-plans, D-033) from Task 1.

## 2026-11-23 — Probe gets the FMC and more 777 fields; milestone 6 closed

**Done:**
- **The probe's PMDG halves now log more fields** (`src/pmdg_probe_common.h`, both `pmdg_probe_*.cpp`):
  - both families: the aircraft model and the FMC's figures (D-120). Those are the distances to top of descent and to
    destination in whole nm, the cruise altitude, V-speeds and flaps, the transition altitude and level, and the
    flight number;
  - the 777 adds window heat, the APP light, `APURunning` and `WheelChocksSet`;
  - the 737's APU EGT is rounded to 10°, because whole degrees logged a line every second the APU ran.
- **Tests:** two for the shared FMC helper, using a stand-in struct (the PMDG headers aren't committed).
- **The gate name:** Dave confirmed SayIntentions called the KPDX gate "D14", maybe "D 14". The sim's parking
  `name` 15 (Gate D) + `number` 14 gives that name. This is recorded in `docs/notes/probe-flight.md` and design.md §17.
- **Next probe flight:** `docs/notes/probe-flight.md` now ends with what it should check. That's the 777, the FMC
  figures, GSX deboarding and the D14 space.
- **Milestone 6 is closed:** pushed to `origin/main`, as one Claude Code session per milestone (D-121).

**Working / verified:**
- Clean release and debug builds have no warnings; both PMDG halves compiled. **335 tests pass** in each:
  blocktime-tests 269 (one timing check skipped) and blocktime-qt-tests 66.

**Unverified or broken:**
- The new probe fields haven't been read in the sim.
- Carried over from the entries below:
  - the fuel check at release (milestone 6's step 5);
  - the review's deferred minors;
  - the 777 and GSX deboarding.

**Next step: milestone 7, in a new Claude Code session.**
- **What it is:** phases and the six stages (`docs/roadmap.md`): the phase rules in `docs/recorder.md` (with D-058),
  touchdown capture, and design.md §13.5's six stages and sub-events.
- **Where to start:** the start-of-session checklist in `CLAUDE.md`, then `superpowers:writing-plans`. Save the plan
  under `docs/plans/`.
- **Runways:** the sim's runways can name the departure and arrival runway from the ground roll. The method and its
  margins are in `docs/notes/probe-flight.md`, results item 4.
- **Also in milestone 7 (D-123):** start logging what milestone 10's analysis needs:
  - the SayIntentions ATC log;
  - the origin and destination ATIS, in the phases that use them;
  - the altimeter settings.

  The fetch timing and the dedupe method are settled in D-123 (Dave confirmed them).
- **The probe now logs the altimeter settings** (`probe_altimeter`) and PMDG's hPa selector. Any flight with
  `--probe` before milestone 7 is planned would show whether PMDG drives the sim's altimeter SimVars. Otherwise
  milestone 7's first sim check does.
- **Its sim checks** should take in the next probe flight (the list at the end of `probe-flight.md`) and the
  deferred fuel check.

## 2026-11-23 — Milestone 6 sim checks passed; the first probe flights read

**Done:**
- **Dave's sim checks.** Dave ran the milestone 6 checks. He flew the delivery leg to KPDX instead of KSAN, and the
  VIP leg KPDX → KSFO, to keep the flights short.
- I checked the results against the event log and the database.
- The probe data went into `docs/notes/probe-flight.md`, `msfs-observed-behaviour.md`, recorder.md (GSX states) and
  design.md §17.

**Working / verified (in the sim, 2026-11-23):**
- **Wrong aircraft.**
  - The Soxxly 737-800's release at KBFI was refused as `wrong_aircraft_type`, and no flight opened.
  - The BBJ2 then matched by folder `pmdg 737-800bbj2`, which confirms D-112 for the BBJ2.
- **Delivery KBFI → KPDX.**
  - `block_out` carried `planned_flight_id` 1, and the flight settled at block in.
  - The money:
    - fuel: 10,790 lb from 0 on record, $4,425;
    - landing fee $660, ATC $115, pilots $325, maintenance $195;
    - total $5,720.
- **VIP KPDX → KSFO.**
  - Fuel was charged only on what was added: 14,679 out − 7,501 on record = 7,178 lb, $2,945 (D-109).
  - VIP fares were $18,025 against $7,465 of costs.
- **Discarded third leg at KSFO.**
  - Leaving after BLOCK OUT logged `flight_interrupted`.
  - The plan (VIP, flight 8) is still waiting.
  - No money lines were added, and N814SX stayed at KSFO.
- **The database:**
  - flights 1 and 2, with the plan's runways (14R → 10R, 10R → 28R), which were the runways flown;
  - N814SX at KSFO with 6,434 lb, the shutdown fuel;
  - no longer awaiting delivery;
  - cash $4,104,840 = $4.1M − $5,720 + $10,560;
  - `user_version` 4, integrity ok, and `errors.log` empty.
- **The probe:**
  - The GSX states and passenger counts read.
  - Every PMDG 737 switch on the list reads.
  - The parking spots give gate names: Gate D 14 at both KPDX and KSFO.
  - The runway data names the takeoff and landing runway from the ground roll, without doubt.

**Unverified or broken:**
- **The fuel check at release** (step 5's "Not recording: load at least … lb") wasn't tried. Save it for a future
  flight: release with less than the fuel on record minus 300 lb, and expect the banner and a `NOT RECORDING
  fuel_below_record` line.
- **Still open from the probe:**
  - the 777, which hasn't been flown with the probe, including its parking brake;
  - GSX deboarding, which wasn't used;
  - the parking spots' `suffix` values;
  - whether SayIntentions called the KPDX and KSFO gates D14.
- The deferred minors from the milestone 6 review (in the entry below) are unchanged.

**Next step:**
- Plan milestone 7 (phases and the six stages) with what the probe found. The departure and arrival runways can come
  from the ground roll against the sim's runways.

## 2026-11-23 — Milestone 6 built: record the planned flight (version 0.7.0)

**Done:**
- Plan `docs/plans/2026-11-23-milestone-6-record-the-planned-flight.md`, all 10 tasks, executed inline (D-033):
  - **`rates.yaml`** (D-107): every fare and cost rate and the fuel price, read when New Flight opens and at settling.
  - **Schema 4:** `flight`, `flight_cabin`, `flight_plan`, `flight_money_line`, 17 money categories, and each type's
    `sim_folder` (D-112, D-115).
  - **The matching check:** the type by the loaded aircraft's folder, within 3 km of the origin, and fuel of at least
    the amount on record minus 300 lb. A refused release logs `block_out_refused`; a matched `block_out` carries its
    `planned_flight_id`.
  - **The Not recording banner** on Live, from the recorder's own check.
  - **Settling** reads the flight from the log, on every block event and at startup, in one transaction (D-109,
    D-113).
  - **Fleet:** slots from hours (D-083, D-114), and the Flights and Hours columns are back.
  - **Live:** after an interruption the plan stays, with a notice (D-108). After block in it shows the summary, with
    At Gate Turnaround and Done for now.
  - **Flights** lists the settled flights from the database, each with the same summary (D-111).
  - **`--probe`** for the test flights (D-116, `docs/notes/probe-flight.md`). The release build includes the PMDG
    halves, from the headers in Dave's PMDG install.
  - **`--grab <png> [--page n]`** renders a screen offscreen, so screens can be checked without taking over the
    desktop (`docs/notes/qt-setup.md`).
- **Whole-milestone review** by a fresh reviewer: 0 Critical, 2 Important, 9 Minor. Fixed, each with a test that
  failed first (D-117):
  - a release refused before the sim matched now keeps a "set the parking brake, then release it" banner (otherwise
    the flight could be lost without a word);
  - no settling at the built-in rates when `rates.yaml` can't be read;
  - Cancel flight is hidden, and refused, while recording;
  - no "no flight is planned" warning over a summary;
  - the startup summary no longer depends on the tracks arriving.
- **Docs:**
  - README: the screens, `rates.yaml`, the catalog upgrade, `--probe`;
  - design.md §6, §11, §13.4, §13.7, §13.8 and §14;
  - recorder.md (matching, events), database.md (schema 4 and its processes), the roadmap;
  - D-108..D-117.

**Working / verified (without the sim):**
- Clean release and debug builds have no warnings. **333 tests pass:** blocktime-tests 275 (one timing check
  skipped) and blocktime-qt-tests 58. qmllint reports nothing.
- **Scratch run.** I used a scratch copy of Dave's data folder, with a hand-written log of the KBFI–KSAN delivery
  flight:
  - migration 4 ran, and the flight settled at startup;
  - N814SX ended up at KSAN with 9,400 lb on board, no longer awaiting delivery, with 2.8 h;
  - cash: $4,087,945, which is $4.1M minus $12,055 of costs;
  - Live, Flights, Fleet and Money looked right, checked with `--grab`.
- The probe builds with and without the PMDG headers. Without the sim it logs nothing extra.

**Unverified or broken:**
- Everything that needs the sim: Dave's checks below.
- Dave confirmed D-117 (the four review fixes) on 2026-11-23. No decision is Proposed now.
- **Deferred minors from the review:**
  - At startup, the lease settle runs before the flight settle. A lease month that falls due between block in and a
    restart is checked against cash without that flight's money. This only matters with very little cash.
  - A diverted summary's title reads "KBFI → KSHT (planned KSAN)" rather than "Diverted to KSHT". The Flights row
    says "diverted to".
  - With one planned flight at a time, its row id is normally 1. "Reused id" is the usual case; the `planned_at`
    guard handles it. The docs call it rare.
  - A `block_in` held by the outbox (D-049) settles only at the next block event or start.
  - No test for a failure partway through the money lines. The rollback is correct by reading.
  - Finding the flown flight reads up to 7 logs on the GUI thread at block in and at startup.

**Dave's checks.** At Dave's request the planned delivery flight was removed from his database before testing, so the
checks start from New Flight. His database was backed up first as `blocktime.before-m6-tests.db`, next to it. The
steps are ordered to need one sim session.

1. **No sim. Copy the new catalog and the rates file, then start BlockTime with the probe:**
   `Copy-Item config\catalog.yaml, config\rates.yaml "$env:LOCALAPPDATA\BlockTime\"`, then
   `build\x64-release\BlockTime.exe --probe`. Expected:
   - v0.7.0 in the sidebar, and no notices;
   - Fleet: "0.0 h flown · 1 of 1 slots in use · slot 2 at 10 h", and N814SX with 0 flights and 0.0 hours;
   - Live: no planned flight;
   - the event feed says "Probe on".

   Then, on Fleet, select N814SX and click New Flight. Plan the delivery flight to KSAN, press Generate on SimBrief,
   and click Import Active SimBrief Plan. Expected: Live shows XLY8 KBFI → KSAN, "Waiting for sim".
2. **Sim, wrong aircraft.** Start MSFS and load the PMDG 737-800 (Soxxly) at KBFI. Expected: "Not recording: you're
   in a 737-800; this flight expects the 737-800 BBJ2 N814SX." Release the parking brake and taxi a few metres.
   Expected: a "NOT RECORDING wrong_aircraft_type" feed line, and no flight. Exit to the main menu.
3. **BBJ2 at the wrong airport.** Load the BBJ2 at KSEA. Expected: "Not recording: you're at KSEA; load in at KBFI."
   Main menu again.
4. **The delivery leg.** Load the BBJ2 at KBFI. Expected: the banner goes. This confirms D-112 for the BBJ2.
   - If the startup profile released the brake before the sim matched, the banner instead says to set the brake and
     release it. Do that.
   - Fly KBFI → KSAN with GSX. Note the gate SayIntentions' ATC gives you at KSAN.
   - At block in, expected:
     - the summary: fuel, landing fee, ATC, pilots and maintenance, with the total;
     - Money: cash down by that total;
     - Fleet: N814SX at KSAN with the fuel left at shutdown, Parked (no longer awaiting delivery), 1 flight and its
       hours.
5. **Turnaround and the fuel check.**
   - Click At Gate Turnaround. Expected: New Flight for N814SX at KSAN.
   - Plan a short VIP leg (KLAX is about 100 nm), generate and import the plan.
   - Release the brake with less fuel than N814SX had at block in, minus 300 lb. Expected: "Not recording: load at
     least … lb of fuel", and a "NOT RECORDING fuel_below_record" line.
   - Refuel, set the brake, release it and fly the leg. Expected: a VIP summary whose Fuel line charges only the fuel
     you added.
6. **A discarded flight.**
   - Plan one more leg and push back until the feed shows BLOCK OUT. Expected: Cancel flight is gone while recording.
   - Exit to the main menu. Expected: "… interrupted and discarded", the plan still waiting, the airplane where it
     was, and no new money lines.
7. **No sim.** Expected:
   - Flights lists exactly your two flights, each with its summary and map;
   - after closing and reopening, everything is the same.

   Then send the day's log file (`events-YYYY-MM-DD.jsonl` in the log folder), so the probe's GSX, PMDG and airport
   data go into `docs/notes/probe-flight.md`.

**Next step:**
- Dave's checks above. Then milestone 7 (phases and the six stages), planned with what the
  probe found.

---

## 2026-11-23 — Milestone 6 planning (docs only)

**Done:**
- Found how to tell the PMDG variants apart (D-088's open question) without a probe flight:
  - each variant has its own SimObjects folder in Dave's Community folder;
  - every Soxxly livery points at its variant's folder;
  - the logs show MSFS reporting that base folder when a livery loads.

  So the type check compares the folder (D-112).
- Settled with Dave:
  - an interrupted flight keeps its planned flight (D-108);
  - settled money uses the actual block time and the fuel added, with the rest as previewed (D-109);
  - 300 lb of fuel slack (D-110);
  - Flights lists the settled flights from the database (D-111).
- Claude's calls, all **Proposed**:
  - the folder rule (D-112);
  - settling from the log, with the nearest airport as the arrival (D-113);
  - hours counted from flights, with slots enforced (D-114);
  - the flight tables (D-115);
  - `--probe` (D-116);
  - D-110's mechanism (the shutdown fuel is the fuel on record; the check runs at the release).
- Plan: `docs/plans/2026-11-23-milestone-6-record-the-planned-flight.md` (10 tasks, version 0.7.0). It includes
  D-107's `rates.yaml`.

**Working / verified:**
- No code changed.

**Unverified or broken:**
- D-112 for the BBJ2 specifically: the first sim check of milestone 6 confirms it.
- The GSX L:var names in Task 9 come from community tools; the probe flight confirms them.

**Next step:**
- Dave reviews the plan and the Proposed decisions. Then execute it (executing-plans, D-033) from Task 1.

---

## 2026-11-23 — Milestone 5 built: New Flight and plan (version 0.6.0)

**Done:**
- Plan `docs/plans/2026-11-23-milestone-5-new-flight-and-plan.md`, all 8 tasks, executed inline (D-033):
  - **Airplane limits** (D-100): defaults on each catalog airframe, overridable per airplane, copied at lease time.
    N814SX gets them at the next start.
  - **Schema 3:** limits, the planned flight with its cabins, and the imported SimBrief plan. Each airplane also gets
    a default free-flight number (D-106, added mid-task at Dave's request).
  - **Loads** (design.md §9) and **pricing** (§10–12), each line rounded to $5.
  - **SimBrief:** the dispatch URL (cargo in thousands, from Dave's check) and the OFP parser, with the import check
    (D-105).
  - **SayIntentions:** getWX runways.
  - **The planned flight and the New Flight draft:** toggles never re-roll.
  - **Qt models** behind a `Planner` interface, plus `QtPlanner` (Qt Network).
  - **Screens:** a New Flight dialog from Fleet, and the before-departure panel on Live.
- Whole-milestone review by a fresh reviewer: 0 Critical, 2 Important, 8 Minor. Both Important findings are fixed,
  each with a test that failed first:
  - Plan flight no longer completes after Cancel, or for another airplane if New Flight is reopened while the runways
    are on their way.
  - A wrong SayIntentions key now says so instead of looking like "no ATIS".
- Docs:
  - README: a "Keys and IDs" section, New Flight, and the upgrade note for `catalog.yaml`. The old map-key command
    overwrote all of `settings.json`, so it's gone.
  - `database.md` (built), `CLAUDE.md`, `design.md` §13.1, `docs/notes/simbrief-api.md` (the cargo unit).
  - D-106.

**Working / verified (without the sim):**
- Clean release and debug builds have no warnings. **265 tests pass:**
  - blocktime-tests 225, of which one timing check is skipped;
  - blocktime-qt-tests 40.
- qmllint reports nothing.
- On a scratch copy of Dave's data folder:
  - migration 3 ran, and N814SX got 7,000 / 146,300 / 190 / 55 / 36,113;
  - the app started with a seeded planned flight on the Live page and no QML errors in `errors.log`.
- **Dave's checks, 2026-11-23:**
  - Checks 1–8 and 10 passed. They used KBLI (6,700 ft) for the runway test, since KMYF isn't in the imported data.
  - So did the real SayIntentions runways (14R → 27), the dispatch page, and a real import (FL410, 2h 52m, burn
    10,493 lb). That confirms the OFP's time formats.
  - Check 9 found the Live page's bottom cards clipped when a flight was planned. **Fixed** in c2bb1b5: the page
    scrolls when it doesn't fit (with a scroll bar only then), and the planned-flight block collapses to its title
    line. The debug build launched cleanly with it, but Dave hasn't looked at it yet.

**Unverified or broken:**
- **The layout fix (c2bb1b5)** needs Dave's eye:
  - with the block expanded and collapsed;
  - at the smallest window size and full screen.

  The release build was relinked after Dave closed BlockTime: 265 tests pass and there are no warnings. At 1100 px
  wide, the live tiles' labels elide ("INDICATED …"); that predates milestone 5.
- **Idea for later (Dave, not needed for the MVP):** Fleet rows don't look clickable until one is selected. Claude's
  recommendation:
  - rows highlight on hover;
  - each row ends in a "⋯" menu whose first item is New Flight (later: return the lease, set the default flight
    number);
  - the header's New Flight button stays for the selected row.

  Dave agreed that an always-enabled New Flight asking which airplane would only duplicate the row menu. The hover
  highlight was cheap, so it's built now (below).
- **Polish from Dave's re-check, built:**
  - Fleet rows light up under the mouse, with a hand cursor.
  - The collapse button is a circle with a lighter icon.
  - Copy route gives Navigraph's form, `KBFI/14R <route> KSAN/27`. SimBrief's route alone couldn't be pasted
    (`SimBrief.RouteWithRunways`, test first).
  - The release build has no warnings and 266 tests pass; qmllint is clean.
  - Dave checked all three on 2026-11-23, along with the scroll and collapse fix: all work.
- **Deferred minors from the review:**
  - If the flight is cancelled during an import, `busy` stays set. It's invisible, since the panel hides.
  - `PlannedFlightModel::flightChanged` is connected to nothing.
  - New Flight does nothing (no message) for a returned airplane.
  - A planned flight outlives its airplane being returned for an unpaid lease.
  - The dialog's modal overlay colour isn't a Theme token.
  - Two new QML files lack `pragma ComponentBehavior: Bound`, like `PrimaryButton`.
  - The Import button stays primary after the plan is in.
  - `redact()` doesn't catch a percent-encoded key.
- **Proposed, for Dave to confirm:** D-103, D-104, D-105, and D-106's storage (re-read from `airline.yaml` at every
  start).

**Dave's checks (no sim needed for any of them):**
1. Copy the new catalog over yours. It's unchanged from the repo's old one apart from the new `limits`:
   `Copy-Item config\catalog.yaml "$env:LOCALAPPDATA\BlockTime\catalog.yaml"`. In `airline.yaml`, add
   `flight_number: 8` (or your number) under N814SX. In `settings.json`, add `simbrief_user_id`,
   `sayintentions_api_key` and `sayintentions_developer_key` (README, "Keys and IDs").
2. Start `build\x64-release\BlockTime.exe`. Expected: Fleet and Money as before, no notices, and `errors.log` with
   only its "started" line.
3. On Fleet, select N814SX and click New Flight. Expected: only "Delivery flight: no passengers or cargo", and the
   flight number filled in.
4. Type KSAN. Expected:
   - costs only: landing fee $660, ATC about $920, pilots and maintenance;
   - a negative total with "+ fuel";
   - "N814SX ends up at KSAN".

   Then try an airport with a runway shorter than 7,000 ft. Expected: the runway message, and Plan flight disabled.
5. Check that nothing in the dialog is clipped. Then click Plan flight. Expected:
   - the browser opens SimBrief's dispatch page with KBFI, KSAN, XLY and your number, N814SX's airframe and 0
     passengers;
   - the ATIS runways, if SayIntentions has them (note whether origin and destination runway stuck);
   - the app switches to Live: "Waiting for plan", "Load in at KBFI, Boeing Field", and the runway line.
6. Click Import Active SimBrief Plan before pressing Generate. Expected: a message that the latest plan is older or
   for another route. Then press Generate on SimBrief and Import again. Expected:
   - "Imported the plan generated HH:MMZ" and the plan line (FL, block time, burn);
   - the route;
   - an estimate that now has a Fuel line;
   - "Waiting for sim".
7. Click Copy route and paste it into Navigraph Charts.
8. Close and reopen BlockTime. Expected: the same planned flight on Live, with its plan.
9. Shrink the window to its smallest. Expected: the Live panel and Fleet stay readable.
10. Click Cancel flight and confirm. Expected: the panel goes, and Fleet's New Flight is enabled again. Planning the
    delivery flight again for milestone 6 is fine either way.

**Next step:**
- **Milestone 5 is done:** all of Dave's checks pass, including the layout fix and the polish.
- Milestone 6 (record the planned flight) starts in a new session: write its plan (`docs/roadmap.md`, milestone 6).
  Include **D-107**: move the fare and cost rates and the fuel price into `rates.yaml`, read when New Flight opens
  (Dave chose milestone 6 for it).
- Dave confirmed D-103..D-107 on 2026-11-23, and chose to keep SayIntentions' runway guess for airports without an
  ATIS (D-105). No decision is Proposed now.
- The deferred review minors are listed above.

---

## 2026-11-23 — Milestone 5 planning (docs only)

**Done:**
- Researched SimBrief's dispatch page and OFP fetch, and SayIntentions' `getWX`. Results are in
  `docs/notes/simbrief-api.md` and `docs/notes/sayintentions-api.md`. Two surprises:
  - SimBrief asks apps not to poll the OFP endpoint.
  - The first part of a SimBrief airframe id isn't the pilot's id; `simbrief-airframes.md` is corrected.
- Settled with Dave:
  - the MVP is for him, not production, so config-file code gets the happy path only (D-099, also in `CLAUDE.md`);
  - airplane limits are airframe defaults, overridable per airplane (D-100);
  - an Import Active SimBrief Plan button replaces polling (D-101, supersedes D-070's polling);
  - the flight number is typed each time (D-102).
- Claude's calls, all **Proposed**:
  - the planned flight is current state, one at a time (D-103);
  - pricing and preview details, including delivery-flight costs and rates as code constants (D-104);
  - ATIS runway choice and the import check (D-105).
- Specs: milestone 5's schema (version 3) in `database.md`; design.md §3, §11 and §13.2; roadmap milestone 5.
- Plan: `docs/plans/2026-11-23-milestone-5-new-flight-and-plan.md` (8 tasks).

**Working / verified:**
- No code changed.

**Unverified or broken:**
- The unit of SimBrief's `cargo` parameter: Dave's 2-minute check before Task 4 (in the plan).
- Nothing is tested against Dave's SimBrief or SayIntentions accounts yet.

**Next step:**
- Dave reviews the plan and confirms D-103..D-105, then does the cargo-unit check. Then execute (executing-plans,
  D-033) from Task 1.

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
