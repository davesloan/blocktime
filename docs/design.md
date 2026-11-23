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
- It shows **where to load in:** the airport, and the gate where the aircraft was last parked, if known (section
  13.7). The app can't make MSFS spawn there; Dave picks it on the world map.
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
| Pre-Departure | At gate | Catering, Fuel loading, Boarding (GSX) |
| Departure | Pushback, Taxi out, Takeoff | Pushback (GSX), Taxiing to runway, Takeoff roll, Takeoff |
| Climb | Climb | Departed runway 30, Level-offs, Above 10,000 ft, Passed transition altitude |
| Cruise | Cruise | Cruising at FL350, Step climb to FL370, 200 nm from destination |
| Descent | Descent, Approach, Landing, Taxi in | Top of descent, Passed transition level, Level-offs, Below 10,000 ft, Final approach, Landed, Taxiing to gate |
| Arrival | Arrived | Parked at gate, Deboarding (GSX), Ready for shutdown or turnaround |

- **Departure runway:** from the takeoff position and heading, matched against the sim's runways.
- **Transition altitude and level, and "200 nm from destination":** from the SimBrief plan.
- **GSX services (D-073):** each service shows as its own line with start and end. Services Dave used appear;
  ones he skipped are left out. If **no** pre-departure service was completed before pushback, a single
  **"Turnaround and Boarding"** line appears just before "Pushback initiated". "Cleaning" isn't a GSX service, so
  it isn't shown.
- **Ready for Shutdown or Turnaround:** shown when GSX deboarding completes. If deboarding wasn't done through GSX,
  it shows once the checks below pass. Under it, an informational checklist (never a requirement or penalty):
  engines off, parking brake set, deboarding complete, beacon, strobe and seatbelt signs off, APU off. Which of
  these PMDG exposes is found in the procedure-capture probe flight; items that can't be read are dropped.

### 13.6 Arrival gate (D-074)

Mid-flight (around top of descent or 200 nm out), the app picks an arrival gate and asks SayIntentions to assign it
(`assignGate`, needs an active SayIntentions flight), so ATC gives it after landing.
1. **Preferred gates** for the airport, from config, first one that fits.
2. Otherwise the parking spot of the **right size and type closest to the arrival runway** (from a fresh ATIS,
   falling back to the plan's runway). Size: the spot's radius fits the family's wingspan (in config).
3. **Type:** passenger flights use gates. **Freighters and charters use ramps** (cargo ramps for freighters).
   If no ramp of the right size exists, any ramp that fits, then a gate as a last resort,
   with a note on the dashboard.
- Parking spots (name, position, size, type) come from the sim over SimConnect. Gate names are sent to
  SayIntentions in its format (alphanumeric, for example "B23"); the exact mapping is checked in a probe flight.
- BlockTime can't see whether an AI aircraft is parked there; SayIntentions and GSX handle that on the ground.
  The summary records the spot where Dave actually parked, matched from the block-in position.

### 13.7 After the flight

- At block in, the app switches by itself to the **flight summary**: score and Claude's evaluation (section 14),
  each income and expense line, and the total gained or lost. Plus the route map.
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

### Flight analysis (D-040)

At the end of each of Dave's flights, the app sends Claude, through the Claude API:
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
- Optional flavour ideas: special cargo jobs with supernumeraries on the 777F; display-only "published schedule"
  labels.

## 17. Open items

- Exact seats per class, hold maximums and hold ratios per configuration (collect from PMDG when needed).
- GSX state values, PMDG switch readability, and SayIntentions gate names: confirm in probe flights before building
  on them.
