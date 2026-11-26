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

- **One native Windows app** (D-063): C++20 with Qt 6 and a QML interface, running **locally on Dave's PC**. No
  hosting, no accounts, no login.
- **Recorder:** the part of the app that talks to MSFS through SimConnect, on its own thread. It writes flight events
  to a local log before anything else (D-008). See `recorder.md`.
- **Airline logic** (money, loads, rotations, catch-up) in plain C++ libraries with unit tests. QML only displays, so
  the logic doesn't depend on the UI.
- **Storage:** one SQLite file holds the whole airline (`%LOCALAPPDATA%\BlockTime\blocktime.db`); backup = copy the
  file.
- **Catalog:** aircraft types and airframes in a hand-edited YAML file until the settings screen (D-092).
- **No running server clock needed.** The airline runs on real time, but background progress is calculated when the
  app opens ("catch-up"), and then every minute while it's open.

## 3. Aircraft catalog

Three layers:

1. **Aircraft type** — what the plane is (737-600, 737-800, 777-200ER…): cruise speed, range, lease cost,
   turnaround time.
2. **Airframe** — a template in the catalog: a type in one configuration (737-800 two-class, 737-800 all-economy,
   777F…): seats per class or cargo capacity, max weight per hold, hold split ratio. The type's role (airliner,
   executive or freighter) decides the kind of flight and whether belly cargo can be carried (only airliners; D-089,
   D-090). The type carries what identifies it in the sim (D-088).
3. **Airplane** — one leased plane, made from an airframe (D-091): registration, optional tail number, its own
   SimBrief airframe (D-087), livery, current location, status, fuel on board, hours.

In software terms the airframe is the class and the airplane the object. When an airplane is leased, its airframe's
values are copied into the database with it (D-082). "SimBrief airframe" is SimBrief's own name for a saved aircraft
profile; each airplane has one, carrying its registration.

The catalog contains only aircraft Dave flies and is edited by hand as his fleet changes. After the MVP it's
managed in the app instead (D-092).

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

Each airplane stores its SimBrief airframe ID (D-087). SimBrief has no airframe API, and New Flight needs a few
values before any plan exists, so they're typed in once (D-100): **minimum runway, MLW, passenger weight, bag weight
and max payload** (MZFW − empty weight). Each catalog airframe holds them as defaults; an airplane may override
them, and they're copied onto it at lease time. They must match the airplane's SimBrief airframe: changing one means
a new SimBrief airframe (on the settings screen, a change blanks the ID). Everything else (MTOW, max fuel, the plan
itself) is SimBrief's. SimBrief OFP data is
available via `https://www.simbrief.com/api/xml.fetcher.php?userid=<id>&json=v2`; SimBrief deletes old OFPs after a
few days, so store a copy of each plan when a flight is filed.

### Fuel burn

An estimate in **pounds per great-circle NM**, per catalog airframe, used only for previews before a SimBrief plan
exists (D-119). SimBrief always decides the actual fuel loaded.

- **Entered:** an optional value on the airframe (in `catalog.yaml` until the settings screen exists). It's used
  whenever it's set and not 0.
- **Learned:** BlockTime works out its own value from completed flights longer than 200 NM (great-circle) on any
  airplane of that airframe, once there are at least 3: fuel burned (block-out fuel − shutdown fuel) divided by
  the great-circle distance, averaged. It's used when nothing is entered, and shown next to the entered value on
  the settings screen.
- **Neither:** the preview shows "+ fuel cost".
- The estimate is padded 10% (a constant in `pricing.h`) to allow for variation. Great-circle distance is what
  previews know, so routing and typical winds end up in the number.

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
| 737-600 | $125k |
| BBJ2 | $150k |
| 737-800 | $235k |
| 777-200ER | $315k |
| 777-300ER | $385k |
| 737-800 BCF | $85k |
| 777F | $500k |

