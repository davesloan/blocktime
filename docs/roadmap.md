# Roadmap to the MVP

Agreed with Dave and approved on 2026-11-22 (D-069, D-077). The MVP is the complete flight flow in `design.md` section 13,
working end to end with real money. This file lists the milestones from here to the MVP and what comes after.
Each milestone still gets its own plan in `docs/plans/` (writing-plans) before work starts.

## Intent

- **Why:** the flight flow is the core of BlockTime: "If we get that right the rest of it falls into place." (Dave)
- **How:** a thin slice first. Milestones 4–6 make the whole loop work in a plain form (set up a flight, plan it,
  fly it, see the money, turn around). Milestones 7–10 deepen it. "Like in real life it's better to have working
  software fast than complete software slow." (Dave)
- **Why this order also helps:** Dave keeps brainstorming while it's built. New ideas and corrections go into the
  deepening milestones instead of reopening finished work.

### Goals

- Dave flies real airline legs, with real money, from milestone 6 on.
- Every later milestone makes a flow he already uses richer.

### Non-goals (until after the MVP)

- Rotations, background flying and catch-up of background legs (section 7–8). From milestone 6 on, every flight
  stores what leg templates will need (D-075), so nothing has to be re-flown later.
- Paying for instant delivery instead of flying the delivery leg.
- A settings screen, including managing airframes in the app (config files until then, D-076, D-092).

### Anti-goals (would make it a failure even if it works)

- Money that doesn't add up. "The money has to work in the MVP." (Dave)
- Punishing Dave: no penalties for interrupted, diverted or cancelled flights (design rule 2, D-072).
- Weight next to the sim: the app stays within the Qt test's limits (under 100 MB, under 1% CPU in flight).

### Constraints

- Everything in `CLAUDE.md` and `decisions.md` (one Qt app, log first, recorder reports facts, PMDG 737/777 on
  MSFS 2020 only, work straight to `main`).
- Each milestone is verifiable in the sim by Dave; `progress.md` lists exactly what to check.

## Approaches considered

| Approach | Trade-off | |
|---|---|---|
| **A. Thin slice first, then deepen** | Plain version of the whole loop in three milestones; later milestones add depth to a flow already in use | **Chosen** |
| B. Finish the recorder first (phases, touchdown, GSX, procedures), then the airline | Each recorder part done once, but weeks of flying with nothing to show in the airline | |
| C. Build in flow order, each step complete before the next | Easy to follow, but the summary and money (what makes a flight count) come last | |

## The thin slice

### Milestone 4 — Airline foundation

- **Config files** (D-076) for the airline (name, ICAO code, callsign, starting cash), fleets, and airplanes
  (registration, SimBrief airframe, which catalog airframe, lease start date, location, delivery status). The
  aircraft catalog of types and airframes stays in YAML (section 3, D-091).
- **The real airline database**, replacing the Qt test's `blocktime.db`. It recognises the test schema
  (`user_version` 1) and discards it, so the made-up tails never mix with real data (D-061 note).
- **The schema follows the database rules** in `CLAUDE.md` (D-078). Milestone 4's plan starts with the schema and
  its entity diagram in `docs/database.md`, for Dave to review before code.
- **Airport data** from OurAirports (D-079): imported once, filtered to usable airports, refreshed manually;
  coordinates, runways (for the runway-length check), time zones.
