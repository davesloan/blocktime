# The debrief: procedure checks, the landing and Claude's review (milestone 10)

Brainstormed with Dave on 2026-11-24 (Claude Code session), replacing the open parts of D-040. Decisions D-156 to
D-162. **Built in 0.11.0** (`docs/plans/2026-11-24-milestone-10-debrief.md`), which completes the MVP. Where the build
settled something this design left open, it's marked here and in D-162.

## Intent

- **Why:** feedback on how each flight was flown, and motivation to keep flying it well. A running average of the
  scores is there for that motivation.
- **For whom:** Dave, reading the Flight Summary after block in.
- **How it judges:** BlockTime's own code runs fixed checks on what the recorder logged and computes two scores. Claude
  reads the same facts plus the ATC log and writes the review: why things went the way they did, and what to work on.

### Goals

- Every check is a fact the recorder logged, judged by a written rule. The same flight always gets the same score.
- The scoring rules can be tuned at no cost: scores are computed from stored measurements whenever they're shown, so a
  changed rule re-scores every flight, and a replay tool runs the checks on a saved event log without flying.
- A landing section with its own **0–5 star** rating ("landing details and an overall landing score would be cool").
- Claude's text connects cause and effect, which code can't (an autopilot disconnect, then a sink, then a firm
  touchdown), and reads the ATC log.

### Non-goals

- ATC compliance in the score. SayIntentions sometimes clears Dave to something wrong, which he ignores, so compliance
  is information in Claude's text only, for now (Dave).
- Debriefs for flights flown before this milestone: their logs lack the new cockpit data.
- Regenerating Claude's text for a flight once it's written.
- Other aircraft than the PMDG 737 and 777 families (D-039).

### Anti-goals

- **The score touching the airline.** No money, no consequences, no penalties (design rule 2, D-072).
- **Scoring against scenery.** Centreline, heading and touchdown-zone figures stay information only (D-122).
- **Marking down a go-around.** Going around from an unstable approach is the right call.
- **A flight that can't settle because Claude is unreachable.** The debrief text is extra; checks and scores never
  need it.
- **A number that drifts.** Claude never sets a score.

### Constraints

- The recorder reports facts; judging is the app's (recorder ground rules). Log first (D-008).
- Light on the sim: the new values are change-based, like the probe's; only touchdown samples fast, as today (D-125).
- The database rules in `CLAUDE.md` (D-078, D-090). History is facts, written once.
- Secrets in `settings.json` only, never logged (the Claude API key joins `AppSettings::secrets()`).
- The 737's seat belt sign, the 737's retractable landing lights, and VREF on both families exist only in PMDG's SDK
  broadcast (`docs/notes/probe-flight.md`). Dave accepted that BlockTime reads it on every flight (D-159): a build
  without PMDG's headers marks those checks "not checked".

## Approaches

| | What it is | Trade-off | |
|---|---|---|---|
| **A. Code scores, Claude explains** | BlockTime runs every check and computes both scores. Claude gets the checks, the timeline and the ATC log and writes the review. | Repeatable, testable scores; tuning costs nothing; the app works without Claude. Claude doesn't set the score (changes D-040). | **Chosen (Dave)** |
| B. Claude sets the overall score | As A, but Claude gives the 0–100. | Closest to D-040 as written; the number can differ for flights flown the same way, which moves the running average for no reason. | |
| C. No Claude | Code-only debrief. | Free, but no reading of the ATC log and no insight. | |

A and B cost the same (about $0.12–0.16 a flight on Opus 5.5). A spike on the KLAS → KSAN leg of 2026-11-24 wrote
both versions from the real log; Dave chose A with **Claude Opus 5.5** (`claude-opus-5-5`). Dave's point: A also lets
the scoring be tuned without paying for more Claude calls.

## 1. What the recorder adds

All of it goes in the event log as facts; nothing is judged here.