Rationale (D-132): a plane Dave flies himself about 15 legs a month roughly covers its lease. Until background flying
exists (after the MVP), his own flights are the airline's only income. When it comes, a plane's background legs are
profit. The first rates (about six times these) assumed busy planes earning in the background from the start.

### Negative cash

No bankruptcy. While cash is below zero you can't lease new planes. A lease that can't be paid returns the plane.

## 6. Setup and growth

**MVP (D-076):** the airline identity, fleets and leased airplanes come from hand-edited config files. A settings
screen for all of this comes after the MVP.

1. **$5M starting cash** (config value).
2. Airline identity: name, logo, ICAO code, callsign.
3. Create the first fleet. Fleets are just named groups (passenger, cargo, premium…); add them any time.
4. Lease the first airplane: registration, optional tail number.
5. **Delivery:** fly it yourself from the factory (737s from Boeing Field KBFI, 777s from Paine Field KPAE), or pay
   that delivery flight's estimated cost for instant delivery to any airport. The delivery flight is the only empty
   flight in the game.

### Fleet growth: earn slots by flying

- Start with **2 aircraft slots** (D-131). Slots cap the whole fleet (active and inactive; returned airplanes don't count).
  Enforced when an airplane is leased (D-083): one that doesn't fit waits, with a notice, until the hours earn its
  slot. Hours are the sum of Dave's flights' block times (D-114). Fleet shows them: "3.2 h flown · 1 of 2 slots in
  use · slot 3 at 10 h".
- Each new slot requires more hours flown **by Dave** (any aircraft, including free and delivery flights).
  Background hours don't count.
- Each slot costs 10 more hours than the last:

| Slot | Hours for this slot | Total hours |
|---|---|---|
| 3 | 10 | 10 |
| 4 | 20 | 30 |
| 5 | 30 | 60 |
| 6 | 40 | 100 |
| 7 | 50 | 150 |
| 9 | 70 | 280 |
| 11 | 90 | 450 |

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
- **Belly cargo** is an option on the rotation (D-089): on or off for every leg its passenger planes fly in the
  background. Rotations for the BBJ2, or any type that can't carry belly cargo, don't offer it.

### Rotations are unlocked by flying them (D-075)

- A rotation can be assigned to an aircraft for background flying only after Dave has flown **every leg in it, in
  that direction, in the same family** (any 737, or any 777). KSFO→KSEA and KSEA→KSFO are separate legs.
- A new rotation built only from legs already flown can be assigned at once.
- Only completed flights count. Interrupted flights are discarded (D-072) and never count. Charter legs count
  (real flying in that family).
- **Leg templates.** Dave's first flight of a leg sets its template; later flights of the same leg refine it
  (averaged). Background flights on the leg use it, with a little random variation so they don't all come out the
  same:
  - **Times** (block time, taxi out and in, GSX turnaround times) carry across the family. The leg's scheduled
    block time comes from the template.
  - **Fuel** scales per configuration: the configuration's learned burn (section 3), corrected by Dave's actual
    burn versus plan on that leg (which captures the route and typical winds).
- This sits alongside the slot rule in section 6: slots grow with hours flown, rotations with legs flown. Both
  reward flying, so the airline can't run itself.

### Taking over legs

- Fly a parked plane's **next leg** yourself; when you close the flight, the plane continues its loop from there.
- If the plane is in the air: **claim** it (it waits at the gate after landing) or **cancel** the leg (it returns to
  its origin, no penalty, leg earns nothing). D-144 renames cancel to **reset**, lets Dave fly that leg, and allows
  it only more than 40 nm from the destination; taking over happens from the Flight Board (section 16).

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
  both computed from the same schedule logic. Replaced by the Flight Board (section 16, D-144).

## 9. Loads

| Flight | Load |
|---|---|
| Passenger, departing 7 am – 7 pm | random 90–100% |
| Passenger, departing after 7 pm or before 7 am | random 40–80% |
| Dedicated freighter | random 50–100% (any time) |
| Spare belly cargo on passenger flights, when chosen (D-089) | random 10–100% of hold space left after bags |
| Charter (BBJ2) | random 10–100% of seats; doesn't change earnings |

Loads are rolled when a flight is planned (shown exactly in the preview) or during catch-up for background legs.
No flights are cancelled for low loads; every leg flies.

### Loading a flight

1. **Passengers:** roll each cabin class separately within the flight's band; round down.
2. **Bags:** passengers × bag weight (from the SimBrief airframe).
3. **Cargo:** on a freighter, or a passenger flight with belly cargo chosen (New Flight's option, or the
   rotation's), roll the cargo range of the space left after bags.
4. **Split by hold:** distribute hold weight by the configuration's ratio (e.g. 4:5 forward:aft on the 737; three-way
   forward/aft/bulk on the 777), capped at each hold's max; overflow spills into another hold if it has room.
5. **Check limits:** over max payload (MZFW − empty weight)? Trim cargo first, never bags.

The app does not compute centre of gravity; the ratio keeps loading balanced and PMDG computes real CG/trim.

### Flight dashboard

Souls on board (passengers + crew), passengers by class, weight by hold, fuel used vs plan, phase of flight, running
revenue/cost estimate. How the dashboard follows a flight is in section 13.

## 10. Crew

- **Pilots:** 2; 3 for flights over 8 hours; 4 over 12 hours. Freighters carry pilots only.
- **Flight attendants:** FAA minimum by seat count (seats installed, not passengers on board). For more than 100 seats:
  2 plus 1 per additional 50 seats or part thereof (51–100 seats: 2; 20–50: 1).
- **VIP (BBJ2):** 3× the normal flight attendant count.

## 11. Costs per flight

**Rounding (D-080):** every money line, income or cost, is rounded to the nearest $5 when it's posted, and the
rounded amount is stored, so lines always add up to their totals. Round the line, not the unit. Large figures show
in short form ($1.4M) on tiles and the cash display, in full in the ledger and summary lines.

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

- **The rates** of this section and section 12, and the fuel price, live in `rates.yaml` (D-107). It's read whenever
  New Flight opens and when a flight is settled. The fuel price is locked when a flight is planned. ($41/100 lb ≈ $2.75/gal at
  6.7 lb/gal.)
- **Fuel stays in the tanks.** Each airplane remembers its fuel when the engines were shut down after its last
  flight (D-110). Fuel charged = fuel at block out − fuel on record: the fuel **added**, never below zero, at the
  price locked when the flight was planned. Fuel can't be removed: extra stays on board and isn't charged again. A
  departure with more than 300 lb less than on record isn't accepted (section 13.4). Preview warns if landing weight
  approaches MLW.
- **Settled money (D-109).** At block in, pilots, flight attendants and maintenance are charged for the real block
  time. Every other line (fares, cargo, charter, landing, gate, handling, ATC, catering) is as previewed, from the
  planned distance and the plan's block time.

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

## 13. Flying a leg (the flight flow)

The core of the app (D-069): every other feature feeds this flow or reads from it. Every flight Dave flies starts
here, including delivery and free flights.

### 13.1 New Flight

1. Dave clicks a parked aircraft and chooses **New Flight**.
2. He enters the destination's ICAO code. The app checks the runway length (section 4).
3. **Flight number (D-106):** each airplane has a default flight number for its free flights (the BBJ2 is always
   XLY8, say), pre-filled and editable. A rotation's legs each carry their own number instead (KSAN–KSLC is always
   XLY1414), and no two airplanes or rotation legs share one.
