# The probe flight (`--probe`, D-116)

Milestones 7–10 build on sim values nobody has seen yet: GSX's service states, which PMDG switches can be read, and
the sim's runways and parking spots. Started with `--probe`, BlockTime logs them during milestone 6's test flights.
Nothing extra is requested without the flag. Results go in this note once a probe flight has been read.

## Switching it on

- **Build:** the PMDG part needs the SDK headers that ship with each PMDG aircraft. They aren't committed. Point CMake
  at their folders once (the release preset keeps them):

  ```powershell
  cmake --preset x64-release "-DBLOCKTIME_PMDG_NG3_SDK_DIR=I:/flightsim/community/pmdg-aircraft-738/Documentation/SDK" "-DBLOCKTIME_PMDG_777X_SDK_DIR=I:/flightsim/community/pmdg-aircraft-77w/Documentation/SDK"
  ```

  Without them the build still works, and the probe logs everything but the PMDG values. `probe_started` says which
  halves are in (`"pmdg_737": true`).
- **Run:** `build\x64-release\BlockTime.exe --probe`. The event feed says "Probe on".
- **The PMDG data broadcast** must be on in the aircraft's options file (`EnableDataBroadcast=1` in
  `737NG3_Options.ini` / `777X_Options.ini`). Dave's SPAD.neXt profiles already read it, so it's on at least for the
  737 (`pmdg-variables-from-spad.md`).

## What's logged

| Event | When | What it answers |
|---|---|---|
| `probe_started` | Each sim session | Whether the probe's requests went in, and which PMDG halves are built |
| `probe_gsx` | Any GSX value changes (checked once a second) | GSX's state values through a turnaround: 1 available … 6 completed (unverified, D-073), passenger counts, cargo percent, fuel hose. The L:var names come from community tools; all zeros means they're wrong or GSX isn't running |
| `probe_simvars` | Any changes (once a second) | `LIGHT BEACON`, `LIGHT STROBE`, `LIGHT LANDING`, `LIGHT TAXI`, `LIGHT NAV`, `CABIN SEATBELTS ALERT SWITCH`, `APU SWITCH`, `GEAR HANDLE POSITION`, `GEAR TOTAL PCT EXTENDED`: whether the plain SimVars follow PMDG's switches |
| `probe_pmdg_737` / `probe_pmdg_777` | Any listed value changes, at most once a second | The switches in `pmdg-variables-from-spad.md`, by SDK name: battery, seatbelt sign, APU, every light, anti-ice, window and probe heat, doors, CMD/AP and A/T, LNAV/VNAV, gear lights, parking brake |
| `probe_facility` | When the sim connects with a flight planned, and after each block in, for its origin and destination | Each runway (position, heading, length, width, number and designator) and parking spot (type, name, suffix, number, heading, radius, offset from the airport). Every record also has `raw` (its bytes in hex), in case a field type in the request is wrong |

## Questions for the first probe flight

1. **GSX (milestone 8):** which states each service goes through, whether `FSDT_GSX_DEPARTURE_STATE` covers pushback,
   and whether the passenger counts fill in.
2. **PMDG switches (milestones 8 and 10):** which `probe_pmdg_*` values move with each switch, and whether the plain
   SimVars match them (especially the 777's parking brake, `msfs-observed-behaviour.md`).
3. **Parking spots (milestone 9):** whether `name` + `number` + `suffix` give the gate names SayIntentions uses (for
   example "B23"). Dave notes the gate ATC gives him at KSAN to compare.
4. **Runways (milestone 7):** whether `heading` and the threshold positions are good enough to name the departure
   runway from the takeoff position.

## Results

None yet: Dave's milestone 6 sim checks are the first probe flight.
