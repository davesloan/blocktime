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
| `probe_pmdg_737` / `probe_pmdg_777` | Any listed value changes, at most once a second | The switches in `pmdg-variables-from-spad.md`, by SDK name: battery, seatbelt sign, APU, every light, anti-ice, window and probe heat, doors, CMD/AP and A/T, LNAV/VNAV, gear lights, parking brake. Also the aircraft model and the FMC's figures (distance to top of descent and to destination in whole nm, cruise altitude, V-speeds, transition altitude and level, flight number). The 777 adds `APURunning`, `WheelChocksSet` and the APP light. The 737's APU EGT is rounded to 10° |
| `probe_facility` | When the sim connects with a flight planned, and after each block in, for its origin and destination | Each runway (position, heading, length, width, number and designator) and parking spot (type, name, suffix, number, heading, radius, offset from the airport). Every record also has `raw` (its bytes in hex), in case a field type in the request is wrong |

## Questions for the first probe flight

These were answered by the 2026-11-23 flights (Results below), except the 777 parts.

1. **GSX (milestone 8):** which states each service goes through, whether `FSDT_GSX_DEPARTURE_STATE` covers pushback,
   and whether the passenger counts fill in.
2. **PMDG switches (milestones 8 and 10):** which `probe_pmdg_*` values move with each switch, and whether the plain
   SimVars match them (especially the 777's parking brake, `msfs-observed-behaviour.md`).
3. **Parking spots (milestone 9):** whether `name` + `number` + `suffix` give the gate names SayIntentions uses (for
   example "B23"). Dave notes the gate ATC gives him at KSAN to compare.
4. **Runways (milestone 7):** whether `heading` and the threshold positions are good enough to name the departure
   runway from the takeoff position.

## Results

### First probe flights: 2026-11-23, PMDG 737-800 BBJ2, recorder 0.7.0

The 737 BBJ2 (N814SX) flew KBFI → KPDX → KSFO, with a GSX turnaround at KPDX. The 737-800 (Soxxly) was loaded
briefly at KBFI. Both PMDG halves were built in (`probe_started`: `"pmdg_737": true, "pmdg_777": true`). The 777
wasn't flown, so it has no results yet. The log is `events-2026-11-23.jsonl` and `events-2026-11-24.jsonl` (UTC
days). Times below are UTC.

**1. GSX.** Every L:var name works. The values were read once a second.

- **States seen:** 1, 3, 4, 5 and 6.
  - **1** is idle (available). Every service sits at 1 once a flight is loaded and goes back to 1 when it ends.
  - **4** means requested and **5** means performing.
  - **6** (completed) shows for about 10 s, then the state goes back to 1. Seen for refuelling and catering.
  - Boarding went 5 → 1 with no 6 seen. Either 6 lasted less than a second, or boarding skips it.
  - **3:** requesting pushback set boarding, catering, deboarding and refuelling to 3 (bypassed). They went back to 1
    when the push ended.
  - **2** (not available) never appeared.
  - So the D-073 list holds for 1, 3, 4, 5 and 6.
  - All states read 0 before a flight is loaded.
- **Pushback is covered by `FSDT_GSX_DEPARTURE_STATE`:**
  - 4 when the push was requested (22:49:23);
  - 5 when the push started (22:51:03), 11 s before the brake release was confirmed as block out;
  - back to 1 when the push finished (22:53:32), with no 6.
- **Refuelling:**
  - 4, then 5;
  - `FUELHOSE_CONNECTED` 1 for about 40–80 s while fuel flowed;
  - 6, then 1.
- **Passenger counts fill in:**
  - `NUMPASSENGERS` is the load GSX is set to: 44 for the VIP leg (184 on the Soxxly at load, from its defaults).
  - `NUMPASSENGERS_BOARDING_TOTAL` counted up 5 → 44 during boarding, about one passenger every 1–8 s.
  - `BOARDING_CARGO_PERCENT` jumped 0 → 50 once, near the end of boarding.
- **Deboarding wasn't used** at KPDX or KSFO, so the deboarding states and counts are still unseen.
- **`COUATL_STARTED`** is 1 while GSX's engine runs. It flickers 0/1 for a few seconds around each load and exit to
  the menu.