4. **Kind of flight:** decided by the aircraft (passengers for airliners, cargo for freighters, empty for the
   factory delivery). A **BBJ2** asks one more question: **VIP airliner** (per-seat tickets) or **Charter**
   (section 12). A passenger airliner that can carry belly cargo offers one more choice: **Carry belly cargo**
   (D-089). The BBJ2 never carries it.
5. Loads are rolled (section 9) and the preview shows revenue, costs and where the plane ends up (design rule 1).
   A delivery flight has no load and shows a loss: its fuel and operating costs (section 11) with no revenue.

### 13.2 Planning (D-070)

1. **Active runways from SayIntentions.** The app fetches the ATIS for both airports from SayIntentions' `getWX`
   endpoint (pilot API key plus a developer key; no active SayIntentions flight needed) and takes the active
   runways. SayIntentions gives a runway even for an airport without an ATIS, and that guess is used (D-105). Only
   when it gives none is the runway left for SimBrief to choose.
2. **SimBrief dispatch page.** The app opens SimBrief's dispatch page in Dave's browser, pre-filled (the dispatch
   redirect, no API key): origin, destination, the airplane's SimBrief airframe, the runways (`origrwy`,
   `destrwy`), and the loads. Dave can change the route, then presses Generate.
3. **Dave imports the plan (D-101).** After Generate he clicks **Import Active SimBrief Plan**. The app fetches the
   latest OFP (`xml.fetcher.php`) once and keeps it if it matches the planned origin, destination and airframe and
   was generated after planning started (D-105). It stores a copy of the plan (section 3). SimBrief asks apps not to
   poll, so nothing is fetched without a click. Until then the dashboard shows "Waiting for plan", with buttons to
   open the dispatch page again and to cancel the flight.
