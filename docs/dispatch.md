# Dispatch, the flight summary and the debrief (after the MVP)

Designed with Dave in the brainstorm of 2026-11-27 (roadmap, "Next, after the MVP", item 2). Decisions D-214, D-215,
D-222, D-223, D-233, D-234 and D-235; the log at Accept, D-236 and D-237. The look is fixed by the mockups on the canvas
**https://claude.ai/artifact/N81DAojKWXThKyjb9itke7**: boards `Final` (the dispatch report, before and after import),
`Summary` and `Debrief`. Their numbers are made up; the other boards on the canvas are the directions tried and
dropped. Open the boards with Play to see the flip, the hover and the clicks.

## Intent

The planned flight panel was the last part of Live still in a debug state: setup buttons, the route, the plan and a
plain money list on one card, under the dashboard. The flight summary was a report, not a payoff. Dave wants:

- Setting a flight up kept apart from flying it: only an accepted flight puts the dashboard on screen.
- To see **how each money line was worked out** before he commits, and to accept (or cancel) on that.
- Screens with a "cool factor": BlockTime is a game on top of a game, and "no one wants to play an ugly, boring
  game". Not paper documents (he dislikes SimBrief's) and not FlyHub's look.

### Goals

- One place for the whole flight, from the form to the summary: **Your Flight** (Live, renamed).
- A dispatch report that is honest (what settles is what was shown, D-222, D-223) and explains every number.
- A summary and debrief that feel like the end of a level: score, landing, what the flight moved forward, the money.
- The same parts everywhere: the ring, the split-flap board, the dashboard's six-stage bar and glass lamps.

### Non-goals

- The money item (realistic fares, the per-hour lease, the flights-per-month setting: D-216 to D-221). Dispatch shows
  whatever the rates give; the money item changes the rates later.
- The Flight Board (after rotations). Create New Flight starts from Fleet until then.
- A paper-style dispatch release or OFP view.

### Anti-goals

