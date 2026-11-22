# BlockTime — product design

This document describes how the virtual airline works. It was worked out with Dave before any code was written.
All numbers are starting values meant to be tuned in config once the app runs. When behaviour changes, update this
file and log the decision in `decisions.md`.

## 1. Purpose and principles

- **Built for Dave.** Public on GitHub so others can play with it, but no effort spent making it easy for others to
  set up.
- **Just the virtual airline part.** Skip FlyHub-style deep simulation; that depth takes the fun out of it.
- **Why:** something Dave enjoys using, and a real test of building production software with AI.
- **Platform:** Microsoft Flight Simulator 2020 only. Aircraft: PMDG 737 and 777 families. Flight planning: SimBrief.

### Design rules

1. **See the outcome before you commit.** Revenue, costs and where the plane ends up are shown before a flight
   starts. No hidden rolls after the fact.
2. **Forgiving.** Experimenting is free: no harsh penalties for cancelling, diverting or a firm landing.
3. **Few numbers, all meaningful.** No crew fatigue, maintenance schedules, competitors or hidden markets.

## 2. Architecture

- **Recorder:** native Windows app in C++ using the MSFS SimConnect SDK. Runs as its own process, writes flight
  events to a local log, reports to the web app. See `recorder.md`.
- **Web app:** Node.js + TypeScript, runs **locally on Dave's PC** with a browser UI on localhost. No hosting, no
  accounts, no login.
- **Storage:** one SQLite file holds the whole airline; backup = copy the file.
- **Catalog:** aircraft types/configurations in a hand-edited YAML file.
- **No running server clock needed.** The airline runs on real time, but background progress is calculated when the
  app opens ("catch-up"), and then every minute while it's open.

## 3. Aircraft catalog

Three layers:

1. **Aircraft type** — what the plane is (737-600, 737-800, 777-200ER…): cruise speed, range, lease cost,
   turnaround time.
2. **Configuration** — how a type is set up (two-class, all-economy, freighter…): seats per class or cargo capacity,
   max weight per hold, hold split ratio, SimBrief airframe ID, sim aircraft title, sells-spare-belly-cargo flag.
3. **Airframe** — one leased plane: registration, optional tail number, livery, current location, status, fuel on
   board, hours.

The catalog contains only aircraft Dave flies and is edited by hand as his fleet changes.

### Starting configurations

| Type | Configuration | Notes |
|---|---|---|
| 737-600 | Two-class | ~112 seats (verify in PMDG) |
| 737-600 | All-economy | ~130 seats |
| 737-800 | Two-class | ~160 seats |
| 737-800 | All-economy | ~184 seats |
| 737-800 BBJ2 | VIP, 48 seats, aux tanks | Extended range / ETOPS; see Executive jets |
| 737-800 BCF | Freighter | |
| 777-200ER | Trent 895 engines, heavier weight variant | One cabin layout |
| 777-300ER | Standard | One cabin, one engine |
| 777F | Freighter | |

Exact seats per class, hold maximums and hold ratios are taken from the PMDG load pages when needed during building.

### SimBrief is the source of truth

Each configuration stores its SimBrief airframe ID. Weights and limits (empty weight, MZFW, MTOW, MLW, max fuel,
passenger and bag weights, minimum runway) are synced from SimBrief rather than retyped. SimBrief OFP data is
available via `https://www.simbrief.com/api/xml.fetcher.php?userid=<id>&json=v2`; SimBrief deletes old OFPs after a
few days, so store a copy of each plan when a flight is filed.

### Fuel burn

Learned per **configuration** from Dave's actual flights; used only for previews. Until enough data exists, previews
show "+ fuel cost". SimBrief always decides the actual fuel loaded.

## 4. Where aircraft are

- **Planes stay where they land.** A flight can only start where one of the airline's aircraft is parked.
- **What's parked decides the flight:** only freighters at an airport means only cargo flights from there.
- **Moving a plane is just a flight.** Every flight between airports carries a paying load. There are no empty
  repositioning/ferry flights, with one exception: factory delivery.