- **Airplane state:** location and fuel on board (section 11, "fuel stays in the tanks").
- **Money ledger:** starting cash, and the **monthly lease charge** (section 5): the first month when an airplane's
  lease starts, then each new month charged whenever the app opens (and every minute while it's open). Negative
  cash follows section 5. A simple money screen or panel shows cash and the ledger lines.
- The Fleet screen shows the real airplanes, their location and fuel.
- **Done when:** with Dave's config, the app shows his airline, his BBJ2 at KBFI awaiting delivery, the right
  cash after the first lease month, and an old test database is cleanly replaced.

### Milestone 5 — New Flight and plan

- **New Flight** from a parked aircraft (section 13.1): destination, runway check, the BBJ2's VIP airliner /
  charter choice, delivery flights forced empty. Loads rolled (section 9), with the **belly cargo option** on
  passenger airliners that can carry it (D-089; never the BBJ2). The flight number is the airplane's default free-flight number, pre-filled and editable (D-106). The
  limits it needs (min runway, MLW, passenger and bag weight, max payload) are airframe defaults in the catalog,
  overridable per airplane in `airline.yaml` (D-100).
- **Money preview** before committing (design rule 1): revenue, each cost line, the total, where the plane ends up.
- **Planning** (section 13.2, D-070, D-101): SayIntentions ATIS for both airports, the pre-filled SimBrief dispatch
  page, an **Import Active SimBrief Plan** button, storing a copy of the plan.
- The live dashboard's **before departure** state (section 13.3): "Waiting for plan" / "Waiting for sim", the route
  with its copy button, where to load in, and Cancel flight.
- **Done when:** Dave sets up the KBFI→KSAN delivery flight, the dispatch page opens filled in with ATIS runways,
  and after Generate, Import Active SimBrief Plan brings the plan into the app.

### Milestone 6 — Record the planned flight

- **Matching check** (section 13.4, D-088): aircraft type and origin, the "Not recording" banner, and
  `block_out_refused` in the log. Also the **fuel-on-board check** (section 11): the departure isn't accepted if
  the sim shows less fuel than the airplane has stored.
- **The flight is saved** to the database with its plan. The arrival airport comes from the block-in position, so
  a diversion closes where the plane parked (section 14).
- **Interrupted flights are discarded** with a notice (section 13.8, D-072).
- **Plain summary** (section 13.7): income and expense lines, total, the route map. No score yet.
- **Settling the flight:** money posted to the ledger, the aircraft moved, fuel on board updated, Dave's hours
  added (counted from the flights in the database, not from the log; D-114).
- **At Gate Turnaround** and **Done for now**.
- **Probe flight** during the sim tests: log every GSX variable through a full turnaround, PMDG's switches
  (beacon, strobe, seatbelt, APU, lights), and the sim's parking and runway facility data, including what
  SayIntentions calls the gates. Results go to `docs/notes/`. This clears the unknowns of milestones 8–10 early.
- **Done when:** Dave flies the delivery leg and a revenue leg back to back with a turnaround, and the summary,
  cash, aircraft location, fuel and hours are all right. A flight in the wrong aircraft is refused; a flight left
  for the main menu is discarded.

## Deepening

### Milestone 7 — Phases and the six stages

**Built in 0.8.0** (`docs/plans/2026-11-23-milestone-7-phases-and-stages.md`, D-124..D-129); Dave's sim checks are in
`docs/progress.md`.

The phase rules in `recorder.md` (old milestone 4, with D-058), touchdown capture (old milestone 5), and the
dashboard's six stages and sub-events (section 13.5): altitude markers, transition altitude and level and
"200 nm from destination" from the plan, the departure runway from the sim's runway data.

It also starts logging what milestone 10's analysis needs (D-123). These can't be fetched after a flight:
- the SayIntentions ATC log, polled during the flight;
- the origin and destination ATIS, in the phases that use them;
- the altimeter settings.

### Milestone 8 — GSX

**Built in 0.9.0** (`docs/plans/2026-11-24-milestone-8-gsx.md`, D-134..D-138) and checked in the sim on 2026-11-24
(KSLC → KLAS, `docs/progress.md`). Each flight also stores its phase and GSX service times for the leg templates (D-137).

`gsx_service` events (D-073), each service on the dashboard with start and end, the "Turnaround and Boarding"
line, and "Ready for Shutdown or Turnaround" with its checklist (the items the probe found readable).

### Milestone 9 — Gates

**Built in 0.10.0** (`docs/plans/2026-11-24-milestone-9-gates.md`, D-145..D-149); Dave's sim checks are in
`docs/progress.md`.

The arrival gate rule and `assignGate` (section 13.6, D-074), asked once SayIntentions has picked its own gate and
landing runway (D-141; the fields are in `docs/notes/sayintentions-api.md`), preferred gates in config, the parked-at gate in the
summary, and the load-in gate for the next flight. Live's Flight progress card moves out of the full width into
the row below the planned flight header: progress on the left, the flight data in the middle, the event log on the
right (design.md section 13.5, D-139). The flight panel loses the Aircraft card and the Position tile, Parking brake
and each engine become green/grey status tiles, and the airspeed tile switches to Mach at M0.70 and back to knots
below M0.66 (D-140). Approach starts at the first flap extension below 10,000 ft instead of 30 nm out, after a
probe check that the sim's flap value follows PMDG's lever (D-142). "Final approach" shows when lined up with the
landing runway within 10 nm, not at 1,000 ft (D-143).

### Milestone 10 — Procedure capture and Claude's debrief

**Built in 0.11.0** (`docs/debrief.md`, `docs/plans/2026-11-24-milestone-10-debrief.md`, D-156..D-162); Dave's sim
checks are in `docs/progress.md`. **The MVP is done here.** Closed by Dave on 2026-11-25 after the sim test (0.11.1).

Old milestone 7 and D-040: cockpit state for the analysis, the SayIntentions ATC log (collected into the event log since 0.8.0, D-123), and
the score and evaluation in the summary. As built: BlockTime's code measures fourteen checks (twelve, then two APU checks in 0.11.1) and the landing and computes
the score and stars; Claude Opus 5.5 writes the review (D-156).

## After the MVP: the dashboard (D-163)

Deepening the live flight comes first. Order agreed with Dave on 2026-11-24; each still gets its own plan.

### Milestone 11 — Flight replay

**Built in 0.12.0** (`docs/plans/2026-11-25-milestone-11-replay.md`, D-175..D-178); closed by Dave on 2026-11-25 after
trying replays of his own flights.

`docs/replay.md`, D-164, D-166, D-167, D-175. A Replay button on every Flight Summary opens a second, read-only
BlockTime window that plays the flight on Live at 1×, 2×, 3× or 10×, through a second `RecorderBridge` fed from the
log. The test bench for the milestones below, and a `--replay … --at … --grab` command so Claude Code can check panels
itself. No sim test needed.

### Milestone 12 — The dashboard header and status lines

`docs/dashboard.md`, D-151, D-152, D-154: the identity row, the route arc, the phase pill, the three status lines and
the header staying at block in. Block-in time shows the plan's until the predictions exist. Checked on replays.

**Built in 0.13.0** (`docs/plans/2026-11-25-milestone-12-dashboard-header.md`, D-180..D-185); reworked with Dave from
mockups (D-184) and closed by him on 2026-11-26 after trying it on replays.

### Milestone 13 — The Pre-Flight panel

`docs/dashboard.md`, D-153: the load sheet, departure data and the clearance, in the instrument panel look (D-179). The clearance is read from the logged
`atc_exchange` lines (D-163), through one plain ATC form (D-168), polled at the gate so it shows before pushback
(D-188). Also the header collapsing to the pill bar with a click (D-187). Mockup first. No frequency readout yet
(D-187).

**Plan:** `docs/plans/2026-11-26-milestone-13-preflight-panel.md`.

**Built in 0.14.0** (D-187..D-190): the mockup approved by Dave, then built to it and checked on replays of Dave's
KSAN → KSJC leg. Closed by Dave on 2026-11-26, then reopened for the first flight's fixes: **0.15.1** (D-205..D-208:
200 px gauges gliding over each second, the GATE and ARR GATE cards, Finish Flight, approach transitions and named
visual approaches, a 0-500 ft radio altitude from 300 ft, the G on LANDING; D-209). Closed again by Dave on 2026-11-26
after reviewing the app.

