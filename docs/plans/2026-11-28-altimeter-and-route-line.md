# The destination altimeter and the route line: implementation plan (0.18.1)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (CLAUDE.md, D-033: no subagent-driven
> development here). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dave's requests after the KBFI → KPDX delivery (flight 9, 2026-11-28):
1. The destination altimeter check counts the seconds late, the way the STD check does.
2. The header's route arc becomes a flat line. Its dots stay where they are, with more of them. The flown part and
   the airplane glide over 1 s, like the gauge needles.
3. The destination altimeter shows on the in-flight panel from 200 nm out, as its own readout. On a flight shorter
   than 200 nm it shows from liftoff.

**Architecture:** (1) is a new stored check part, `destination_baro_late_s`, added by migration 13. Flights already
settled keep `destination_baro_off_tenths` and the old rule, because stored measurements are never rewritten
(debrief.md section 4). (2) is QML only. The dots and the coloured line are drawn on Canvases that repaint only when
their data changes. The glide moves only a clip width and the marker's `x`, so no Canvas repaints on every frame.
(3) has two parts: in the recorder, `within_200nm` fires on the first airborne sample within 200 nm, not only when
the flight crosses 200 nm. In core, `inFlightReadouts` gets a DEST ALTIMETER readout.

**Tech stack:** C++20 with GoogleTest, Qt 6 QML, and SQLite migrations (`src/airline_db.cpp`).

**Spec:** `docs/debrief.md` (section 2, check 9), `docs/dashboard.md` ("Route progress" and the in-flight readouts
table), `docs/recorder.md` (the `flight_marker` row).

## Global constraints

- No warnings at `/W4 /permissive- /utf-8`. `blocktime-app_qmllint` reports nothing new (ReplayBar's old info line
  stays). Every test passes in debug and release.
- Every colour, size and duration lives in `qml/Theme.qml`. The glide uses `Theme.needleGlideMs` (1000) with linear
  easing, the same as `GaugeNeedle.qml`.
- The migration is numbered and runs in a transaction. Tests build an empty database through every migration. The
  app makes the backup `blocktime.before-schema-13.db` itself.
- Never rewrite history: flight 9 keeps its 8.5 hPa miss and the review Claude wrote for it.
- Version 0.18.1 in `CMakeLists.txt`. Build `x64-release` before telling Dave it's ready (Dave runs the release exe).
- Decisions: D-254 (the check), D-255 (the route line and its glide, an exception to "no continuous animations"
  alongside D-195 and D-205), D-256 (DEST ALTIMETER, and `within_200nm` on a short hop).

## Review focus

- **Flights already settled score as before:** flight 9 still scores 88 with "8.5 hPa off". Test
  `DebriefScore.OldDestinationBaroPartStillScores` (Task 1).
- **The ATIS changes letter during the descent:** a setting still within 1 hPa of the ATIS before it counts as set.
  Test `DebriefMeasure.DestinationBaroLateAcceptsTheAtisBefore` (Task 1).