**2. PMDG 737 switches.** Every `probe_pmdg_737` value moved with its switch, and the values match the SDK header
comments (`PMDG_NG3_SDK.h`).

- **Lights:**
  - `LTS_PositionSw`: 0 steady, 1 off, 2 strobe & steady. 2 at the runway, 0 after landing; 1 wasn't seen.
  - Also moved: `LTS_AntiCollisionSw`, `LTS_TaxiSw`, `LTS_LandingLtFixedSw[2]`, `LTS_RunwayTurnoffSw[2]`, `LTS_LogoSw`,
    `LTS_WingSw` and `LTS_WheelWellSw`. On this panel the wheel-well light went on and off with the taxi light.
  - `LTS_LandingLtRetractableSw` stayed 0: the BBJ2 flights left those lights off.
- **Cabin and systems:**
  - `COMM_FastenBeltsSelector` is 0 off, 1 auto, 2 on.
  - `APU_Selector` is 0 off, 1 on, 2 start, and springs back to 1. A value of 3 showed for one sample at two of the
    starts. It isn't in the header, so it's probably the knob in motion.
  - `ICE_WindowHeatSw[4]` and `ICE_ProbeHeatSw[2]` showed each switch flipped in turn. Wing and engine anti-ice
    weren't used.
- **Doors:** `DOOR_annun` is logged in the header's order:

  | Index | Door |
  |---|---|
  | 0 | forward entry |
  | 1 | forward service |
  | 2 | airstair |
  | 3–4 | forward overwing |
  | 5 | forward cargo |
  | 6 | equipment |
  | 7–8 | aft overwing |
  | 9 | aft cargo |
  | 10 | aft entry |
  | 11 | aft service |

  GSX opened 0 for boarding, 1 and 11 for catering, and 5 and 9 for cargo. All doors were closed 1 to 2.5 minutes before block out.
- **Autopilot and gear:**
  - `MCP_annunCMD_A/B`, `MCP_annunATArm`, `MCP_annunLNAV` and `MCP_annunVNAV` followed the autopilot. CMD A came on
    about 13 s after liftoff, and CMD B was added for the approach.
  - `MAIN_annunGEAR_transit` and `MAIN_annunGEAR_locked` showed each leg moving in turn, about 8 s from up to down.
  - `PED_annunParkingBrake` matched every brake set and release, including the sets at the hold short.
- **The plain SimVars only partly follow the PMDG 737:**

  | SimVar | Follows |
  |---|---|
  | `LIGHT BEACON` | the anti-collision switch |
  | `LIGHT STROBE` | position switch at 2 |
  | `LIGHT LANDING` | the fixed landing lights |
  | `LIGHT TAXI` | taxi **or** runway turnoff lights (it stayed 1 with only the turnoff lights on) |
  | `LIGHT NAV` | nothing useful: 1 the whole time the aircraft had power, whatever the position switch |
  | `CABIN SEATBELTS ALERT SWITCH` | never moved; read `COMM_FastenBeltsSelector` instead |
  | `APU SWITCH` | never moved; read `APU_Selector` instead |
  | `GEAR HANDLE POSITION`, `GEAR TOTAL PCT EXTENDED` | the gear |

  **`GEAR HANDLE POSITION` reads 0 while the sim is paused** (the Escape menu). For example, it went 0 at 23:41:20 and
  back to 1 at 23:41:25 with the gear down. PMDG's gear lights didn't move, so ignore the plain gear handle while
  paused.
- **The 777's parking brake** (`msfs-observed-behaviour.md`) is still open: no 777 flight yet.

**3. Parking spots.** Each airport's spot list arrived (KBFI 245, KPDX 154, KSFO 247), and the block positions match
spots closely. These enum readings come from the values seen, not from documentation.

- **`name`** behaves like SimConnect's parking name list:
  - 0 none, 1 parking, 2 N parking, 6 S parking, 7 SW parking, 8 W parking;
  - **12 onwards are Gate A, B, C …**
  - KSFO's 12–18 match its boarding areas A–G, and KPDX's 12–16 match its concourses A–E.
