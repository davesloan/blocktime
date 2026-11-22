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
