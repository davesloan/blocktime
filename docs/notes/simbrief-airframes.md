# SimBrief airframes

What a SimBrief custom airframe holds, and Dave's values. Each airplane has its own SimBrief airframe carrying its
registration (D-087). design.md §3 makes SimBrief the source of truth for weights and limits.

## Where to find things on SimBrief

- **Internal ID** (top right of the airframe's page), shaped `<SimBrief user id>_<number>`, for example
  `730097_1790536204614`. This is what `airline.yaml`'s `simbrief_airframe_id` holds, and what the dispatch page's
  `type` parameter takes. The first part is **not** reliably the pilot's SimBrief id (an OFP seen in research had
  user 857341 and airframe `80_…`; `docs/notes/simbrief-api.md`), so the pilot id is a setting of its own.
- **Civil Registration** is the airplane's registration. **Fin Number** is the tail number.
- The page also has SELCAL, equipment codes, fuel policy and performance profiles. BlockTime doesn't use those.

## N814SX, 737-800 BBJ2 (Soxxly Airlines, fin 1001)

From Dave's SimBrief airframe, 2026-11-22. Dave checked every number in the sim's FMC.

| Field | Value |
|---|---|
| Internal ID | `730097_1790536204614` |
| Base type / ICAO code | BBJ2 / B738 |
| Engines | CFM56-7B27, takeoff thrust 27,300 lbf |
| Max passengers | 48 |
| Passenger weight / baggage weight | 190 lb / 55 lb |
| Empty weight | 102,187 lb |
| Max zero fuel weight | 138,300 lb |
| Max takeoff weight | 174,700 lb |
| Max landing weight | 146,300 lb |
| Max fuel capacity | 69,986 lb |
| Max cargo weight | 17,645 lb, split 4:5 forward:aft (7,842 / 9,803 lb) |
| Min runway | 7,000 ft |
| Service ceiling | 41,000 ft |
| Max diversion | 417 nm |
| ETOPS | 120 min threshold, 180 min certification |

## Used where

- **Milestone 4:** the catalog's BBJ2 holds total the 17,645 lb max cargo, split 4:5 forward:aft (the ratio
  confirmed by Dave, 2026-11-22). The 48 seats match.
- **Milestone 5:** New Flight needs, before any OFP exists:
  - passenger and bag weights;
  - max payload (MZFW − empty weight = 36,113 lb);
  - MLW, for the landing-weight warning;
  - min runway, for the runway check.

  Where these live (the catalog airframe, or each airplane, copied at lease time like the seats) is decided in
  milestone 5's plan. SimBrief has no public airframe API that we know of, so they're typed in once from this page.