- **No hubs and no airport leases.** Fly anywhere from where a plane is parked; the only physical limit is runway
  length (configuration's minimum runway vs the airport's longest runway).

## 5. Leasing

- **Wet lease, paid monthly in advance.** No buying, no loans. Crew comes with the lease (explains crew costs; no
  pilots to manage). Maintenance is a flat per-hour cost (see Costs).
- **Return anytime, no penalty.** Prorated refund for the unused part of the month.
- You can't lease a plane you can't afford that month.

### Monthly lease rates

| Type | Lease / month |
|---|---|
| 737-600 | $750k |
| BBJ2 | $900k |
| 737-800 | $1.4M |
| 777-200ER | $1.9M |
| 777-300ER | $2.3M |
| 737-800 BCF | $500k |
| 777F | $3M |

Rationale: a lease takes roughly half of a busy plane's monthly flight profit, so planes must keep working to pay
for themselves.

### Negative cash

No bankruptcy. While cash is below zero you can't lease new planes. A lease that can't be paid returns the plane.

## 6. Setup and growth

1. **$5M starting cash** (config value).
2. Airline identity: name, logo, ICAO code, callsign.
3. Create the first fleet. Fleets are just named groups (passenger, cargo, premium…); add them any time.
4. Lease the first airframe: registration, optional tail number.
5. **Delivery:** fly it yourself from the factory (737s from Boeing Field KBFI, 777s from Paine Field KPAE), or pay
   that delivery flight's estimated cost for instant delivery to any airport. The delivery flight is the only empty
   flight in the game.

### Fleet growth: earn slots by flying

- Start with **1 aircraft slot**. Slots cap the whole fleet (active and inactive).
- Each new slot requires more hours flown **by Dave** (any aircraft, including free and delivery flights).
  Background hours don't count.
- Each slot costs 10 more hours than the last:

| Slot | Hours for this slot | Total hours |
|---|---|---|
| 2 | 10 | 10 |
| 3 | 20 | 30 |
| 4 | 30 | 60 |
| 5 | 40 | 100 |
| 6 | 50 | 150 |
| 8 | 70 | 280 |
| 10 | 90 | 450 |

## 7. Aircraft states and rotations

### States

- **Inactive:** no rotation. Available for free flights anywhere from where it's parked.
- **Active:** assigned to a rotation and flown in the background.
- **Paused:** keeps its rotation but is temporarily Dave's for free flights. On pause it finishes its current leg.
  It resumes from any airport on its rotation where it's parked (reposition by flying there first if needed).

### Rotations

- A **rotation** is a reusable loop of legs, e.g. KSAN–KSLC–KPDX–KOAK–KLAS–KSAN.
- **One rotation, many planes.** Edit it once; every plane on it follows from its next leg.
- A rotation belongs to **one aircraft type** so its scheduled block times fit every plane on it.
- Each leg has a **scheduled block time** (parking brake release to parking brake set). It drives background flights
  and previews; Dave's own flights use actual block time.
- **No timetable.** An active plane flies its legs back to back, separated by the type's turnaround time:
  **737s 35 min, 777s 60 min**.
- **Staggered starts:** a plane joins the loop at whichever rotation airport it's parked at.

### Taking over legs

- Fly a parked plane's **next leg** yourself; when you close the flight, the plane continues its loop from there.
- If the plane is in the air: **claim** it (it waits at the gate after landing) or **cancel** the leg (it returns to
  its origin, no penalty, leg earns nothing).

### One-off background flights

Schedule a single background flight for an inactive plane (e.g. move a BBJ2 from KBOS to EIDW without flying it).
It is built from a SimBrief plan (route, block time, fuel); the plane lands with the plan's leftover fuel and parks at
the destination.

### Curfew

No passenger departures between **11 pm and 5:30 am local time at the departure airport** for **any background
flight** (rotation or one-off). Background planes simply wait until 5:30 am. Dedicated freighters fly any time.
Dave's own flights are not subject to the curfew, and neither are charters he flies himself.

### Example schedule