- A number on the dispatch report that settles differently without a reason shown.
- A plan whose loads differ from the rolled ones getting through (GSX would board SimBrief's numbers).
- Continuous animation: the flip and the ring's hover are one-time or user-driven (the CPU budget).

### Constraints

Everything in `CLAUDE.md`: QML only displays (the workings, deltas and checks come from plain C++ with tests), the
dark theme in `Theme.qml`, the database rules, money in cents and lines in $5. D-222's locked rates and accepted
breakdown; D-223's fuel line. The review's groundwork (`docs/reviews/2026-11-27-technical-review.md`, fix step 3: one
`PriceInput` builder, lines with their workings, the accepted state, the `PlannedFlightModel` split) comes first or
at the start.

## Approaches considered (D-233)

| Approach | Trade-off | |
|---|---|---|
| A. New Flight grows into a three-step window | Closest to today; too small for the report, and sits over the app while Dave is in SimBrief | |
| B. A Dispatch page in the sidebar | Room for everything; Dave: "starting to feel like I'm copying FlyHub" | |
| **C. Your Flight shows dispatch until Accept, then the dashboard** | Never both at once; full-page room; no new screen; the same parts as the dashboard | **Chosen** |

## The flow (D-233)

1. **Start.** Fleet → an airplane → **Create New Flight** opens Your Flight with the form pre-filled (later the
   Flight Board too). Opening Your Flight directly with no flight shows the empty form.
2. **The form** (Dave's design; the look is the builder's, in the dispatch report's style):
   - Airplane, destination (with today's runway length check, D-100), flight number (D-106).
   - Loads: **fully loaded** (D-215: every seat and belly cargo up to max payload, free flights only) or random.
     Today's options stay: the BBJ2's VIP airliner / charter, belly cargo on random loads, delivery flights empty.
   - **Additional fuel** in pounds (blank = none), sent to SimBrief so GSX loads it. Check SimBrief's parameter for
     extra fuel first and record it in `docs/notes/simbrief-api.md` (it isn't there yet).
   - **Route:** if Dave changed SimBrief's route on this leg before, his last one is offered ("Route: your last
     (KSAN–KSJC, 12 Nov) · use SimBrief's"), sent as `route`. Check the forum report that picking an airframe on the
     dispatch page can replace a route passed in the URL.
   - **Dispatch** rolls the loads, once (no hidden rolls), and shows the dispatch report before import.
3. **Before import:** see "The dispatch report" below. Open SimBrief ↗ opens the pre-filled dispatch page (as today).
4. **Import from SimBrief** (Re-import from SimBrief after the first) fetches the OFP once per click (D-101) and checks
   it against the flight: passengers exactly; cargo within about 1% or 100 lb (pounds/kilograms rounding);
   destination, airplane and flight number. A mismatch is **refused**, saying what to regenerate ("SimBrief has 40
   passengers and 2,100 lb cargo; this flight has 44 and 2,300 lb"), with Open SimBrief pre-filled with the right
   values. A refused re-import keeps the plan already imported. (Review finding 4.7.)
5. **A re-import** changes fuel, route, block time and the money lines that follow, never the loads. Rows that changed
   flip; the rest stay still. After Accept it replaces the accepted breakdown (D-222) with the rates locked at
   acceptance, without asking again.
6. **Accept** stores the breakdown and its rates (D-222) and puts Your Flight into Pre-Flight with the header.
   **Cancel flight** stays available until block out. In the log, Accept writes `flight_accepted` with a snapshot,
   the flight's start (D-236), and the recorder opens the flight's own log file (D-237).
7. **At block out** the money and Re-import lock (review finding 4.6): fuel is the fuel added (D-223), crew and
   maintenance follow the real block time (D-109, D-222), and nothing a new plan could change matters any more.
8. **View Dispatch Report** (a button on the dashboard, any time in the flight) opens the accepted report in a popup:
   Re-import there until block out, read-only after.
9. **Block in** settles the flight as today (D-208). The button beside LANDED is renamed **Settle Up**; it only
   reveals the summary.
10. **At Gate Turnaround** goes back to the form pre-filled for the same airplane where it parked. **Done for now**
    clears Your Flight.

No runway check on the report: the length check is the form's, and the runways are suggestions ATC may change.

## The dispatch report (D-234, board `Final`)

From top to bottom:

- **Header:** "DISPATCH REPORT", the flight number and route, a badge for extra fuel (information, not a warning).
  A landing-weight-near-MLW or fuel-on-board warning appears as an amber badge only when there's something to warn
  about.
- **The money card**, three columns:
  - **The ring.** The fares are the whole circle; each cost is a slice sized by its amount, in colour families (fuel
    red; landing, gate and handling orange; pilots, cabin crew and maintenance purple; catering and ATC blue); what's
    left is a green arc. The centre shows the profit and "of $X fares". **Hover** lifts a slice outward with a small
    tag (name and amount) just outside the ring. **Click** selects it: the slice stays lifted, the centre shows that
    cost, the working appears on the right.
  - **The split-flap board.** One row per line, fares at the top and profit at the bottom, 20 tiles a row (label left,
    value right). Labels light; values settle in amber (costs) or green (fares, profit). Rows are buttons: clicking
    one selects the line (and its slice).
  - **The working.** Before a selection, a hint ("Click a line on the board, or a slice of the ring…"). After, the
    line, its amount and how it was worked out ("9,200 lb block fuel − 3,300 lb on board = 5,900 lb added × $0.41 /
    lb; price locked when you booked"), plus what changed since booking where something did ("Was −$2,180 at
    booking: your +1,000 lb extra fuel") and "Charged on the actual block time" on crew and maintenance. Accept
    flight (green) and Cancel flight (red outline) sit at the bottom.
- **Load card:** "LOAD · registration, type, kind"; a strip of seats (filled ones lit); seats, bags and the holds;
  zero fuel, takeoff and landing weight as bars against their limits (the limit marked in red).
- **Plan card:** "PLAN · GENERATED <time>Z"; the two airports with an altitude-coloured line between them; runways,
  SID and STAR; cruise, distance, block, block fuel, burn and arrival time; then **Open SimBrief again ↗** and
  **Re-import from SimBrief**, centred. Claude's call: the **Copy route** button (for Navigraph Charts, §13.3) moves
  here, beside the route, since the planned flight panel goes away.

**Before the first import:** the board shows the labels with empty value tiles; the ring is an empty track with only
a dash in the centre; the working column says to import the plan; the plan card shows three steps ("Open SimBrief:
the dispatch page opens filled in with this flight, its loads and your extra fuel" / "Check the route and press
Generate there" / "Import the plan here") with **Open SimBrief ↗** (a filled secondary button) and **Import from
SimBrief** (primary) centred; the load card is filled (the loads are rolled at Dispatch) with takeoff and landing
weight "after import"; Accept is greyed. **Import** flips the values in row by row, once, and fills the ring.

**A flight that loses money** (a delivery flight, or costs above the fares): the circle is the costs instead, every
slice red to purple with no green arc, and the centre shows "Loss" and the amount in red. A delivery flight's board
shows its fares row as $0. (Claude's call; the mockups only show a profit.)

## The flight summary (D-235, board `Summary`)

Shown when Settle Up is clicked (and on Flights for a selected flight, without the action buttons).

- **Header:** "FLIGHT SUMMARY · date", the flight and route, the airplane and where it parked. Replay (the flight
  replay, D-175), Done for now, At Gate Turnaround (primary).
- **The flight in one picture:** the map with the track coloured by altitude (red near the ground, yellow, green, blue
  at cruise and back), with readouts over its corner: BLOCK (and the plan's, +4m), FUEL USED (and the plan's burn),
  OUT · IN (and cruise, distance).
- **Beside the map:**
  - **Score:** the score in a ring, the landing stars, `−562 fpm · 1.41 G`, a line about the touchdown (firmness,
    centreline, distance past the threshold) and a **Debrief →** button.
  - **What this flight moved forward:** hours flown with this flight's share, as a bar to the next slot (D-131);
    small milestones; the new cash balance with this flight's result. **Milestones are new** (nothing records them
    yet): examples are "10th arrival at KSJC", "first flight at FL400", "smoothest landing in the BBJ2 so far". Which
    ones, and whether they come in this milestone or later, is open; later, rotation progress ("2 of 4 legs toward
    the San Diego rotation", D-075) goes here too.
- **Money:** the dispatch report's ring, board and working, with the settled amounts. A row that differs from the
  accepted breakdown gets an amber tag beside it (`+$30`, profit `−$60`), and its working says why ("+$30 vs the
  dispatch report: 4 minutes longer than planned"). The board flips in once when the summary is revealed.

## The debrief (D-235, board `Debrief`)

Replaces the summary in place on Your Flight and Flights; **← Summary** goes back. Not a modal: it's long, and
Flights shows it too.

- **Header:** ← Summary, "DEBRIEF · date", the flight, Replay.
- **Score card:** the score ring (larger), the stars, how the score was reached ("100 − 6 for one missed check − 8 for
  a ★3 landing", from the score formula, `debrief.md` §4) and the last-10 average with ▲/▼.
- **Claude's review:** the review's opening as readable prose, a **…more** link that expands it to the full review
  in place (and "less" to fold it), and the "to work on" items as amber chips.
- **The checks, where they happen:** the dashboard's six-stage bar; under each stage, its checks as glass lamps
  (D-179: lamps are for what the debrief scores) with short names. Green passed, red missed, dark not checked; a stage
  with none says so. Clicking a lamp shows on the right what was measured and what passes ("Landing lights came on
  100 s after passing 10,000 ft on the way down: 40 s past the minute allowed"), the points, or why it wasn't checked.
  The stage each check belongs to follows its window in `debrief.md` §2.
- **The landing:** a top-down runway with the threshold, the aim-point zone and the touchdown marked (distance past the
  threshold, metres off the centreline, `debrief.md` §3's information), the stars and the points, then tiles for
  sink, G, bounce, speed against VREF, pitch, bank and wind with its crosswind, and a line on what the next star needs.

## What the code needs (for the plan)

- **The accepted state** and reopening the form for a flight that exists (review 4.5); "imported, not accepted"
  learnt by Your Flight's stages (review 3.1, 3.2).
- **Lines with their workings** (review 4.4): each money line carries its inputs (count × rate, hours × rate,
  pounds added × locked price) so the report, the summary and their deltas are all worked out in C++. The accepted
  breakdown is stored (D-222), so the summary can compare against it after a restart.
- **The import check** against the rolled loads (review 4.7), and Re-import refused after block out (review 4.6).
- **Remembered routes per leg:** a stored route for a city pair, written when an imported plan's route differs from
  SimBrief's automatic one. Schema by the database rules (D-078); the builder designs it in `database.md`.
- **Additional fuel** on the planned flight, sent to SimBrief.
- **The log at Accept** (D-236, D-237): the `flight_accepted` marker and snapshot, one log file per flight written
  by the recorder, the readers moved to it, and the one-time splitter for the old daily logs (kept untouched). The
  replay starts 30 s before the first GSX request, beacon or block out after the marker.
- **Fully loaded** in the load rolling (§9): every seat; cargo to max payload after bags, trimmed as §9 step 5 says.
  The flights-per-month setting that hides it at 30+ (D-220) comes with the money item; until then it's always offered
  on free flights.
- **Milestones**, if they come in this milestone: computed from the flights in the database, not stored.
- **QML:** the ring (a `Shape` or `Canvas` with an arc per line), the board (from `FlipCard`), the lamps (the in-flight
  panel's `Lamp`), the stage bar (the header's). Hover lift on the ring is a short transition, not a running animation.

## Left to the builder

- The form's layout, in the dispatch report's style.
- Exact sizes, spacing and colours, in `Theme.qml`; the ring's colour families as given above.
- Whether the View Dispatch Report popup is a `Dialog` or an overlay panel.
- How the board fits narrower windows (Flights is narrower than Your Flight): fewer tiles a row, or a smaller tile.
- Naming in code (`DispatchReport`, `YourFlightPage`, …) and the rename of Live in the UI, docs and code.

## Calls made by Claude (confirmed by Dave, 2026-11-27)

- Copy route moves to the plan card (it lived on the planned flight panel).
- The map, score card and progress card above the money on the summary (the money was the dispatch report's hero).
- Re-import after Accept doesn't ask to accept again.
- Cargo tolerance of about 1% or 100 lb on import.
- A losing flight's ring (above).
- Flights settled before dispatch have no accepted breakdown: their summary shows no "moved since the dispatch
  report" tags.
