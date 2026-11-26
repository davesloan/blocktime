# SimBrief API: dispatch page and OFP fetch

Researched 2026-11-23 for milestone 5 (design.md §13.2, D-070). Nothing here is tested against Dave's account yet;
each item says where it comes from.

Sources:
- Dispatch redirect guide: https://forum.navigraph.com/t/dispatch-redirect-guide/5299
- Fetching the latest OFP: https://forum.navigraph.com/t/fetching-a-users-latest-ofp-data/5297
- API docs: https://developers.navigraph.com/docs/simbrief/using-the-api
- A real JSON v2 OFP, used as a test fixture by an open-source Go client:
  https://raw.githubusercontent.com/mrlm-net/simbrief/main/pkg/types/testdata/ofp_v2.json (not copied into the repo;
  its licence wasn't checked)

## The dispatch page (no API key)

- `https://dispatch.simbrief.com/options/custom?` plus query parameters (guide 5299). Opens in the browser; the user
  must be logged in to SimBrief.
- Parameters: `orig`, `dest`, `airline`, `fltnum`, `callsign`, `type`, `origrwy`, `destrwy`, `pax`, `cargo`, `units`
  (`LBS`/`KGS`), `date`, `deph`/`depm`, `route`, `static_id`; also `reg`, `fin`, `fl`, `manualzfw`, `acdata`.
- **A custom airframe goes in `type`**, as its internal id (`type=730097_1790536204614`). Confirmed by SimBrief staff
  (https://forum.navigraph.com/t/directly-call-dispatch-url-and-set-specific-aircraft/7315). A deleted airframe gives
  "Unknown Aircraft".
- Forum report, not checked: choosing an airframe on the page can recalculate and replace a route passed in the URL.
- **`cargo` is in thousands** of the plan's units (checked by Dave, 2026-11-23): with `units=LBS`, `cargo=5` gave
  5,000 lb of freight, and `cargo=5000` was capped at the airframe's max cargo (17,645 lb for N814SX). SimBrief adds
  passenger baggage itself from `pax`, so `cargo` is only the extra freight. BlockTime sends cargo in thousands with
  up to three decimals (exact pounds).
- `origrwy`/`destrwy` from SayIntentions went through in Dave's first real planning test (2026-11-23, milestone 5 check 5).

## Fetching the OFP

- `https://www.simbrief.com/api/xml.fetcher.php?userid=<id>&json=v2` (or `username=`), plus `&client=<app name>`.
  `&static_id=` fetches a specific flight.
- **Don't poll it.** SimBrief asks apps to call it "only in response to a user action" and says excessive calls can
  get an app blocked by its firewall (5297). No number is given. This changes D-070's "polls until one appears".
- Wrong user id: HTTP 400 and `{"fetch":{"status":"Error: Unknown UserID", ...}}` (tried). Success has
  `fetch.status == "Success"`. A user with no OFP isn't documented or tried.

## JSON v2 fields (from the fixture)

All values are strings, in the OFP's units (`params.units`, `lbs` or `kgs`).

| What | Field | Note |
|---|---|---|
| Generated at | `params.time_generated` | ISO `2026-10-03T21:18:38Z` (the XML has Unix seconds) |
| OFP id | `params.request_id` | unique per OFP; also `sequence_id`, `static_id` (empty unless set) |
| Pilot id | `params.user_id`, `fetch.userid` | |
| Airports | `origin.icao_code`, `destination.icao_code` | |
| Runways | `origin.plan_rwy`, `destination.plan_rwy` | |
| Transition altitude / level | `origin.trans_alt`, `destination.trans_level` | `"05000"`; both exist on both airports |
| Route | `general.route` | also `route_navigraph`, `route_ifps` |
| Cruise | `general.initial_altitude` | feet |
| Distances | `general.air_distance`, `route_distance`, `gc_distance` | nm |
| Fuel | `fuel.plan_ramp`, `fuel.enroute_burn` | there is no `est_burn` |
| Times | `times.est_block`, `times.est_time_enroute` | **`HH:MM:SS` in JSON v2** (seconds in the XML); also `est_out`/`est_in` (ISO) |
| Loads | `weights.pax_count`, `weights.cargo`, `weights.payload`, `weights.est_zfw`, `weights.est_ldw` | |
| Airframe limits | `weights.oew`, `max_zfw`, `max_ldw`, `max_tow`, `pax_weight`, `bag_weight`; `aircraft.max_passengers` | from the airframe; only available once an OFP exists |
| Airframe id | `aircraft.internal_id`, `aircraft.is_custom` | |

- No minimum runway length was seen in the OFP.
- Unclear: whether `weights.pax_weight` includes bags (the fixture's 80 kg + 15 kg don't match its airframe's
  176 + 33 lb).

## The airframe id isn't `<user id>_<number>`

The fixture has `params.user_id` 857341 but `aircraft.internal_id` `80_1722530034323`, so the first part of an
airframe id isn't reliably the pilot's id (it may have been a copied airframe). The second part looks like a creation
time in Unix milliseconds. BlockTime takes the pilot id from settings, never from the airframe id.

## The navlog (milestone 12, D-182)

The dashboard's route arc follows the plan's navlog. In the OFPs BlockTime stores (`ofp_json`), `navlog` is an
**array of fixes**; SimBrief's documentation shows `navlog.fix`, so `parseNavlog` reads both. Each fix has `ident`,
`pos_lat` and `pos_long` (strings, degrees), `stage` (`CLB`, `CRZ`, `DSC`), and more that BlockTime doesn't read. The
list includes the `TOC` and `TOD` pseudo-fixes and ends with the destination airport itself (KSAN → KSJC, 17 fixes:
`tests/data/ofp-ksan-ksjc-navlog.json`, cut down to those four fields).

## The Pre-Flight panel's fields (milestone 13, D-188)

Read from the stored OFP (`ofp_json`) by `parsePlanBrief`; checked in Dave's KSAN → KSJC plan of 2026-11-25
(`tests/data/ofp-ksan-ksjc-brief.json`, cut down to these fields).

| What | Field | Example |
|---|---|---|
| SID and its transition | `general.sid_ident`, `general.sid_trans` | `PADRZ2`, `IKAYE` (the route string starts `PADRZ2 IKAYE …`) |
| STAR (not read yet) | `general.star_ident`, `general.star_trans` | `SILCN6`, `TROXX` |
| ZFW | `weights.est_zfw` | `112967` |
| Cargo | `weights.cargo` | `2420`: bags and freight together (44 bags × 55 lb, `freight_added` 0) |
| Block fuel | `fuel.plan_ramp` | `13439` (already a `simbrief_plan` column) |
| Takeoff runways | `tlr.takeoff.runway[]`: `identifier`, `magnetic_course` (also `true_course`, lengths, V-speeds) | `27` → `275` |
| Planned surface | `tlr.takeoff.conditions.surface_condition` | `dry` |

- The takeoff report lists every runway at the origin, not only the planned one, so a runway ATC changes still has a
  magnetic course. Plans made without the TLR have no `tlr`; older fixtures (`ofp_v2_min.json`) have none of these.
- Other fields seen and not used: `general.costindex` (`30`), `fuel.reserve`, `fuel.alternate_burn`, `weights.pax_count`.