One 737 on KSAN–KSLC (2h10) – KPDX (1h50) – KOAK (2h00) – KLAS (1h20) – KSAN (0h50), 35-min turns, starting at KSAN
05:30: about 8 legs/day, overnighting at a different airport each night; the pattern repeats every 3 days (25 legs,
5 loops). Because schedules are deterministic, the app can show future departures.

## 8. Background simulation (catch-up)

- Each plane stores a **last known point**: where it is, when it got there, and which leg is next.
- **Catch-up** replays every plane's schedule from its last known point up to now, **in time order across all planes**
  (so money events like lease payments land on their real dates), saving each completed leg with its rolled loads and
  costs, then moving the last known point forward.
- Any user action (claim, cancel, own flight, pause, rotation edit) sets a new last known point; the schedule is
  recalculated from there.
- Completed legs are stored once and never recalculated; history doesn't change.
- While the app is open it catches up every minute, which keeps live boards current.
- **Boards:** a current-flights board (every plane, where it is in its rotation) and a departures board per airport,
  both computed from the same schedule logic.

## 9. Loads

| Flight | Load |
|---|---|
| Passenger, departing 7 am – 7 pm | random 90–100% |
| Passenger, departing after 7 pm or before 7 am | random 40–80% |
| Dedicated freighter | random 50–100% (any time) |
| Spare belly cargo on passenger flights | random 10–100% of hold space left after bags |
| Charter (BBJ2) | random 10–100% of seats; doesn't change earnings |

Loads are rolled when a flight is planned (shown exactly in the preview) or during catch-up for background legs.
No flights are cancelled for low loads; every leg flies.

### Loading a flight

1. **Passengers:** roll each cabin class separately within the flight's band; round down.
2. **Bags:** passengers × bag weight (from the SimBrief airframe).
3. **Cargo:** if selling spare space (or a freighter), roll the cargo range of the space left after bags.
4. **Split by hold:** distribute hold weight by the configuration's ratio (e.g. 4:5 forward:aft on the 737; three-way
   forward/aft/bulk on the 777), capped at each hold's max; overflow spills into another hold if it has room.
5. **Check limits:** over max payload (MZFW − empty weight)? Trim cargo first, never bags.

The app does not compute centre of gravity; the ratio keeps loading balanced and PMDG computes real CG/trim.

### Flight dashboard

Souls on board (passengers + crew), passengers by class, weight by hold, fuel used vs plan, phase of flight, running
revenue/cost estimate.

## 10. Crew

- **Pilots:** 2; 3 for flights over 8 hours; 4 over 12 hours. Freighters carry pilots only.
- **Flight attendants:** FAA minimum by seat count (seats installed, not passengers on board). For more than 100 seats:
  2 plus 1 per additional 50 seats or part thereof (51–100 seats: 2; 20–50: 1).
- **VIP (BBJ2):** 3× the normal flight attendant count.

## 11. Costs per flight

Fixed rates, the same at every airport. No leases, discounts or airport-size pricing.

| Cost | 737 (narrowbody) | 777 (widebody) |
|---|---|---|
| Fuel | $41 per 100 lb (config) | same |
| Landing fee | $4.50 per 1,000 lb MLW | same |
| Departure / gate | $300 | $900 |
| Ground handling | $900 per turn | $2,800 per turn |
| ATC / navigation | $1.00 per nm | $2.00 per nm |
| Catering, per passenger | <3h $5 · 3–8h $15 · 8h+ $35 | same |
| Cargo handling | $40 per 1,000 lb | same |
| Pilots | $200 each per block hour | $300 each per block hour |
| Flight attendants | $55 each per block hour | same |
| Maintenance | $300 per block hour | $700 per block hour |

- **Fuel price** lives in a config file, editable any time; locked when a flight is planned. ($41/100 lb ≈ $2.75/gal at
  6.7 lb/gal.)
- **Fuel stays in the tanks.** Each airframe remembers its fuel at block in. Fuel charged = fuel at block out − fuel
  remaining. Fuel can't be removed: extra stays on board and isn't charged again. The app warns if the sim shows less
  fuel than stored and won't accept the departure until it matches. Preview warns if landing weight approaches MLW.