### The in-flight brainstorm (2026-11-26)

Designed with Dave on 2026-11-26 (`docs/dashboard.md`, "The in-flight panel"; D-191 to D-199; mockup
`docs/images/dashboard-inflight-v1.html`). No radio frequencies anywhere (D-191). Split into three milestones (D-199).

### Milestone 14 — The in-flight panel

One instrument panel below the header from block out to block in (D-194): the lamp strip, flashing while a switch is
wrong by the debrief's rules (D-195); the speed gauge (ground speed 0–40 kt on the ground, then airspeed), altitude,
vertical speed and fuel (D-196); the ATC card with the gist of each clearance, slot by slot (D-197); the readouts for
each phase, including the arrival ATIS; cards and gauges flipping in a left-to-right wave when they change (D-198).
Also the ATIS letter as its word (D-193) and the departure frequency off the Pre-Flight panel (D-191). A v2 mockup
first, with what changed after v1. Also (D-200): ATC fetched every 30 s from block out, the destination ATIS from
200 nm out, and debrief check 2 ending at the transition altitude for new flights. Checked on replays; no sim test.

**Plan:** `docs/plans/2026-11-26-milestone-14-inflight-panel.md` (D-201).

**Built in 0.15.0** (D-201..D-204): to Dave's approved mockup v2, with his additions (souls on board and fuel
remaining in every phase, the pounds on the fuel gauge, the radio altitude face, D-203). Checked on replays of the
KSAN → KSJC leg. Closed by Dave on 2026-11-26.

