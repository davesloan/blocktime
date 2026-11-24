# SayIntentions API (SAPI)

Researched 2026-11-23 for milestone 5's ATIS runways (design.md §13.2, D-070) and later milestones. Not yet tried
with Dave's keys.

Sources:
- The official SAPI reference: https://p2.sayintentions.ai/p2/docs/
- Help-center articles on kb.sayintentions.ai (the old freshdesk articles are gone)
- Unauthenticated test calls on 2026-11-23

## Keys

- Two keys. The **developer key** identifies the app and goes in an `X-Developer-Key` header, never in the URL, "so it
  doesn't end up in URLs, logs and browser history". The pilot's **`api_key`** identifies the pilot (portal account
  page, or `api_key` in `http://localhost:43117/flightJSON`).
- Developer keys come from https://p2.sayintentions.ai/p2/#developer-key. A **personal** key is issued at once and
  works only with your own `api_key`. A **distribution** key, for other pilots, is reviewed first.
- The developer key may not go in public source code, and each app needs its own. Keys must not be logged. BlockTime
  keeps both in `settings.json`, like the CARTO key.
- The docs say SAPI needs a subscription and is "in PREVIEW and Free". The test calls without any key returned data,
  which is undocumented: build with both keys.

## getWX

- `GET https://apipri.sayintentions.ai/sapi/getWX?api_key=<key>&icao=KBFI,KSAN` with the header. `icao` takes a
  comma-separated list; `with_comms=1` adds frequencies.
- Documented shape: `{"airports":[{airport, atis, atis_cpdlc, metar, taf, active_runway, wind_direction,
  wind_speed}], "comms":[...]}`.
- **The live answer differs from the docs**: no `active_runway`, but `active_runways_departing` ("16L,16R,17"),
  `active_runways_arriving`, `closed_runways`, `preferred_runway_ga_departing`/`_arriving`, `currently_operating`,
  `runway_source`, wind fields, `altimeter`, `current` (the ATIS letter). Undocumented, so parse it loosely and fall
  back to no runway.
- Loose types: a field can come back as a number in one call and a string in another (wind 240 vs "130"). Runways
  come as "9R" in one field and "09R" in another.
- An airport without an ATIS: `"atis": ""`, runway and METAR fields still filled. An unknown ICAO: HTTP 200 with
  `{"airports":[]}`.
- Errors: `{"error":"..."}` with 400, 401, 403 or 500 ("The provided API key is invalid.").
- No strict rate limits; they ask apps to cache weather and avoid rapid-fire calls.

## Later milestones

- **assignGate** (milestone 9): `GET .../sapi/assignGate?api_key=&gate=A21&airport=KSAN`. Gate is letters and digits,
  up to 30. Needs an active SayIntentions flight. Returns `{"assigned_gate_name":"A21"}`; errors "Unknown gate: Z99",
  "No active flight could be found."
- **getCommsHistory** (milestone 10, the ATC log for D-040): `GET .../sapi/getCommsHistory?api_key=&since_id=`.
  Returns `flight_id` and `comm_history[]` (`stamp_zulu`, `frequency`, `station_name`, `incoming_message` from the
  pilot, `outgoing_message` from ATC, English versions, audio URLs). **Current flight only**: it can't be fetched
  after the flight ends, so it has to be collected during it. Unread report: a reply can be written a few seconds
  late, so `since_id` polling may skip it (https://github.com/psyraxaus/ProsimCompanion/issues/237).
  - **No local copy.** Checked 2026-11-23: SayIntentions' folders on Dave's PC hold no ATC text:
    - `%LOCALAPPDATA%\SayIntentions\SI-Telemetry\logs\si_telemetry.log` is the telemetry bridge's own log;
    - the rest is the client's WebView cache and a Stream Deck log.

    So a flight's ATC log exists for BlockTime only if it was polled during the flight. Milestone 6's flights
    (KBFI–KPDX–KSFO) have none.
- Also listed: getParking, getAirport, getTFRs, getVATSIM, setFreq, importVAData (for virtual airlines).
