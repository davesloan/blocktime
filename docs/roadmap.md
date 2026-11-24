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

The phase rules in `recorder.md` (old milestone 4, with D-058), touchdown capture (old milestone 5), and the
dashboard's six stages and sub-events (section 13.5): altitude markers, transition altitude and level and
"200 nm from destination" from the plan, the departure runway from the sim's runway data.

It also starts logging what milestone 10's analysis needs (D-123). These can't be fetched after a flight:
- the SayIntentions ATC log, polled during the flight;
- the origin and destination ATIS, in the phases that use them;
- the altimeter settings.

### Milestone 8 — GSX

`gsx_service` events (D-073), each service on the dashboard with start and end, the "Turnaround and Boarding"
line, and "Ready for Shutdown or Turnaround" with its checklist (the items the probe found readable).

### Milestone 9 — Gates

The arrival gate rule and `assignGate` (section 13.6, D-074), preferred gates in config, the parked-at gate in the
summary, and the load-in gate for the next flight.

### Milestone 10 — Procedure capture and Claude's debrief

Old milestone 7 and D-040: cockpit state for the analysis, the SayIntentions ATC log (still to find out how), and
the Claude score and evaluation in the summary. **The MVP is done here.**

## After the MVP (not ordered)

Rotations unlocked by flying (D-075) and background flying with catch-up (sections 7–8), the settings screen (airline, fleets, leasing, and managing the
catalog's types and airframes in the app, D-092),
instant delivery, boards, the pre-departure view on Live (D-118, design section 16), the fuel burn estimate for
previews (D-119, design section 3), predictions on Live such as top of descent and gate arrival (D-120, design
section 16), and whatever the deepening turns up.

### Live ideas (for the brainstorm after milestone 10)

Deepening the live flight comes first after the MVP. Ideas collected while testing, one line each; the brainstorm
after milestone 10 sorts and designs them.

- **Pre-departure view** (D-118): everything needed to set up the FMC before pushback; Live switches to the in-flight
  view at block out.
- **Predictions** (D-120): top of climb and descent, landing and gate arrival, shifted by ahead/behind plan.
- **Developer mode:** Live's normal view designed for flying, with today's debugging data (raw sim values, events,
  phase-rule state) behind a flag or a setting.
- **Altitude-coloured track:** the flight's line on the map coloured by altitude, red near the ground through yellow
  and green to blue at cruise, then back through green and yellow to red on the way down, fading smoothly between
  colours rather than in bands.
- **Short final view:** on short final the dashboard shows altitude above ground, and below 300 ft it refreshes at
  a higher rate. (The recorder already samples fast only briefly around touchdown, milestone 7's touchdown capture;
  this would show that data live.)

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
