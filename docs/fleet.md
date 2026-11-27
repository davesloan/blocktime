# Fleet: slots, adding aircraft from SimBrief, returning them (milestone 15b)

Designed with Dave in the brainstorm of 2026-11-27, before milestone 16. The look is fixed by the mockups in
[mockups/fleet/](mockups/fleet/): open the HTML files in a browser. Their numbers are made up.

- `1-slot-layouts.html`: option **B** (character sheet) was chosen; A and C were dropped.
- `2-fleet-cards.html`: the airplane cards, revised (the type and layout first, a coloured border for rotation and
  journey planes).
- `3-aircraft-screen.html`: option **B** (character sheet again), plus what New screenshot and Return open.
- `4-add-aircraft.html`: the Add Aircraft screen after a screenshot has been read.

## Intent

The MVP is done, and adding an airplane still means hand-editing `airline.yaml`: typing the registration, the
SimBrief airframe ID and the limits, and restarting the app. Dave wants the fleet to be part of the game:

- **Slots you can see.** Fleet looks like an RPG's armour slots: the airline is the character, its airplanes are the
  gear, and locked slots show the hours that open them.
- **Adding an airplane from SimBrief.** Dave makes a SimBrief airframe for each airplane anyway (D-087). He drops a
  screenshot of its edit page into the app, Claude reads the values, and the app checks them and leases the plane.
  SimBrief stays the source of truth for an airplane's weights and limits (CLAUDE.md).
- **Picking a plane for a flight.** Fleet becomes the place a flight starts: each card says what the plane is, where
  it is and whether it can fly now, with a Fly button.

### Goals

- Fleet in the sidebar opens the slots screen; the table goes.
- Slots: 2 at the start, 1 more for every 10 h of Dave's block time, at most 20.
- Add Aircraft: screenshot → Claude → checks → lease, with factory delivery or instant delivery.
- An aircraft screen per airplane: all its values, New screenshot, Return airplane.
- Supported aircraft come from data files shipped with the app, so a new type is a new file.

### Non-goals

- Rotations and journeys. The cards are designed for them (Fly This Leg, Continue Journey, the coloured border), but
  only the states that exist today are built.
- Making the recorder read SimVars from the aircraft files. The file format leaves room for that (the first
  non-PMDG aircraft, after probe flights), but nothing reads it yet. D-039 (PMDG only) stands.
- The rest of the settings screen (D-092): the airline's name, code and callsign stay in `airline.yaml`.
- The money item (D-216 to D-221, milestone 18). The lease stays monthly; the refund and instant delivery use
  today's rates.

### Anti-goals

- A value misread from a screenshot getting into the airline unseen: every value is shown for checking before Lease
  or Save, and a re-read shows only what changes.
- An airplane whose BlockTime values differ from its SimBrief airframe (seats, weights, holds): the loads and the
  plan would disagree.
- Money that can't be explained: instant delivery and the refund show their workings, like every other money line
  (D-238).

### Constraints

Everything in `CLAUDE.md`: QML only displays (the slot rule, the checks, the hold split, the refund and the delivery
price are plain C++ with tests), the dark theme in `Theme.qml`, the database rules (D-078, D-090), money in cents and
lines in $5 (D-080), atomic changes, history never updated. Network calls get real error handling; the config files
get the happy path only (D-099).

## Approaches considered

| Approach | Trade-off | |
|---|---|---|
| **A. One milestone, built from the data up** | One plan and one session (D-121); the pieces depend on each other, and Dave tests once at the end | **Chosen** |
| B. Two milestones: the screens first, then adding planes | Dave sees the screens sooner, but planes still come from `airline.yaml` in between, an empty slot leads nowhere, and the schema changes twice | |
| C. Add Aircraft first, behind today's table | Gets rid of YAML editing soonest, but builds an Add button on a screen that's about to be deleted | |

It's called **milestone 15b** rather than renumbering: the docs and many decisions already call dispatch milestone 16.

Reading the screenshot: Claude through the Messages API, as Dave asked. Copying the SimBrief page's text was tried and
gives the labels without the values (the form's inputs aren't copied). SimBrief has no airframe API (D-100).

## Terms

- **Aircraft** and **airplane** mean the same: one leased plane (D-091).
- **Airframe** stays the catalog's term for a type in one layout ("737-800 Two-class"); in the files below it's a
  **layout**. A **SimBrief airframe** is SimBrief's saved aircraft profile, one per airplane (D-087).

## Slots