## 12. Revenue

### Passenger fares

- **Economy = $90 + $0.075 per nm** (great-circle distance). Business = 3× economy. First = 5× economy.
- Same price day or night (night loads already earn less).
- At 500 nm: $127.50 / $382.50 / $637.50. At 750 nm: $146.25 / $438.75 / $731.25.

Margins at 500 nm (737s) / 750 nm (777s), before lease:

| Configuration | 40% load | 70% load | 100% load |
|---|---|---|---|
| 737-600 two-class | −6% | 38% | 56% |
| 737-600 economy | −3% | 40% | 57% |
| 737-800 two-class | 21% | 54% | 67% |
| 737-800 economy | 18% | 52% | 65% |
| 777-200ER | 4% | 45% | 61% |
| 777-300ER | 11% | 48% | 64% |

(Modelled with estimated fuel burns and seat splits; re-check once real data exists.)

### Cargo

- **$15 + $0.028 per nm, per 100 lb** ($29 at 500 nm). Same rate for freighters and spare belly cargo.
- A half-empty 737 BCF loses ~4%; the 777F almost never loses money.

### Executive jets (BBJ2, 48 seats)

Two ways to operate; both can run rotations:

- **VIP airliner:** per-seat tickets, normal gates, curfew and load rules. Never loses money.
  Fare = **$180 + $0.48 per nm per seat** ($420 at 500 nm). Margins ~5% at 40%, ~44% at 70%, ~61% full.
- **Charter:** flat rate for the whole plane, **$7,500 per block hour, 2-hour minimum**. Passengers random 10–100% of
  seats; earnings don't depend on them. Curfew applies only when flown in the background.
- **VIP costs:** catering doubled ($10 / $30 / $70 per passenger); 3× flight attendants; pilots $250 and FAs $75 per
  block hour.
- **Charter ground costs:** a **$1,500 GA services fee** (FBO, e.g. Signature) replaces gate and ground handling.
  Landing fee still applies.

## 13. Flight recording (summary)

Full detail in `recorder.md`.

- Flights start at parking brake release at the gate and end when it's set at the destination.
- **Sim crash:** the user chooses **discard** (never happened; plane back at origin) or **complete** (treated as a
  background flight; plane moved to the destination).
- **Diversion:** the flight closes where the plane actually parked; revenue counts; a rotation plane is flagged out of
  position until repositioned.
- Phases are detected by the recorder (plan-aware, forward-only) and displayed in the web app.

### Flight analysis (D-040)

At the end of each of Dave's flights, the web app sends Claude, through the Claude API:
- the recorder's events for the flight,
- the SayIntentions ATC log,
- the ATIS for the origin and destination airports.

Claude sends back a written analysis and a **0–100 score**. Examples of what it checks:
- Fully configured and stable by 1,000 ft.
- Beacon on before pushback.
- Landing lights on below 10,000 ft.
- Seatbelt signs on below 18,000 ft.
- Altimeters set to the right pressure at the right time (standard above the transition altitude, local QNH from
  the ATIS below the transition level).

The full list of checks, how they're scored, and whether the score affects anything in the airline get brainstormed
when the recorder's procedure-capture milestone starts.

## 14. Reference data

- **Airports:** OurAirports public dataset (coordinates, runways). Time zones derived from coordinates.
- **Distance for fares:** great-circle distance between airports (not the SimBrief route distance).

## 15. Later

- **SayIntentions integration:** now part of the flight analysis (section 13, D-040). How to get the ATC log and
  ATIS out of SayIntentions is still to be found out.
- Optional flavour ideas: special cargo jobs with supernumeraries on the 777F; display-only "published schedule"
  labels.

## 16. Open items

- Exact seats per class, hold maximums and hold ratios per configuration (collect from PMDG when needed).
- Delivery mechanism from recorder to app (local file + HTTP/WebSocket?) — decide when building the app side.