### Milestone 15 — The predictions and the passenger header

The header's `Arrival 12:35 PM` with `On time` / `Early ~4m` / `Delayed ~4m` under it, measured from block out
(D-192); the arrival estimate as now plus the plan's time from the current position (refining D-120); top of climb,
top of descent (and its tick on the arc), landing time, fuel against plan and fuel at landing on the panel; the plan's
cruise speed on the airspeed gauge; the altitude and speed on the header's distance line,
`127 NM flown · 103 NM to go · FL350 · M.74` (flight levels above the transition altitude, indicated airspeed;
D-210). The taxi times are Dave's own, the median of his last 5 at the airport, SimBrief's where he hasn't flown
(D-212). Last, developer mode: `--dev` shows Live's flight tiles and event feed, hidden without it (D-211). Checked
on replays; no sim test.

**Built in 0.16.0** (plan `docs/plans/2026-11-26-milestone-15-predictions.md`), checked on replays of the KSAN → KSJC
leg.

Milestone 15 is the last milestone to the MVP; milestone 16 (developer mode) was folded into it (D-211). What comes
next is a new brainstorm, starting from "After the MVP" below.

**Every panel below the header uses the instrument panel look** (D-179, `docs/dashboard.md`): an inset panel with
steam gauges for values that move, glass indicator lamps for what the debrief scores, and radio-stack readouts for
other numbers. Not in milestone 12: the header stays flat.

## Next, after the MVP (brainstorm of 2026-11-27)

Dave's order: the technical review, then dispatch and the summary, then Journeys; the Flight Board after rotations.
The review is done (`docs/reviews/2026-11-27-technical-review.md`) and its fix order approved (D-225): steps 1, 2
and 3 are built. Agreed with Dave on 2026-11-27, with the money item's place Claude's suggestion. Each milestone still gets
its own plan in `docs/plans/` first.

1. **Fix step 3 — groundwork for dispatch.** Built (D-238): the preview's fuel is the fuel to add (D-223), the
   money inputs priced one way with a test that New Flight and the planned flight agree (review 4.3), money lines
   that carry their workings (4.4), the migration runner's rebuild support (1.5) and migration 10's lookups (2.1).
   The accepted money (D-222) is stored in milestone 16.
2. **Milestone 16 — dispatch on Your Flight** (`docs/dispatch.md`, D-214, D-215, D-233, D-234). Live renamed Your
   Flight; the form with fully loaded, additional fuel and remembered routes; the dispatch report (the ring, the
   split-flap board, load and plan); the import check against the rolled loads; Accept; View Dispatch Report; money
   and Re-import locked at block out; Settle Up. Includes the review's `LiveStage` and `PlannedFlightModel` split
   (3.1, 3.2), the accepted state and reopening the form (4.5), and 4.6 and 4.7. Also the log changes that Accept
   makes possible: Accept marks the flight's start in the log (D-236), each flight gets its own log file, and a
   one-time splitter copies the old daily logs, which stay untouched (D-237).
3. **Milestone 17 — the summary and the debrief restyled** (`docs/dispatch.md`, D-235). The map with the altitude
   track, the score and "what this flight moved forward" cards, the settled money with its changes from the dispatch
   report, the debrief with Claude's review (…more), the checks as lamps under the stages, and the landing. Which
   milestones the summary shows is settled in its plan.
4. **Milestone 18 — the money item** (D-216 to D-221). Fares towards real ones, the lease charged by use, the
   flights-per-month setting (default 20). A short brainstorm first to set the numbers, checked against Dave's real
   flights; after milestone 16, since the dispatch report shows every line's working. A per-use lease needs the
   review's `lease` table (2.2).