- **2 to start, +1 for every 10 h of block time, at most 20.** Slot 3 at 10 h, slot 4 at 20 h … slot 20 at 180 h.
  Hours are the sum of Dave's flights' block times, as now (D-114); background hours won't count. Replaces D-131's
  growing steps. `slotsEarned` and `hoursForSlot` in `leasing.h` change; nothing else counts slots.
- Slots cap the airplanes in the fleet (not returned). Returning one frees its slot.
- **Slots aren't numbered or stored** (Claude's call). The cards are the airplanes in lease order, then the empty
  slots, then the locked ones. The aircraft screen's title is the registration.

## The Fleet screen

Fleet in the sidebar opens it; `FleetPage`'s table and `FleetTableModel` go. Layout as `2-fleet-cards.html` on the
shape of `1-slot-layouts.html` B:

**Left: the airline.** Logo, name, ICAO code and callsign; slots open as "4 / 20"; a bar to the next slot with
"23.4 / 30 h"; hours flown; cash; leases per month (the sum of the fleet's monthly rates). The airline's notices
(a missing config file, no airport data, an unpaid lease) stay on this screen, one line each, above the cards.

**Right: the slots.**

- **A card per airplane**, in lease order. Top line: the type and layout ("737-800 Two-class"); under it the
  registration (Dave picks by type, not registration). Then where it is and its fuel on board, and its current leg
  when it has one (the planned flight today; a rotation leg or journey leg later). Two buttons: the main one and
  **Manage** (the aircraft screen).
- **The glyph's colour** says whether Dave can fly it: green ready, amber awaiting delivery, grey in the air
  (background flying, later). The plane with Dave's planned or in-progress flight uses the accent blue (Claude's
  call). A plane in the air also gets a dimmed card and an "In the air" button, greyed.
- **Rotation and journey planes** (later) get a border in the rotation's teal or the journey's violet, and a corner
  ribbon saying which.
- **The main button**, first that applies:
  1. **Continue Flight**: this plane has the planned or in-progress flight. Opens Live (Your Flight from milestone 16).
  2. **Fly Delivery**: awaiting delivery. Opens New Flight with this plane (its delivery flight).
  3. **Continue Journey** (later): its journey's next leg.
  4. **Fly This Leg** (later): on a rotation and parked; the rotation's next leg (design.md §7, taking over legs).
  5. **Fly**: anything else. Opens New Flight with this plane, as New Flight does from the table today.

  While one plane has the planned or in-progress flight, every other card's main button is greyed: one flight at a
  time, as today. Milestone 16 points Fly and Fly Delivery at the dispatch form instead of New Flight.
- **Empty slots**: a dashed card, "+ Add aircraft". Opens Add Aircraft.
- **Locked slots**: a compact row of small tiles with the hours each opens at ("30 h"), the next one in amber; after
  seven tiles, "+N more".

New colours (teal, violet, the accent glyph) go in `Theme.qml`.

## The aircraft screen

Opened by Manage. Layout as `3-aircraft-screen.html` B.

**Left: the airplane.** The glyph large, the registration, tail number and flight number, type and layout, its status
("Ready at KSAN"), a bar through the current lease month ("5 of 30 days"), and two buttons: **New screenshot** and
**Return airplane**. A back button returns to Fleet.

**Right: four cards**, each saying where its values come from:

- **Layout** (catalog): the layout's name, seats per class, the holds' total and each hold's maximum, drawn as a bar.
- **Weights** (SimBrief, with the date read): empty weight, MZFW, MTOW, MLW, max fuel, passenger and bag weight.
- **SimBrief**: Internal ID, base type, min runway, max payload (MZFW − empty weight).
- **Record & lease**: fuel on board, flights and hours, the monthly rate, the next charge, and the refund if returned
  today.

An airplane leased before 15b has no screenshot yet: its Weights and SimBrief cards show what the database has and
say "No screenshot yet", and New screenshot fills them.

### New screenshot

A drop area (drag a PNG or JPEG in, or Ctrl+V). Claude reads it as in Add Aircraft and the same checks run, with
these differences:

- The type can't change. A screenshot of another type is refused: return the plane and lease a new one.
- The registration can't change: a screenshot with another registration is refused (Claude's call). Flights, the
  ledger and the debrief show `airplane.registration`, so a rename would relabel every past flight; a plane with a new
  registration is a new lease. The Internal ID may change (Dave rebuilt the SimBrief airframe), but not to one another
  airplane in the fleet uses.
- If Max Passengers changed, Dave picks another layout of the same type whose seats match; the cabins change with it.
- Only the changes are shown, old → new (the hold split recomputed), with Cancel and **Save changes**. Nothing is
  written until Save.