- **A `cockpit` event**, written whenever one of its values changes (at most once a second, like the probe's):
  - `landing_lights` (bool): any landing light on. The 777 and the 737's fixed lights from `LIGHT LANDING`; the 737's
    retractable lights from PMDG's `LTS_LandingLtRetractableSw` (on if either is on). They only work with PMDG's
    non-LED lights package; Dave flies the LED package, where the landing lights are on/off only. The runway turnoff
    lights, which Dave uses for taxi, don't count.
  - `seat_belts` (0 off, 1 auto, 2 on): the 737 from PMDG's `COMM_FastenBeltsSelector`; the 777 from
    `CABIN SEATBELTS ALERT SWITCH` (0/1, logged as 0 or 2).
  - `gear_down` (bool): from `GEAR TOTAL PCT EXTENDED` (fully extended). Samples while paused are ignored, because
    `GEAR HANDLE POSITION` reads 0 then (`probe-flight.md`); whether the percent does is checked in the sim.
  - `vref_kt` and `landing_flaps`: the FMC's `FMC_LandingVREF` and `FMC_LandingFlaps` on both families (0 when not set).
  - `autopilot` (bool): any autopilot engaged (the 737's `MCP_annunCMD_A`/`B`; the 777's AP engaged annunciator), so
    the review can tie a disconnect to what followed. Not checked or scored.
  - `apu_switch` and `apu_running` (bool, 0.11.1, D-172): the APU selector away from OFF (737 `APU_Selector`, 777
    `ELEC_APU_Selector`), and the APU running (777 `APURunning`; 737 the selector on and its EGT at 300 °C or more,
    since PMDG's 737 data has no running flag). The plain `APU SWITCH` never moves on either family.
  - A field the build can't read (no PMDG headers) is left out of the event, never written as 0.
- **Beacon, strobes, flaps, airspeed and altitudes** are already in `telemetry`; **the altimeters** in `altimeter`
  (D-123); the ATIS and the ATC log since 0.8.0.
- **`touchdown` gains** `sim_pitch_deg`, `bank_deg`, `heading_true_deg`, `wind_dir_deg`, `wind_kt` (from
  `PLANE PITCH DEGREES`, `PLANE BANK DEGREES`, `PLANE HEADING DEGREES TRUE`, `AMBIENT WIND DIRECTION`,
  `AMBIENT WIND VELOCITY` in the fast touchdown samples). The pitch is logged raw: the SDK documents nose up as
  negative, which the sim check confirms before the landing rating relies on it.
- **A `bounces` event** 15 s after touchdown: how many times the wheels left the ground again for at least 0.5 s. Fast
  sampling stays on until then (Claude's call: a separate event, so the touchdown line on Live isn't delayed).
- **`cockpit` carries `pmdg`** (whether PMDG data arrived this session), so a missing value reads as "no PMDG data",
  not as "off". **A `cockpit` snapshot is written at block out** (as built, D-162): the lines are change-based, so
  after a long turnaround the state at the push would otherwise be older than the 30-minute lead-in.
- **The pitch is stored as the sim gives it** (`sim_pitch_tenths`); the scoring flips it (as built, D-162), so if the
  sim check shows the sign the other way round, one constant changes and every flight re-scores.
- **The PMDG reader** moves out of `--probe`: a small normal-running part subscribes to the 737's or the 777's
  broadcast (whichever is loaded) and reads only the fields above. `--probe` keeps logging everything as today.

## 2. The checks

Each check is measured once, from the log, when the flight settles. The window, the measurement and the rule that
passes it are below. All tolerances are Claude's proposals (D-160) and are meant to be tuned.

| # | Check | Measured | Passes when |
|---|---|---|---|
| 1 | Beacon on before pushback | seconds the beacon came on after the earlier of block out and the first engine start | ≤ 10 s |
| 2 | Seat belt sign on from taxi to the transition altitude | seconds it was off between the start of taxi out and `transition_altitude` (D-200; flights settled before 0.15.0 were measured to `above_10000` and keep it, D-160) | 0 |
| 3 | Taxi speed | highest ground speed in taxi out and taxi in | ≤ 30 kt |
| 4 | Origin altimeter | at the takeoff roll, both altimeters' difference from the latest origin ATIS (or the one before it), tenths of hPa | ≤ 10 (1 hPa) |
| 5 | Lights for takeoff | strobes and landing lights 10 s into the takeoff roll (as built: lights switched on as thrust comes up count) | both on |
| 6 | 250 kt below 10,000 ft | seconds above 260 kt IAS while airborne below 10,000 ft indicated | ≤ 10 s |
| 7 | STD by the transition altitude | seconds both altimeters reached STD (1013.2 hPa) after the `transition_altitude` marker | ≤ 60 s |
| 8 | Seat belt sign on for the descent | seconds off between the `transition_level` marker and block in | 0 |
| 9 | Destination altimeter | both altimeters' difference from the latest destination ATIS 60 s after `transition_level`, tenths of hPa | ≤ 10 |
| 10 | Landing lights below 10,000 ft | seconds off between `below_10000` + 60 s and touchdown | 0 |
| 11 | Stable at 1,000 ft AGL | at the landing approach's `final_approach` marker: gear down; flaps at the FMC's landing setting; sink; IAS minus VREF; lined up | gear down, landing flaps, sink ≤ 1,000 fpm, VREF −5 to +20 kt, a `lined_up` before the marker |
| 12 | Strobes off after landing | seconds after taxi in began until the strobes went off | ≤ 180 s |
| 13 | APU off by the transition altitude (0.11.1, Dave) | seconds after the `transition_altitude` marker until the APU selector was off | 0 |
| 14 | APU on at the gate (0.11.1, Dave) | whether the APU selector was on when the last engine stopped at block in (Dave may switch it on only once parked, D-173) | on; a miss costs 2 points, not 6 |

Rules that apply to every check:

- **Not checked** when its data isn't in the log (no PMDG headers, VREF never entered, no ATIS logged, no transition
  marker on a short hop). On a 737 without PMDG data that includes the landing lights (checks 5 and 10), since
  `LIGHT LANDING` misses the retractable ones. A not-checked check counts neither way and the summary says why. The reason is stored with
  the flight (section 7), so it's still shown once the log is gone.
- **Stable approach parts** (check 11) are stored separately and shown separately, but score as one check: missed if any
  part is missed. A part whose data is missing is not checked; the rest still count.
- **A go-around:** checks 10 and 11 use the approach that ended in the landing (check 10's window starts again at
  the go-around). The earlier approach is shown as
  information ("Went around at 600 ft") and never marked down.
- **A diversion:** check 9 is not checked unless the ATIS logged is for the airport landed at.
- **STD** is read as 1013.2 hPa on the millibar values, because `KOHLSMAN SETTING STD` never moves on PMDG
  (`msfs-observed-behaviour.md`). A real QNH of exactly 1013.2 at the transition altitude would pass by luck; that's
  accepted (rare, D-099).
- Flap detents map to degrees per family for check 11 (737: 0, 1, 2, 5, 10, 15, 25, 30, 40; 777: 0, 1, 5, 15, 20, 25,
  30), checked against `flaps_handle` from the probe flights.

## 3. The landing

Shown in its own section: sink rate, G, bounces, pitch, bank, speed against VREF, wind and its crosswind component
(from the `landing_runway` heading), plus the centreline and heading offsets as information (D-122).

**Stars (0–5, whole stars, Claude's proposal, D-158):**

1. **From the sink rate:** ≤ 240 fpm ★5; ≤ 360 ★4; ≤ 600 ★3; ≤ 900 ★2; ≤ 1,200 ★1; above ★0.
2. **Crosswind of 15 kt or more:** one star back (at most 5), so a firm crosswind landing is judged as one.
3. **Minus one each** for: a bounce (each), bank above 5°, pitch outside 0° to 8° (737) or 0° to 7.5° (777), speed
   outside VREF ±10 kt.
4. **G caps it:** 1.7 G or more at most ★2; 2.0 G or more at most ★1.
5. Never below 0. Without VREF the speed rule is skipped; without pitch or bank, those rules.

The spike's landing (591 fpm, 1.47 G, no bounce, 6 kt crosswind against the sim's heading, at VREF) gets ★3.

## 4. The score

**0–100, from a formula, never stored (D-160):** 100, minus 6 for each missed check (2 for the APU at the gate,
D-172), minus the landing's share
(★5 0, ★4 4, ★3 8, ★2 14, ★1 20, ★0 30), never below 0. The spike's flight misses checks 1, 2, 6, 7 and 10 and
lands ★3: 100 − 30 − 8 = **62**. (The spike itself missed check 6: measured properly, the descent passed 10,000 ft at
275 kt and stayed above 260 kt for 17 s.)

- **Measurements are stored, not results.** A flight's rows hold what was measured (163 s, 591 fpm). The rules
  (tolerances, stars, points) live in one code file and run whenever a score is shown, so changing a rule re-scores
  every flight and the averages with it.
- Changing *how* something is measured (a new window, a new bounce rule) applies to flights settled after the change.
  Stored measurements are never rewritten: the replay tool shows what the new measurement would give on any flight
  whose log still exists (30 days), so a change can be judged before it's made.

## 5. Claude's review

- **When:** right after the flight settles (at block in, or at startup for a flight settled then), in the background.
  The Flight Summary shows "Writing the debrief…" until it arrives.
- **Input** (one request, built in plain C++ so the replay tool can print it):
  - the flight in one line each: route, aircraft, block times, the planned cruise level;
  - a timeline of the flight's events: phases, markers, level-offs, `cockpit` changes, altimeter changes, liftoff,
    touchdown and the runways, plus derived lines (the state at 1,000 ft, the sink from 1,000 ft to touchdown, the
    autopilot's disconnect when the PMDG data has it; in the air each cockpit change gives its height, and each spell
    above 260 kt below 10,000 ft has its own line). As built, it opens at the engine start or block out with one
    line giving the cockpit state then: the 30-minute lead-in can reach the previous leg's landing (D-162);
  - each check with its measurement and result, and the landing with its stars;
  - the ATIS texts, once per letter;
  - the ATC log: `COM` channels only (cabin crew and PA are on `INTERCOM`), each line labelled with the station and
    whether it was Dave's side (`incoming_message`) or the station's (`outgoing_message`); the field names read the
    other way round (`docs/notes/sayintentions-api.md`);
  - the missed checks of the last five debriefed flights, so it can point out habits.
- **The instructions** ask for: what went well, what to work on (most important first), the landing explained, and ATC
  compliance as information (clearances against what was flown, saying when an instruction looks wrong or late rather
  than calling it a bust). No score numbers in the text, so it stays true after a re-score. A few hundred words. As
  built, the instructions also carry Dave's corrections from the sim test (D-171): the checks are the verdict, ATC
  only in its own part, approach level-offs can be FMC constraints, no remarks on frequencies, normal flare bleed-off.
- **Output:** structured JSON (`output_config.format`): `summary`, `work_on` (a list), `landing`, `atc`. Stored once
  with the flight, plus the model id and the time.
- **The call:** the Messages API over HTTPS from Qt (there's no official C++ SDK), `claude-opus-5-5`, adaptive thinking,
  `effort` set explicitly (start at `medium`, tune with the replay tool), `max_tokens` 16,000, a timeout of about 5
  minutes. Server-side refusal fallbacks are on (`fallbacks: "default"`). A refusal, an HTTP error or a timeout stores
  nothing.
- **A debrief that can't be measured or written never stops the flight settling** (as built, D-162): the money is
  settled and the flight shows no debrief.
- **One automatic try, at settling.** If it fails, the summary shows "Debrief not written" with the reason and a
  **Retry** button. The reason is kept only while the app runs; after a restart, or if the app closed while the call
  was running, a flight with checks and no review just shows "Debrief not written" and Retry. Nothing retries on its
  own, so a failing call never spends money unseen.
- **No key** in `settings.json` (`claude_api_key`): "Add a Claude API key to write debriefs", with the checks and
  scores shown anyway.
- **Cost:** estimated at 8,000–15,000 tokens in and 3,000–5,000 out ($0.12–0.16 a flight). **Measured** on the
  KLAS → KSAN leg: about 6,000 in and 1,500–1,900 out, **about $0.05 a flight** (`docs/notes/claude-debrief.md`).

## 6. Where it shows

**The Flight Summary** (Live at block in, and Flights for any flight with checks):

```
KLAS → KSAN · SOX8 · N814SX                         Score 68 · Landing ★★★☆☆

Debrief   A tidy flight from the runway onward, with the misses bunched where workload is high…
          To work on: 1. Beacon and seat belts before calling for push  2. Landing lights passing 10,000 ft
          Landing: CMD A off at 500 ft, sink peaked at 1,016 fpm near 230 ft, touchdown at VREF…
          ATC (information): levelled at FL360 as cleared; "maintain 5,300" came while passing 5,262…

Checks    ✗ Beacon on before pushback          163 s after block out        −6
          ✗ Seat belt sign, taxi to trans. alt off 13 min                   −6
          ✓ Taxi speed                         25 kt
          ...
Landing   591 fpm · 1.47 G · no bounce · pitch 5.1° · bank 0.8° · VREF +0 · wind 330/9 (8 kt cross)   ★★★☆☆ −8
          2.7 m off the centerline, 0.6° off the heading (information)
```

The money lines and the map stay as they are; the layout is the builder's within Theme.qml (D-150). **As built
(D-162):** the score and stars sit in the summary's header, and a **Debrief** button swaps the times, money and map for
the debrief panel (review, checks, landing), since the summary is only about 520 px wide on Flights.

**The Flights screen** gets a strip above the list:

```
Average score  71  (last 10: 74 ▲)          Landings  ★★★☆☆ 3.4  (last 10: 3.8)
```

Averages over the flights that have checks. The ▲/▼ compares the last 10 with the all-time figure.

## 7. Storage (migration 7)

The builder designs the tables in `docs/database.md` first, following D-078. The shape:

- one row per measured check part per flight (the flight, the part from a lookup table of check parts, the measured
  integer);
- one row per check part that couldn't be measured (the flight, the part, the reason from a lookup table: no PMDG
  data, no VREF, no ATIS, no marker, diverted). Each part of a flight from milestone 10 on is in exactly one of the two;
  a flight in neither had no checks (flown before milestone 10);
- the landing's measurements, one row per flight, columns for each value (a value that couldn't be read is a missing
  column only where D-090 allows it, otherwise a separate row);
