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
- **Gate names SayIntentions uses (seen, for milestone 9):**
  - 2026-11-24, KLAS, 777-300ER from KSLC, in cruise: SayIntentions' status window showed the arrival gate as
    **"Terminal 3 Gate E7A"**. Its status window fills in the STAR, landing runway, runway transition and arrival gate
    as soon as its AI decides them, before top of descent; ATC only says the gate on the ground, when you're ready to
    taxi.
  - 2026-11-24, KSLC arrival: ground said "Gate Bravo Two via Alpha-Five" (`atc_exchange` `incoming_message`), so
    spoken gates are in the phonetic alphabet ("Bravo Two" for B2), not the short form.
- **The local `flightJSON` has the status window's data** (`http://localhost:43117/flightJSON`, read 2026-11-24 in
  cruise to KLAS, no key needed). The fields that matter:
  - `flight_details.current_flight`: `assigned_gate` ("Terminal 3 Gate E7A"), **`assigned_gate_lat` /
    `assigned_gate_lon`** ("36.0853653715", "-115.136728607269", as strings), `flight_plan_star` ("CHOWW4"),
    `flight_plan_arriving_runway` ("26L"), `destination_arriving_runways` ("26L,19L,19R"), `flight_destination`.
  - `flight_details.atc.route`: `star`, `filed_star`, `runway_transition` ("PRINO"), `approach_transition` ("STEWW").
  - `flight_details.atc.approach`: `type` ("ILS"), `variation` ("Z"), `runway` ("26L").
  - `flight_details.atc.altitude`: `miles_to_tod` ("32"), `minutes_to_tod` ("4"), `cruise_ft`,
    `transition_altitude_ft`. Dave: SayIntentions' TOD is always 40-80 nm earlier than the FMC's, so don't use it
    for predictions.
  - `flight_details.arrival_wx`: the destination's ATIS, METAR, TAF and active runways, like getWX.
  - Stale values: `atc.ground.on_runway` still said "34R" in cruise.
  - **The same JSON holds the pilot's `api_key` and email**: never log or store the whole thing, only pick fields.
- **No TaxiTuner access (checked 2026-11-24).** TaxiTuner is SayIntentions' airport-layout editor (taxiways, gates,
  stands, with terminal, size and assigned airline per spot since v3). Dave cloned KPDX there so XLY always gets
  D14. SAPI has no endpoint that lists an airport's gates or reads or writes airline assignments. The only gate calls
  are `assignGate` (above) and **`getParking`**: the active flight's own parking, `{"parking": {"id", "name": "Gate
  A21", "lat", "lon", "heading"}}`. `importVAData` (POST) only takes free-text prompt data for crew and dispatch. So
  BlockTime picks from MSFS's parking data and asks with `assignGate` (D-074), which is what Dave wants: no per-airport
  TaxiTuner editing.
- **Timing (for milestone 9, D-141):** SayIntentions had already chosen E7A in cruise, well before D-074's "around
  top of descent". So BlockTime waits for SayIntentions' own `assigned_gate`, uses its landing runway to pick, then
  sends `assignGate` and reads the result back from `getParking` or the flightJSON `assigned_gate`. Still to test in
  the sim: that `assignGate` replaces a gate SayIntentions already chose (Dave expects it does, since the gate
  hasn't been broadcast yet; he checks SayIntentions' status screen).
- **For milestone 9:** the gate's position should map it onto an MSFS parking spot (nearest `probe_facility` parking),
  which avoids parsing "Terminal 3 Gate E7A". Still open: whether `assignGate` takes "E7A" or needs the terminal.
- **SayIntentions' names aren't always MSFS's (checked 2026-11-24).** KLAS "Terminal 3 Gate E7A" at its
  `assigned_gate_lat/lon` is 16.6 m from MSFS's Gate E7 (name 16, number 7, suffix 0); the next spot is a plain
  "Gate 46" 20 m away. Every KLAS spot has suffix 0, so the "A" is SayIntentions' own. BlockTime sends MSFS's short
  form ("E7") and, if `assignGate` answers "Unknown gate", keeps SayIntentions' own gate (D-147). Milestone 9's sim
  test shows which happens.
- **The SAPI reference, read 2026-11-24:** `assignGate`'s gate is "max 30 characters, alphanumeric only" (no spaces);
  errors `{"error": "Invalid airport code: ..."}`, `{"error": "Unknown gate: Z99"}`, `{"error": "No active flight
  could be found."}`, `{"status": "UNKNOWN ERROR"}`. `getParking` answers `{"parking": {"id", "name": "Gate A21",
  "lat", "lon", "heading"}}` or `{"error": "No parking found"}`: its name has "Gate " in front, `assignGate`'s answer
  (`assigned_gate_name`) doesn't. **`getAirport`'s answer isn't documented** ("comprehensive airport information
  including weather and communication frequencies", for the airports of the current flight plan): with `--probe`,
  BlockTime logs it once per flight as `probe_si_airport` (every `api_key` removed) to see whether it lists gates.
- **How BlockTime uses it (0.10.0, `FlightWatcher`):** polls flightJSON every 30 s from block out; once
  `assigned_gate` is set for the planned destination and the destination's spots are loaded, it picks (D-147) and
  logs one `arrival_gate` event per flight. Failures before the send are quiet (SayIntentions may not be running).
  SayIntentions' gate counts only when `assigned_gate_lat/lon` are within 5 km of the destination: what
  `assigned_gate` holds before departure hasn't been read yet (it may be the departure gate). Milestone 9's sim test
  reads it at the gate.
- **Seen in milestone 9's sim test (2026-11-24, N814SX KSFO→KLAS→KSAN, 0.10.0):**
  - Before pushback at KSFO, Dave found no `assigned_gate` in flightJSON. SayIntentions fills it in only once it picks
    the arrival gate (in cruise both legs: 22:45Z, 8 min after reaching FL390; 00:21Z, 30 s after reaching FL360).
  - **`assignGate` answers without an error even when it changes nothing.** KLAS: BlockTime sent "239" (MSFS's
    "Gate 239", a stand mid-field by 08R); no error, but `getParking` still read back "E7A" and ATC kept Terminal 3
    Gate E7A. KSAN: BlockTime sent "70" (MSFS's "Parking 70", a GA ramp); SayIntentions took it and named it
    "Gate 106" (`getParking` "106"), replacing its own Terminal 2 East Gate 27. So a send counts as accepted only when
    the read-back changes.
  - **MSFS's parking data lists spots the scenery doesn't have.** KSAN's Parking 70 (32.7320, -117.1900, type 4, radius
    24 m) sits in what the KSAN scenery shows as a construction zone: no stands, obstacles on the ground. The facility
    data gives no hint of this.
  - **`getAirport` returned `[]`** for both KLAS and KSAN: no gates, no frequencies. Not useful as called.
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