- Greyed while the plane has a flight planned or in progress. Settled flights keep their own figures.

### Return airplane

A confirmation: "Return N814SX? The lease ends today and $125,000 comes back for the unused 25 days. Its slot becomes
empty. Its flights stay in your history. This can't be undone." Buttons **Keep it** and **Return N814SX**.

- **The refund** is the current lease month's charge × unused days ÷ the month's days, rounded to $5 (D-080). Unused
  days count from tomorrow to the day before the next month starts; returning on the month's first day refunds
  all but that day. (Claude's call on the day count; design.md §5's "prorated refund for the unused part of the
  month".)
- In one transaction: `returned_on` today, reason `returned` (a new `return_reason` row), and the refund as a ledger
  line (a new category, with a fact row pointing at the airplane, like `lease_charge`). No later months are charged.
- Greyed while the plane has a flight planned or in progress. Allowed while it awaits delivery.

## Add Aircraft

Opened by an empty slot. Layout as `4-add-aircraft.html`. Before a screenshot: a large drop area (drag, Ctrl+V or a
file picker). After Claude has read it, five parts:

1. **From SimBrief**: registration (Civil Registration), tail number (Fin Number; empty means none), Internal ID,
   base type, max passengers, passenger weight, bag weight, empty weight, MZFW, MTOW, MLW, max fuel, max cargo, min
   runway. Each in an editable field, so Dave can fix a misread; the card says the values must match SimBrief.
2. **Type and layout**: the checks with ✓ or ✗, then the layouts to pick from (below), then the hold split.
3. **Yours**: the flight number for free flights (optional, 1–4 digits, unique: D-106).
4. **Delivery**: fly it from the factory, or instant delivery to an airport Dave types.
5. **Lease**: the first month charged today, instant delivery if chosen, and the cash after.

**Lease** is greyed, with the reason shown, while any of these holds:

- No supported type accepts the base type.
- No layout of those types has exactly SimBrief's Max Passengers.
- The registration or Internal ID belongs to an airplane in the fleet.
- No slot is open (can't happen from an empty slot, but checked in the core).
- Instant delivery's airport isn't in the airport data, or its longest runway is shorter than the min runway.
- Cash is less than the first month plus instant delivery.

Pressing Lease writes everything in one transaction (below) and returns to Fleet, where the new card is.

### Matching the type and layout

- Each aircraft file lists the SimBrief base types it accepts. The candidate layouts are every layout of every type
  that accepts the screenshot's base type.
- **Max Passengers must equal a layout's total seats exactly** (Dave). That one is pre-selected; layouts with other
  seat counts are shown greyed with "184 seats; SimBrief says 160". If several match, the first in file order is
  pre-selected and Dave can change it.
- This also tells variants apart if SimBrief gives several the same base type (the BCF's 0 seats). N814SX's SimBrief
  airframe shows Base Type BBJ2 (ICAO code B738), so the BBJ2 file accepts BBJ2; the others are estimates until Dave's
  screenshots confirm them.
- A freighter layout has 0 seats and matches Max Passengers 0.

### The holds

SimBrief's **Max Cargo Weight is the holds' total** for bags plus belly cargo: SimBrief always puts the passengers'
bags in the cargo load, and belly cargo comes on top, up to that maximum (Dave). The layout gives each hold's share;
SimBrief's total is split in those proportions. A layout of 7,500 forward and 9,500 aft with SimBrief's 18,000 gives
7,941 and 10,059: each hold's share rounded down to whole pounds, the remainder to the last hold (Claude's call). The
layout's loading ratio (4:5) is kept as it is: it splits a flight's load between the holds (design.md §11), which is
separate from each hold's maximum.

### Delivery

- **Fly it from the factory**: as today. The airplane starts at its family's factory (737s at Boeing Field KBFI, 777s
  at Paine Field KPAE, design.md §6), awaiting delivery; its first flight is the empty delivery flight.
- **Instant delivery to an airport**: the estimate New Flight gives today for a delivery flight from the factory to
  that airport (`pricePreview` with kind `delivery`: pilots only, no cabin crew; maintenance; ATC; landing), with
  its fuel replaced by a **Delivery fee** (Dave). New Flight can't estimate fuel before a SimBrief plan exists, so
  the fee stands in for it: the great-circle distance × the type's `delivery_fee_per_nm`, rounded to $5 (D-080),
  roughly what the fuel would cost. To start, $8 per nm for the 737s and $18 for the 777s (Claude's estimates: about
  11 and 25 lb per nm at $0.70 per lb; Dave tunes them in the files). Every line shows its working
  ("Delivery fee: 943 nm × $8"). Charged at Lease and posted as those lines (pilots, maintenance, ATC, landing in
  their existing ledger categories, the fee in a new one), all pointing at one instant delivery fact row with the
  airplane, the factory and the airport (the ledger shows "N738SX instant delivery KBFI–KSAN · Delivery fee").
  The airplane starts at that airport, not awaiting delivery, with **0 lb fuel on record**, so its first flight pays
  all its own fuel (Claude's call). Instant delivery earns no hours. If the airport is the factory, there's nothing
  to buy: the option says so and the plane starts there, delivered.

### The lease transaction

One transaction: insert the airplane (registration, tail, type, layout name, SimBrief ID, leased today, the type's
monthly rate, location, fuel, awaiting delivery, flight number), its cabins from the layout, its holds from the split,
its limits and SimBrief values from the screenshot; charge the first month (as `leasing.cpp` does now); post instant
delivery if chosen. If any check fails inside the transaction (cash, slots, a duplicate registration or Internal ID),
nothing is written and the form shows why.

## Reading the screenshot

- **The call**: the Messages API with the image (base64 PNG or JPEG), through the same HTTP code and the same
  `claude_api_key` as the debrief (`claude_reviewer.cpp`, D-156). Model `claude-opus-5-5` (Claude's call: a few cents a
  read, done rarely, and a misread costs more). The answer is a fixed JSON shape (structured output), one field per
  value in "From SimBrief" plus the base type, each a string or number, or null when the field isn't on the image.
- **The prompt** names each field by its label on SimBrief's page (Civil Registration, Fin Number, Internal ID, Base
  Type, Max Passengers, Passenger Weight, Baggage Weight, Empty Weight, Max Zero Fuel Weight, Max Takeoff Weight, Max
  Landing Weight, Max Fuel Capacity, Max Cargo Weight, Min Runway) and says the weights are in pounds. If the page's
  Weight Units isn't LBS, the read is refused with "Set SimBrief's weight units to LBS" (Claude's call; Dave's are).
- **Failures**: no key, a network error, an HTTP error or an answer that doesn't parse shows a one-line reason and a
  Retry button. The fields stay editable, so Dave can type the values in by hand and lease anyway. A null field is
  left empty and marked.
- **Privacy**: the screenshot goes only to Anthropic's API with Dave's own key. The key never reaches a log
  (`app_settings.h`'s secret scrubbing). The image isn't kept.
- **A command-line tool** (`blocktime-airframe <image>`, next to `blocktime-debrief`) prints the JSON Claude returns,
  so the reading can be checked against Dave's real screenshots without the app. Tests use a saved response, never the
  network.

## Supported aircraft files

The catalog leaves `%LOCALAPPDATA%\BlockTime\catalog.yaml` and ships with the app as one YAML file per type in
`data/aircraft/` (copied next to `BlockTime.exe` by the build, like the QML). The old `catalog.yaml` is no longer read
and is left where it is.

Each file holds what `catalog.yaml` has today for its type, minus the stand-in limits:

```yaml
# data/aircraft/pmdg-737-800.yaml
code: 737-800
family: "737"
role: airliner            # airliner | executive | freighter
turnaround_min: 35
cruise_speed_kt: 450
lease_per_month: 235000   # whole dollars, multiple of $5
sim_folder: PMDG 737-800  # the matching check (D-112)
wingspan_m: 35.8          # the arrival gate's size (D-147)
simbrief_base_types: [B738]
factory: KBFI             # where a factory delivery starts (design.md §6)
delivery_fee_per_nm: 8    # instant delivery's stand-in for fuel: whole dollars per great-circle nm
layouts:
  - name: 737-800 Two-class
    cabins: { business: 16, economy: 144 }
    holds:                                 # each hold's share of SimBrief's Max Cargo, and the loading ratio
      - { hold: forward, share_lb: 7500, ratio: 4 }
      - { hold: aft, share_lb: 9500, ratio: 5 }
    fuel_burn_lb_per_nm: 9.5               # optional (D-119)
  - name: 737-800 All-economy
    cabins: { economy: 184 }
    holds:
      - { hold: forward, share_lb: 7500, ratio: 4 }
      - { hold: aft, share_lb: 9500, ratio: 5 }
# recorder: later, the SimVars this aircraft is read with (after probe flights); not read yet
```

- At each start every file's type is written to `aircraft_type`, as the catalog's types are now (D-093).
- A file that doesn't parse is skipped with a notice (happy path otherwise, D-099).
- The seven types and their layouts in today's `catalog.yaml` become seven files.

## `airline.yaml` and the database

- **`airline.yaml`** keeps `airline` (name, ICAO, callsign), `founded_on`, `starting_cash` and `preferred_gates`. Its
  `fleets` and `airplanes` lists are no longer read (no warning, D-099). `config/airline.example.yaml` loses them.
- **Fleets are dropped** (Dave: rotations will group planes). The `fleet` table and `airplane.fleet_id` go.
- **Each airplane's SimBrief values**: the limits it already has (`airplane_limits`: min runway, MLW, passenger and
  bag weight, max payload) stay what the app reads, and the screenshot's other values (empty weight, MZFW, MTOW, max
  fuel, max cargo, max passengers, base type, the date read) are stored per airplane in a new table with one row per
  airplane. No row means no screenshot yet (the two airplanes leased before 15b), so no column has to be NULL for
  "unknown". A re-read updates both (current state, like location and fuel).