- **`type`** fits the same list:
  - 2–4 GA ramps (small, medium, large), 5 cargo ramp, 6–7 military, 8–10 gates (small, medium, heavy), 12 fuel,
    13 vehicles;
  - KPDX's 15 military spots fit the Air National Guard base there.
- **The spot nearest each block event was about 5 m away:**

  | Where | Spot | So the gate is |
  |---|---|---|
  | KBFI start | name 1, number 70, type 3 | Parking 70, a GA medium ramp |
  | KPDX block in | name 15, number 14, type 10 | **D14** |
  | KSFO block in | name 15, number 14, type 8 | **D14** |

  KPDX and KSFO really are two different spots with the same name: their headings (86° and 140°) and types differ.
- **`suffix`** is 0 on almost every spot. A few at KPDX (12, 13) and KSFO (12, 22–33) aren't 0. Their meaning isn't
  known yet; check one against the scenery before building gate names with suffixes.
- **Compared with SayIntentions:**
  - At KPDX, Dave parked at the gate SayIntentions assigned.
  - At KSFO he parked at the nearest gate instead, so KSFO doesn't test the match.
  - **SayIntentions called the KPDX gate "D14"** (Dave, 2026-11-23; there may have been a space after the D). So
    `name` 15 (Gate D) + `number` 14 gives SayIntentions' gate name. Confirm the space on a later flight before
    milestone 9 sends gate names to SayIntentions.

**4. Runways.** Each record has the runway's centre, its true heading, its length and width, and both ends'
numbers and designators (0 none, 1 L, 2 R, 3 C). KBFI 14L/32R comes out as 1,131 m (3,709 ft), which is right.

- **Naming the runway from the track works without doubt.** I took the ground roll above 40 kt and picked the runway
  end whose centreline and heading fit it best:

  | Roll | Runway | Track off the runway heading | Off the centreline |
  |---|---|---|---|
  | KBFI takeoff | 14R | 0.1° | 3 m or less |
  | KPDX landing | 10R | 0.4° | 5 m or less |
  | KPDX takeoff | 10R | 0.0° | 2 m or less |
  | KSFO landing | 28R | 0.9° | 5 m or less |

- In every case the next-best runway was at least 115 m off the centreline, including the close parallels at KSFO
  (28L, 235 m) and KBFI (14L).
- The threshold positions come from centre ± half the length along the heading. That was good enough here.

**5. The FMC's predictions (D-120), from the SDK headers.** No flight has read these yet.

- Both headers have `FMC_DistanceToTOD` (nm; 0.0 once passed, negative if not available) and `FMC_DistanceToDest`
  (nm). They also have the FMC's cruise altitude, V-speeds, transition altitude and level, and flight number.
- The 777 header also has `APURunning`, `WheelChocksSet` and `DOOR_state[16]`:
  - `DOOR_state` values: 0 open, 1 closed, 2 closed and armed, 3 closing, 4 opening;
  - the doors: 0–9 entry 1L to 5R, 10 forward cargo, 11 aft cargo, 12 main cargo (freighter), 13 bulk cargo,
    14 avionics, 15 EE access.
- **The probe reads them from 2026-11-23 on** (after the first probe flights), in both halves.

The analysis scripts weren't kept. Rerun from the events: the GSX and switch timelines are the changes in each
`probe_*` event. The runway match is the telemetry's ground roll against each runway's centreline.

## Questions for the next probe flight

Flown during milestone 7's sim checks, with `--probe`:

1. **The 777** (ideally the 777-300ER, the one with the parking brake question in `msfs-observed-behaviour.md`):
   - which `probe_pmdg_777` values move with each switch;
   - whether `BRAKES_ParkingBrakeLeverOn` and the plain `BRAKE PARKING POSITION` agree on a release;
   - the `DOOR_state` values through a GSX turnaround.
   - BlockTime can't record a 777 flight yet (one slot until 10 h flown), but the probe logs the switches anyway. The
     runways and parking spots are logged only with a flight planned.
2. **The FMC's figures (D-120), on either family:** whether `FMC_DistanceToTOD_nm` counts down to 0 at the top of
   descent, and whether `FMC_DistanceToDest_nm` tracks the distance flown.
3. **GSX deboarding:** its states and passenger counts.
4. **The gate name:** whether SayIntentions writes "D14" or "D 14".