4. The destination runway is only a planning guess; the wind can change before arrival.

### 13.3 Before departure

- The **live flight dashboard** opens: "Waiting for sim".
- It shows the route with a **copy button**, so Dave can paste it into Navigraph Charts and pick his approach for
  the runways in use. The route is in the form Navigraph takes, with the plan's runways at both ends:
  `KBFI/14R <route> KSAN/27`.
- It shows **where to load in:** the airport, and the spot where the aircraft was last parked, if known ("Load in
  at KLAS, Harry Reid International Airport, Gate E7"; built in milestone 9, D-148). The app can't make MSFS spawn
  there; Dave picks it on the world map.
- Dave starts SayIntentions, SPAD.neXt and the sim, then GSX in the sim.

### 13.4 Matching check (D-088)

A flight is only recorded if the sim matches the planned flight:
- the sim's aircraft is the planned airplane's **type** (737-800, 737-800 BBJ2, 777-300ER…), and
- the aircraft is **at the planned origin airport** (within 3 km of its reference point), and
- it has at least the **fuel on record** for the airplane, less 300 lb (D-110): fuel stays in the tanks (section
  11), so loading less than the last flight left is refused.

The registration isn't checked: it comes from the livery, and one livery serves many registrations (D-087, D-088).
The type is read from the folder of the loaded aircraft (D-112).
If either doesn't match, the dashboard shows a clear **Not recording** banner saying what's wrong (for example
"You're in a 777-300ER at KSEA; this flight expects the 737-800 BBJ2 N738BT at KBFI"), and the brake release doesn't open a flight. Once
the sim matches, the banner goes away.

### 13.5 During the flight: stages and sub-events

The dashboard groups the recorder's phases (`recorder.md`) into six **stages**. Under each stage, sub-events appear
as they happen, each with its start and end time. The same records go into the flight log.

| Stage | Recorder phases | Sub-events (examples) |
|---|---|---|
| Pre-Flight | At gate | Catering, Fuel loading, Boarding (GSX) |
| Departure | Pushback, Taxi out, Takeoff | Pushback (GSX), Taxiing to runway, Takeoff roll, Takeoff |
| Climb | Climb | Departed runway 30, Level-offs, Above 10,000 ft, Passed transition altitude |
| Cruise | Cruise | Cruising at FL350, Step climb to FL370, 200 nm from destination |
| Descent | Descent, Approach, Landing, Taxi in | Top of descent, Passed transition level, Level-offs, Below 10,000 ft, Final approach, Short final, Landed, Taxiing to gate |
| Arrival | Arrived | Parked at Gate E7, Deboarding (GSX), Ready for shutdown or turnaround |

- **Milestone 12 (D-150, D-152, D-180):** the dashboard header replaces the Flight progress card on Live. The six
  stages show as its phase pill, and what happens as three status lines under it (`docs/dashboard.md`); the stage
  lines below are still built (FlightStages) and the stored phase times are unchanged, but Live no longer lists them.
  The first stage is called **Pre-Flight** (was Pre-Departure). The bullets about the card below describe it as it
  was until milestone 12.
- **Built in milestone 7** (D-128): Live's Flight progress card, from the recorder's events. "Landed" carries the
  landing runway and the touchdown rate.
- **Milestone 9:**
  - Arrival shows "Parked at Gate E7", the spot the recorder named at block in, or plain "Parked" (D-148).
  - "Final approach, runway 26L" runs from when Dave is lined up with a destination runway (within 10 nm of the
    threshold, 10° of its heading, 0.5 nm of its centreline, for 10 s; D-143, D-145) until 1,000 ft AGL, where
    "Short final" takes over until touchdown (D-149). A go-around ends both.
  - The arrival gate line ("Arrival gate: Gate E7") appears in the stage that's current when it's settled, usually
    Cruise (section 13.6).
  - Approach starts at the first flap extension below 10,000 ft, or 3,000 ft AGL (D-142).
- **The card spans the gate (milestone 8, D-134).** Before block out it shows the next leg: Pre-Departure with the GSX
  services so far, whenever a flight is planned or a service has run. After block in it stays under the summary with
  the flight's Arrival stage. It switches to the next leg at At Gate Turnaround, Done for now, or a new plan.
- **Three columns (milestone 9, D-139).** Below the planned flight header (which stays full width), Live's row has
  three columns: the Flight progress card on the left, the flight panel in the middle, and the running event log on
  the right. The progress card no longer runs full width. The flight summary is unchanged.
- **The flight panel (milestone 9, D-140):** no Aircraft card or Position tile. Parking brake and each engine (1 on
  the left) are status tiles, green when set or running, grey otherwise. The airspeed tile shows Mach ("M.78") from
  M0.70 until it drops below M0.66, knots otherwise.
- **Departure runway:** from the takeoff position and heading, matched against the sim's runways.
- **Transition altitude and level, and "200 nm from destination":** from the SimBrief plan.
- **GSX services (D-073, D-138):** each service shows as its own line with start and end: "Catering",
  "Refuelling", "Boarding" under Pre-Departure, "Deboarding" under Arrival. Services Dave used appear; ones he skipped
  or abandoned are left out. A GSX push reads "Pushback (GSX)". If no GSX service was completed before pushback,
  Pre-Departure simply shows as done with no lines (D-136, which drops D-073's "Turnaround and Boarding" line).
  "Cleaning" isn't a GSX service, so it isn't shown.
- **Ready for shutdown or turnaround (D-135):** shown when GSX deboarding completes. If deboarding wasn't done through
  GSX, it shows once the checks below all pass; if GSX deboarding starts after that, Ready waits for it to complete.
  Under it, an informational checklist that stays live (never a
  requirement or penalty): engines off, parking brake set, deboarding complete (only when GSX deboarding was used),
  beacon off, strobes off. All from plain SimVars; no seat belts or APU, which the 737 only shows in PMDG's SDK data.
- **Stored (D-137):** each settled flight keeps its phase start times and its completed GSX services in the database
  (`docs/database.md`), for the leg templates (section 7).

### 13.6 Arrival gate (D-074, D-141, D-155)

Once SayIntentions has picked its own arrival gate (usually in cruise; it sets the landing runway and transition at
the same time), the app decides whether to ask for another one with `assignGate` (needs an active SayIntentions
flight), so ATC gives it after landing. **SayIntentions' gate is the default**: it knows which gates and ramps the
scenery has, and sends GA flights to charter ramps and freighters to cargo ramps. MSFS's parking data doesn't: it
lists spots the scenery shows as a construction zone (KSAN) and mid-field stands (KLAS). The app asks for another
spot only for:
1. **A preferred spot** for the airport (`preferred_gates` in airline.yaml, by ICAO, first one that fits), unless
   SayIntentions' gate is already one of them. Gates for passenger and VIP flights; ramps for charters, deliveries
   and freighters, so a favourite ramp works too. Military spots are never used.
2. **A gate too small for the type:** twice the spot's radius is less than the type's wingspan (`wingspan_m` in
   catalog.yaml, D-147). Then the nearest spot of the same kind (gate or ramp) that fits, from SayIntentions' gate;
   if none fits, SayIntentions' gate stands.
- Parking spots (name, position, size, type) come from the sim over SimConnect. Gate names are sent to
  SayIntentions in short form, letters and digits only ("E7" for MSFS's Gate E7, "70" for Parking 70; D-147).
  SayIntentions' own names aren't always MSFS's (its KLAS "E7A" is MSFS's E7, its KSAN "106" is MSFS's Parking 70).
  SayIntentions' gate is matched to an MSFS spot by its position (within 40 m), and it counts as its arrival pick
  only once that position is within 5 km of the destination (before departure flightJSON's `assigned_gate` may be
  the departure gate).
- **The read-back decides.** `assignGate` can answer without an error and change nothing (KLAS, milestone 9), so the
  app reads SayIntentions' parking back with `getParking`: the request took only if it moved away from SayIntentions'
  own gate. Live names the gate ATC will give. If `assignGate` answers "Unknown gate" or fails, SayIntentions keeps
  its own gate and Live says so.
- **One try per flight**, logged as an `arrival_gate` event and shown on Live (section 13.5). Built in milestone 9
  (`gate_picker.h`, `gate_assigner.h`, `FlightWatcher`).
- BlockTime can't see whether an AI aircraft is parked there; SayIntentions and GSX handle that on the ground.
  The summary records the spot where Dave actually parked: the nearest spot to the block-in position, up to 40 m
  away, because the sim's spots don't always line up with the gate textures (D-122). It's stored on the flight
  (`arrival_parking`) and shown in the summary and on Live ("Parked at Gate E7"), and it's the next flight's load-in
  spot (section 13.3).

### 13.7 After the flight

- **Milestone 12 (D-154):** at block in the dashboard header stays at the top of Live, filled in (block in time,
  final block time, the whole arc coloured, Arrival in progress), with "Parked at …" and the shutdown checklist under
  its status lines; the flight summary appears below it in place of the planned flight.
- At block in, the app switches by itself to the **flight summary**: score and Claude's evaluation (section 14),
  each income and expense line, and the total gained or lost. Plus the route map. **Built in milestone 6** (the flight
  number and route, the airplane, block out and in, block time, fuel used, the money lines with their total, and the
  track) and **milestone 10** (the score and landing stars in its header, and a Debrief view with the checks, the
  landing and Claude's review, `docs/debrief.md`). A flight settled at startup (BlockTime was closed or
  crashed at block in) shows its summary the same way.
- **Flights (D-111)** lists the settled flights from the database, newest first, and shows the same summary for the
  one selected. The track comes from the event log; once the log is gone (30 days), the map shows a straight line.
- Two buttons:
  - **At Gate Turnaround:** back to New Flight with the same aircraft at its new airport and gate.
  - **Done for now**: the aircraft stays parked; the app remembers the gate for the next
    load-in.

### 13.8 Interrupted flights (D-072)

A flight that ends without block in (Dave exits to the main menu, the sim crashes, the app is killed) is **deleted
as if it never happened.** No resume, no completing it in the background. The aircraft stays where it was; no money,
fuel, hours or leg-template data. GSX services from that attempt go too. The app shows a short notice (for example
"KSFO–KSEA interrupted and discarded") so a missing flight never looks like a bug. The recorder still logs
`flight_interrupted` as a fact.

**The planned flight stays (D-108):** same loads, same SimBrief plan. Live goes back to "Waiting for sim", so Dave
can load in and fly it again, or cancel it.

## 14. Flight recording (summary)

Full detail in `recorder.md`.

- Flights start at parking brake release at the gate and end when it's set at the destination.
- **Interrupted flight** (sim crash, main menu, app killed): discarded as if it never happened (section 13.8,
  D-072).
- **Diversion:** the flight closes where the plane actually parked (the imported airport nearest the block-in
  position, D-113); revenue counts, and every line but crew, maintenance and fuel is as previewed (D-109); a rotation
  plane is flagged out of position until repositioned.
- Phases are detected by the recorder (plan-aware, forward-only) and displayed in the app.

### Flight analysis (D-040, D-156 to D-162)

**Built in milestone 10; the full design is `docs/debrief.md`.** BlockTime's own code measures fourteen procedure checks
from the event log when the flight settles (beacon before pushback, seat belt sign, taxi speed, both altimeters, lights
for takeoff, 250 kt below 10,000 ft, STD by the transition altitude, landing lights below 10,000 ft, stable at 1,000 ft,
strobes after landing, the APU off by the transition altitude and switched on by engine shutdown at the gate, D-172, D-173) and the landing (sink rate, G, bounces, pitch, bank, speed against VREF, wind). It stores the
measurements; the **0–100 score** and the **0–5 landing stars** are computed from them by fixed rules whenever they're
shown, so the rules can be tuned and every flight re-scored at no cost. The Flights screen shows the running averages.

Claude (Opus 5.5, through the Claude API) gets the checks, a timeline of the flight, the SayIntentions ATC log and the
ATIS for both airports, and writes the review: what went well, what to work on, the landing explained, and ATC
compliance as information only (SayIntentions is sometimes wrong). Claude never sets the score (D-156). The score never
touches the money or anything else in the airline (D-157).

**Never scored (D-122):** how far a takeoff or landing was from the runway centreline or heading, or from the
touchdown zone. The sim's runway positions often don't match the painted runway, so an autoland the sim calls perfect
can look off and a landing on the painted centreline can read off. These figures may appear in the logs and the
summary as information only.



## 15. Reference data

- **Airports:** OurAirports public dataset (coordinates, runways), imported once and filtered (D-079). Time zones
  from an airport dataset joined on the ICAO code (D-084). The schema is in `database.md`.
- **Distance for fares:** great-circle distance between airports (not the SimBrief route distance).

## 16. Later

- **SayIntentions integration:** ATIS comes from `getWX` (section 13.2) and the arrival gate from `assignGate`
  (section 13.6). Still to find out: how to get the ATC log for the flight analysis (section 14, D-040).
- **Settings screen** (D-092) for the airline, fleets and leasing airplanes, and for **managing the catalog's
  aircraft types and airframes** in the app (config files until then, D-076). The catalog then moves from YAML
  into the database; airplanes already leased keep the values copied when they were leased (D-082).
- **Pre-departure view on Live (D-118).** Before block out, Live shows everything Dave needs to set up the FMC
  and get ready for pushback, so BlockTime is the only thing open beside the sim. It changes to the in-flight view
  at block out. Sources: the SimBrief plan (route, weights, fuel, cost index, cruise altitude and the rest of perf
  init) and SayIntentions (runway in use, ATIS, clearance). Takeoff performance comes from the PMDG EFB, as landing
  performance already does; BlockTime shows the EFB's inputs (weight, runway, wind, temperature, QNH) and calculates
  nothing. The plan's weather is old by the time Dave is at the gate, so the ATIS is polled, around every 5 minutes.
  Charts stay in Navigraph. Its name (Dispatch Plan, Flight Plan, Pre-Departure Brief, Load Sheet?) and exactly what
  it shows are for a brainstorm when it is built.
  Designed on 2026-11-24 as the dashboard's Pre-Flight panel: `dashboard.md` (D-153).
- **Predictions on Live (D-120).** Time and distance to top of climb and top of descent, landing and gate arrival,
  from the SimBrief navlog (waypoints with planned times, TOC and TOD) and the recorder's position and groundspeed.
  Each waypoint passed compares actual with planned time; the remaining times shift by that ahead/behind figure.
  Taxi-in comes from Dave's past flights on the leg (leg templates) rather than SimBrief's. A TOD prediction could
  also time the arrival gate assignment (section 13.6). Vectors, holds and route changes throw it off until the
  flight is back on the plan. Whether the PMDG's own FMC predictions can be read is an open item (section 17).
- Optional flavour ideas: special cargo jobs with supernumeraries on the 777F; display-only "published schedule"
  labels.

### Flight Board and Journeys (D-144)

**Flight Board.** One screen styled like an old split-flap airport board, replacing section 8's two boards. It lists
every background flight in the air and every plane at a gate in a turnaround before its next leg, plus the next leg
of each journey. With no background flights and no journeys it's greyed out. Rows flip only when they change, and
not at all while a flight is being recorded (no continuous animations).

Clicking a flight takes it over (section 7, "Taking over legs"):
- **Parked:** fly its next leg.
- **In the air, more than 40 nm from its destination:** **reset** (it goes back to its origin, the leg earns nothing,
  and Dave flies that leg) or **claim** (it lands, then waits at the gate for Dave to fly its next leg).
- **Within 40 nm:** claim only.
BlockTime knows the flight belongs to a rotation: after an At Gate Turnaround it queues the rotation's next leg for
Dave. "Done for now" hands the plane back to its rotation from where it's parked.

**Journeys.** A one-way sequence of legs that only Dave flies (an around-the-world trip in a BBJ2, say), with an end.
Unlike a rotation it isn't flown in the background, so the unlock rule (D-075) and the curfew don't apply. Its legs
are real flying: they count toward leg templates and rotation unlocks like any other completed flight.
- **Starting:** attach an inactive airplane parked at the journey's first origin. The airplane is then **On journey**
  and locked to it: no rotation, no free flights. The journey's next leg shows on the Flight Board. There can be as
  many journeys running as Dave has airplanes to put on them.
- **Pausing:** the journey keeps its place (next leg, at the airport where it stopped). The airplane leaves it and
  goes back to Inactive where it's parked, free to fly or to join a rotation. A paused journey resumes when **any**
  airplane parked at that airport is attached, of any type: a 737-800 across North America, a 777-300ER for the
  Atlantic, another 737 from Alaska. Getting the right airplane there (a delivery or a free flight) is part of the
  fun. "Journey paused" isn't "airplane paused" (section 7's state).
- **Planning form:** columns Origin, Destination, Direct distance (NM); five rows to start and an "Add more legs..."
  link that adds one. Each row's origin is the row above's destination, so only the first origin and then one code
  per leg are typed. The distance is the great-circle distance from the airport import, shown as soon as both
  codes are known, with each airport's name to catch typos. Rows can be removed. A total (legs, distance) sits
  underneath. Once an airplane is attached, legs beyond its range or with a runway shorter than its airframe's
  minimum (D-100) get a warning, not a block.
- **Journey page:** a map of the whole journey. Flown legs show the actual track, coloured like a flight summary;
  legs still to fly show a dotted great-circle line to each next destination. When the journey ends, the map is the
  record of the trip, to view, screenshot or share.

## 17. Open items

- Exact seats per class, hold maximums and hold ratios per configuration (collect from PMDG when needed).
- GSX state values, PMDG switch readability, and SayIntentions gate names.
  - The first probe flight (2026-11-23, `docs/notes/probe-flight.md`) confirmed the GSX states except deboarding,
    and every listed PMDG 737 switch.
  - It also confirmed the sim's parking names: Gate D + 14 is SayIntentions' "D14" (maybe "D 14").
  - Still open: the 777 and GSX deboarding.
- Whether the PMDG SDK (or the sim's GPS flight-plan SimVars) exposes the FMC's predictions (ETAs, distance to TOD)
  for the 737 and 777: check in a probe flight before building predictions (D-120). Both SDK headers declare the
  distance to top of descent and to destination; the probe logs them from 2026-11-23.