- **New ledger categories**: instant delivery and lease refund, each with a fact table pointing at its airplane
  (`UNIQUE` on the airplane: one of each per lease), like `lease_charge`.
- **A new `return_reason`** row: `returned`.
- **Re-leasing is allowed** (Claude's call). Registration, SimBrief Internal ID and flight number are unique among
  airplanes in the fleet, not across every row ever leased: partial unique indexes `WHERE returned_on IS NULL` replace
  today's `UNIQUE` constraints. So Dave can return N814SX and later lease it again with the same SimBrief airframe, as
  a new airplane row (one row is one lease, as now). Its old flights and lease lines stay with the old row and show
  the same registration, which is true. `database.md`'s "leasing the same registration again isn't supported" goes.
- **One migration** (11), rebuilding `airplane` without `fleet_id` (migrations can rebuild a table since review 1.5).
  It's run on a copy of Dave's airline before release, with the backup and checks as for migration 10.
- `docs/database.md` gets the new tables and the entity diagram without `fleet`.

## Testing

Careful tests where money or history is involved (D-174):

- The slot rule: 2 at 0 h, 3 at 10 h, 20 at 180 h and beyond.
- Layout matching and the hold split (the remainder, three holds, a freighter).
- The lease transaction: everything written or nothing; each refusal (cash, slots, duplicates).
- The refund's day count, rounding, and its transaction; no month charged after a return.
- Instant delivery: its lines equal New Flight's delivery preview (pilots only) plus the fee, distance × rate rounded
  to $5; the airplane's location, fuel and delivery state.
- Migration 11 from an empty database and on a copy of Dave's.
- Parsing Claude's answer: a saved good response, a missing field, junk.

The screens are checked with `--demo` and the debug build's hot reload. `blocktime-airframe` is run on Dave's real
screenshots. No sim test is needed: nothing here talks to the sim.

## Documents to update when it's built

- `docs/design.md` §3 (airframes as shipped files, airplanes from SimBrief), §5 (return with refund), §6 (the slot
  rule and table, Add Aircraft, instant delivery, no fleets).
- `docs/roadmap.md`: milestone 15b before 16; instant delivery and the fleet part of the settings screen leave the
  after-MVP list.
- `docs/dispatch.md` step 1: Fleet → an airplane's Fly button.
- `docs/database.md`, `README.md` (no airplanes in `airline.yaml`; the data folder), `config/airline.example.yaml`.

## Decisions to record

D-239 onwards, all Dave's unless marked:

- The slot rule: 2, +1 per 10 h, at most 20 (supersedes D-131's steps).
- Fleet becomes the slots screen with Fly and Manage; the table goes (supersedes D-097's table).
- Airplanes are added in the app from a SimBrief screenshot read by Claude (supersedes D-076's airplanes in
  `airline.yaml`, D-087's typed SimBrief ID, D-100's catalog defaults and per-airplane overrides).
- Seats must match a layout exactly; holds take SimBrief's total in the layout's shares.
- Supported aircraft are shipped data files, one per type (supersedes `catalog.yaml` in `%LOCALAPPDATA%`; part of
  D-092 done).
- Fleets dropped.
- Instant delivery built now: the delivery flight's estimate with a Delivery fee per nm in place of fuel; return with a
  prorated refund built now.
- Milestone 15b, not a renumbering (Dave + Claude).
- Claude's calls: slots not stored; the accent glyph for Dave's current plane; the hold remainder; the refund's day
  count; the delivery fee rates ($8 and $18 per nm); 0 fuel after instant delivery; Opus 5.5 for the read; LBS only; the SimBrief values table; re-leasing
  allowed (uniqueness among the fleet); no registration change on a re-read.