- **An altimeter set before any destination ATIS is logged** (from ATC's value): it counts from when the ATIS
  arrives, and is never skipped as `no_atis` if an ATIS turns up before block in. Test
  `DebriefMeasure.DestinationBaroLateWaitsForTheAtis` (Task 1).
- **A replay seek or a window resize:** the line and the airplane glide (a seek) or jump (a resize, because the
  share stays the same), and no Canvas repaints on every frame. Checked by hand with `--demo` and `--grab`
  (Task 2, step 4).
- **The recorder restarting mid-flight inside 200 nm:** `within_200nm` fires again on the first sample (one more
  log line). FlightWatcher's `!destinationTimer_.isActive()` guard keeps the ATIS polling single. This is accepted
  under D-174 and noted in D-256; no test.

---

### Task 1: Destination altimeter, in seconds late (D-254)

**Files:**
- Modify: `src/debrief_measure.h` (the part list and its comment), `src/debrief_measure.cpp` (`destinationBaro()`)
- Modify: `src/debrief_score.h/.cpp` ("Destination altimeter")
- Modify: `src/airline_db.cpp` (migration 13: `INSERT INTO check_part (code) VALUES ('destination_baro_late_s')`)
- Create: `tests/data/flight-kbfi-kpdx/events-2026-11-28.jsonl` (flight 9's lines from Dave's log, 16:56:38 to
  18:05:38Z; add it to `tests/data/README.md`)
- Test: `tests/debrief_measure_test.cpp`, `tests/debrief_score_test.cpp`, `tests/airline_db_test.cpp`
- Docs: `docs/debrief.md` (check 9's row and section 4), `docs/database.md` (schema 13),
  `docs/notes/claude-debrief.md` (what went wrong on flight 9), D-254

**Interfaces:**
- Produces: the check part `destination_baro_late_s` (int, seconds). `kCheckParts` keeps
  `destination_baro_off_tenths` for old flights, and `measureFlight` no longer writes it.

- [ ] **Step 1: Write the failing tests**
  - `DebriefMeasure.KbfiKpdxDestinationBaroLate`: on the new fixture (flight `2026-11-28T17:22:56.027Z`), the value is
    `destination_baro_late_s` = 84 (transition level 17:40:11.7, both altimeters at 1021.7 at 17:41:35.8;
    30.17 inHg = 1021.7 hPa), and the result has no `destination_baro_off_tenths`.
  - `DebriefMeasure.DestinationBaroLateZeroWhenSetBefore`: set to the ATIS's QNH before the marker gives 0.
  - `DebriefMeasure.DestinationBaroLateAcceptsTheAtisBefore`: ATIS A 1021.7, set to 1021.7 at TL+20, ATIS B 1023.0
    at TL+40 gives 20.
  - `DebriefMeasure.DestinationBaroLateWaitsForTheAtis`: set to 1021.7 at TL+10, the first destination ATIS (30.17)
    at TL+50 gives 50.
  - `DebriefMeasure.DestinationBaroLateNoAtis`: no destination ATIS before block in gives skip `no_atis`. Diverted
    and no-marker are skipped as today.
  - `DebriefScore.DestinationAltimeterLate`: 84 is missed, −6, "1 min 24 s after the transition level". 60 passes.
    0 passes with "Set by the transition level".
  - `DebriefScore.OldDestinationBaroPartStillScores`: `destination_baro_off_tenths` = 85 is missed with
    "8.5 hPa off", and 10 passes.
  - `AirlineDb`'s migration test: schema 13, and `check_part` contains `destination_baro_late_s`.
  - Update the KLAS → KSAN fixture's expectations wherever they named `destination_baro_off_tenths`.

- [ ] **Step 2: Run them and see them fail.** Run
  `ctest --preset x64-debug -R "DebriefMeasure|DebriefScore|AirlineDb"`. The new tests are expected to fail.

- [ ] **Step 3: Implement**
  - `destinationBaro()`: the candidate times are the `transition_level` marker, then every `altimeter` line and
    every destination `atis` line after it, up to `end()`, in order. The value is the seconds from the marker to
    the first candidate `t` where `baroOff("destination", t) <= 10` (it already picks the ATIS in force at `t` or
    the one before). If none passes, the value runs to `end()`. If no destination ATIS is logged by `end()`, skip
    with `no_atis`. `kDestinationBaroDelay` goes.
  - The scorer: if `destination_baro_late_s` is stored, pass at ≤ 60 s (a new `kDestinationBaroLateS = 60`, next to
    `kStdLateS`), with the detail "Set by the transition level" for 0 or "<duration> after the transition level".
    Otherwise use today's `baro("destination_baro_off_tenths")`. The skip reasons are read from whichever part was
    stored.
  - Migration 13 inserts the part. `PRAGMA user_version` becomes 13.

- [ ] **Step 4: Run all the tests and see them pass.** Then run `blocktime-debrief --log-dir
  "%LOCALAPPDATA%\BlockTime\events" --flight 2026-11-28T17:22:56.027Z --prompt`. Expected:
  `✗ Destination altimeter  1 min 24 s after the transition level  −6`. The score is still 88, and Claude's input
  says "missed (1 min 24 s after the transition level)".

- [ ] **Step 5: Write the docs and D-254, then commit.** Commit message: "Destination altimeter counts the seconds
  late, like STD (D-254)".

### Task 2: The route line: flat, fixed dots, a 1 s glide (D-255)

**Files:**
- Modify: `qml/FlightHeader.qml` (the `arc` Canvas and the marker, about lines 272–353)
- Modify: `qml/Theme.qml` (`arcRise` goes; add `arcDotSpacing: 10`, the px between dot centres, about 2–3 times
  today's 50 dots at 1920 px)
- Docs: `docs/dashboard.md` ("Route progress": a flat line, fixed dots, the glide), D-255, and CLAUDE.md's Qt
  paragraph (add the route line to the list of animation exceptions)

**Interfaces:** QML only. `DashboardHeader.progress`, `arcShares` and `todShare` stay as they are.

- [ ] **Step 1: Make the line straight.** `pointAt(t)` returns `(x0 + t·(x2 − x0), yEnds)`, and `yControl` goes.
- [ ] **Step 2: Fix the dots in place.** A `dots` Canvas draws a dot every `Theme.arcDotSpacing` from `x0` to `x2`,
  along the whole line. It repaints only when `x0`, `x2` or `yEnds` change. The flown line (`arcWidth` 5) is drawn
  above it and covers the dots (`arcDotSize` 3) as it fills in.
- [ ] **Step 3: Add the glide.** Add `property real shown: card.header.progress` with
  `Behavior on shown { enabled: ready; NumberAnimation { duration: Theme.needleGlideMs; easing.type: Easing.Linear } }`
  (`ready` is set after the first frame, as `markerRow` does, so the page doesn't slide in from 0).
  - The flown line is a Canvas inside an `Item { clip: true }` whose width follows `shown`. It starts
    `arcWidth / 2` before `x0`, so the round start cap stays.
  - The Canvas paints every bucket up to `ceil(progress · n)`. It repaints when `arcShares` changes or that bucket
    count changes, so never on every frame.
  - The airplane marker's position follows `shown`, and the TOD tick becomes a Rectangle at its share.
  - The airplane covers the clipped square end of the line.
- [ ] **Step 4: Check it.**
  - qmllint is clean.
  - `--grab` pictures: the empty line before block out, partway along in the demo, and the 1300 px window.
  - Hot reload with `--demo` (and the replay at 10×): the dots stay still while the line covers them, and the
    airplane glides steadily.
  - Task Manager shows the app's CPU about where it was with the needles alone (under 1% in flight).
- [ ] **Step 5: Write the docs and D-255, then commit.** Commit message: "Route line: flat, fixed dots, glides over
  1 s like the needles (D-255)".

### Task 3: DEST ALTIMETER from 200 nm, and `within_200nm` on a short hop (D-256)

**Files:**
- Modify: `src/phase_detector.cpp:291-295` (fire when airborne and within 200 nm; `before` goes)
- Modify: `src/inflight_readouts.cpp` (Climb and Cruise)
- Test: `tests/phase_detector_test.cpp`, `tests/inflight_readouts_test.cpp`, `tests/qt_models_test.cpp` if it counts
  the readouts
- Docs: `docs/recorder.md` (the `flight_marker` row), `docs/dashboard.md` (the Climb and Cruise rows), D-256

**Interfaces:**
- Consumes: `InFlightFacts::destinationAtis`, plus `altimeterText` and `valueAndUnit` (already in
  `inflight_readouts.cpp`).
- Produces: `ReadoutView{title "DEST ALTIMETER", value "30.17", unit "IN", tag "SET AT TL"}` (in hPa wherever the
  ATIS is).

- [ ] **Step 1: Write the failing tests**
  - `PhaseDetector.ReplaysKbfiKpdx` (117 nm) now expects `marker:within_200nm` at the replay's first airborne sample
    (AGL ≥ `kAirborneMinAglFt`, within seconds of liftoff). Read the time from the failure and check it against the
    CSV. `ReplaysKpdxKsfo` keeps `00:48:47`.
  - `InFlightReadouts.CruiseShowsDestAltimeter`: with the destination ATIS (`altimeter` "30.17"), the readout right
    after DEST ATIS is DEST ALTIMETER 30.17 IN, tag SET AT TL. Without the ATIS it shows a dash with no tag, as
    `CruiseBeforeDestAtis` does for DEST ATIS.
  - `InFlightReadouts.ClimbShowsDestAltimeterOnceIn`: Climb has no DEST ALTIMETER without the ATIS, and has it at the
    end once the ATIS is in.
  - Update `Cruise` and `CruisePredictions` for the new order: SOULS, FUEL REMAINING, CRUISE, DEST ATIS,
    DEST ALTIMETER, TOP OF DESCENT, LANDING, FUEL AT LANDING, ARR GATE.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement.** In `markers()`:
  `fire(MarkerKind::Within200Nm, distanceToDestinationNm(s) <= kMarkerRangeNm)`, inside the existing airborne
  guard. Add the readout as above. Cruise has 9 readouts, which wraps to a third row (Dave chose this
  2026-11-28). `readoutSlots` (14) is still enough.
- [ ] **Step 4: Run all the tests and see them pass.** Check a Cruise `--grab` picture with three rows.
- [ ] **Step 5: Write the docs and D-256, then commit.** Commit message: "DEST ALTIMETER from 200 nm, and
  within_200nm on hops under 200 nm (D-256)".

### Task 4: Release, review, progress

- [ ] Bump `CMakeLists.txt` to 0.18.1. Build `x64-debug` and `x64-release` with no warnings, and run every test in
  both.
- [ ] Do a dry run of migration 13 with the release build on a scratch copy of Dave's airline (`--demo`/`--grab`
  copy, D-226): schema 12 → 13, `foreign_key_check` empty, `integrity_check` ok, and flight 9 still scores 88.
- [ ] Get one fresh review of the diff (superpowers:requesting-code-review, then receiving-code-review). Fix what's
  real.
- [ ] Write a `docs/progress.md` entry: what's done, and what Dave should see on his next flight (DEST ALTIMETER
  after liftoff on a short hop, the line gliding, the check reading in seconds). Push `main`.