- the review, one row per flight once written: the four text fields (`work_on` as child rows, not a list in a
  column), the model id, the time;
- all written once and never updated.

## 8. The replay tool

`blocktime-debrief.exe`, built with the app from the same core library:

- `--log-dir <dir> --flight <flight_id>`: measures the checks from a saved event log and prints each with its result,
  the landing and its stars, and the score;
- `--prompt`: also prints the exact request Claude would get (no call, no cost);
- `--claude`: makes the call and prints the review (costs, needs the key).

The spike's KLAS → KSAN log becomes a trimmed test fixture, so the checks are tested against a real flight.

## Calls made by Claude (for Dave to confirm)

- Every tolerance in the checks table, the star bands and the score's points (D-160, D-158). Tunable at no cost.
- Seat belts as one check from taxi to the transition altitude (10,000 ft until D-200), so one slip isn't counted twice.
- AUTO on the 737's seat belt selector counts as on (Dave never uses it on the 737, and on the 777 only in a
  freighter, 2026-11-24).
- Measurements stored and scores computed on display (D-160), and no score numbers in Claude's text.
- Structured JSON from Claude, with four sections.
- The last five flights' misses as Claude's habit memory.
- One automatic try at settling, then the Retry button; no retries on their own across restarts.
- The not-checked reason stored with the flight.
- Stored measurements never rewritten; the replay tool shows a changed measurement before it's adopted.
- The autopilot logged in `cockpit` for the review (not scored).
- The replay tool (`blocktime-debrief.exe`).

## Left to the builder

- The exact tables, their names and the check-part lookup (with `docs/database.md` first, D-078).
- The prompt's wording and the timeline's format, tuned with the replay tool.
- The Flight Summary's layout and the strip's look, within Theme.qml.
- How the PMDG reader is split between normal running and `--probe`.

## Checked in the sim (Dave, in milestone 10's sim tests)

- The BBJ2's landing lights reach `landing_lights` and its runway turnoff lights don't; the 777's lights through
  `LIGHT LANDING`.
- `GEAR TOTAL PCT EXTENDED` while paused.
- The sign of `PLANE PITCH DEGREES` at touchdown, and that `bounces` counts a real bounce.
- VREF and landing flaps read from the 777's FMC in normal running (seen in the probe: VREF 144, flaps 30).