5. **Fix step 4 — groundwork for Journeys.** Tracks in the database with a backfill (2.3), the `LiveFlight` object
   (3.3), the `Recorder::run` split with a re-record test (3.5).
6. **Milestone 19 — Journeys** (design.md §16), without the Flight Board.
7. **Then** rotations and background flying (sections 7–8, D-075), and after them the Flight Board (D-144). The
   settings screen and the MCP server for Claude (D-186), timing open.

**Fix step 5** (the docs, dead code, the shared windows and accessors, the rest of the review's §6 and §7) runs
alongside, a little at a time.

## After the MVP (not ordered)

Rotations unlocked by flying (D-075) and background flying with catch-up (sections 7–8), the settings screen (airline, fleets, leasing, and managing the
catalog's types and airframes in the app, D-092),
instant delivery, the Flight Board and Journeys (D-144, design section 16; journeys need no background flying,
so they and a board showing them could come before it), the pre-departure view on Live (D-118, design section 16), the fuel burn estimate for
previews (D-119, design section 3), predictions on Live such as top of descent and gate arrival (D-120, design
section 16), and whatever the deepening turns up.

Towards the end, timing open: **an MCP server for Claude** (D-186), part of BlockTime itself and on whenever the app
runs, on the sim PC only. Claude on that PC can then read flights for a deeper analysis than the debrief's.
Designed when it joins a milestone.

### Live ideas (for the brainstorm after milestone 10)

Deepening the live flight comes first after the MVP. Ideas collected while testing, one line each; the brainstorm
after milestone 10 sorts and designs them.

The first brainstorm (2026-11-24, during milestone 9) designed the dashboard's header, status lines and Pre-Flight
panel: [dashboard.md](dashboard.md), D-150 to D-154. The ideas below that it doesn't cover yet stay open.

- **Pre-departure view** (D-118): everything needed to set up the FMC before pushback; Live switches to the in-flight
  view at block out.
- **Predictions** (D-120): top of climb and descent, landing and gate arrival, shifted by ahead/behind plan.
- ~~**Developer mode:** Live's normal view designed for flying, with today's debugging data (raw sim values, events,
  phase-rule state) behind a flag or a setting.~~ A `--dev` flag in milestone 15 (D-211).
- **Altitude-coloured track:** the flight's line on the map coloured by altitude, red near the ground through yellow
  and green to blue at cruise, then back through green and yellow to red on the way down, fading smoothly between
  colours rather than in bands.
- **Short final view:** on short final the dashboard shows altitude above ground, and below 300 ft it refreshes at
  a higher rate. (The recorder already samples fast only briefly around touchdown, milestone 7's touchdown capture;
  this would show that data live.)
- **Distance to destination card:** distance remaining to the destination, along the planned route.
- **TOD card:** distance to top of descent counting down, and the estimated time it starts in Dave's local time
  (the Windows time zone), for example "TOD: 45 NM, ~9:15 AM". The first concrete piece of the predictions (D-120).

## Before starting, Dave needs to

- **Milestone 5:** request a **SayIntentions developer key** (`getWX` needs it alongside his pilot API key), and
  have his **SimBrief username or user ID** for the settings file. Done: Dave has both (2026-11-23).
- **Milestone 5:** seats per class, hold maximums and hold ratios per configuration from the PMDG load pages
  (`design.md` section 17), at least for the configurations he flies first. Until then, the design's estimates.

## Calls made by Claude (confirmed by Dave, 2026-11-22)

- Config files live in `%LOCALAPPDATA%\BlockTime\` next to `settings.json`, with an example copy in the repo; keys
  (CARTO, SayIntentions, SimBrief) stay in `settings.json`, never in the repo.
- An airplane awaiting delivery is marked in config; New Flight then offers only its delivery flight.
- The lease month runs from each airplane's lease start date.
- The probe flight is part of milestone 6's sim tests rather than its own milestone.
- Dave's SPAD.neXt profiles are used for the probe flight's PMDG variable names; names go into `docs/notes/`, and
  the profiles themselves aren't committed unless they're Dave's own.
