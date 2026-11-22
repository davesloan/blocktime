# Recorder Milestone 3 (Ending Flights Cleanly) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every flight that starts with `block_out` ends with exactly one `block_in` or `flight_interrupted`, even
when Dave exits to the main menu, the recorder is killed, or a log write fails. No flight is ever resumed (D-046).

**Architecture:** Three small additions:
- The `SimConnection` reports the sim's `Sim` running/stopped state to `main.cpp`. A stop with a flight open ends
  it as interrupted.
- A new core function scans the event log at startup for a flight a killed recorder left open, and closes it.
- A new core `EventOutbox` holds block events whose write failed, and retries them.

The pure parts live in `blocktime-core` and are unit-tested. `main.cpp` only wires them together.

**Tech Stack:** C++20, MSVC /W4, CMake presets + Ninja, vcpkg (nlohmann-json, gtest), SimConnect (MSFS 2020 SDK).

**Spec:** `docs/recorder.md` (milestone 3, Flight lifecycle, Sim crash / disconnect), `docs/decisions.md` D-009,
D-036, D-037, D-044, D-045, D-046; `docs/notes/msfs-observed-behaviour.md`.

## Context

Milestone 2 detects block out and block in, and logs `flight_interrupted` when the sim session ends or the
recorder exits. Dave has now decided (D-046) that a flight is never resumed. Exiting to the main menu means the
flight is over, and the web app discards or completes interrupted flights (D-009). Three holes are left:
1. **Main menu:** an open flight stays open after Dave exits to the main menu, and swallows the next flight's push.
2. **Recorder killed / power cut:** nothing writes `flight_interrupted`, so the log has a `block_out` with no end.
3. **Failed write:** a block event that fails to write is lost; only a stderr line says so.

**Main-menu signal, from the 2026-11-22 logs:**
- Every exit to the main menu logged `sim_stopped` within about 1 s (17:07:54, 06:26:40, 07:05:08).
- The Escape menu (pause flag 8) and active pause (flag 4) never did.
- The first junk sample sometimes came about 60 s after the exit (17:08:52), so the junk filter is the wrong
  signal.
- `sim_stopped` also flips several times while a flight loads, but no flight is open then.

## Global Constraints

- Builds with no warnings at `/W4 /permissive- /utf-8`, in both release and debug. All tests pass.
- Every new source file starts with the GPL header used in `recorder/src/*.cpp`.
- The core library and tests must not include `windows.h` or `SimConnect.h`.
- Log line format is unchanged (D-020): `{"ts","type","data"}`. Event types and fields are unchanged; only new
  `reason` values are added.
- Only the PMDG 737/777 on MSFS 2020 (D-039).
- Recorder version becomes **0.3.0** (`CMakeLists.txt` and `vcpkg.json`).
- Commits go straight to `main` (D-038). Messages explain why and cite D-numbers. They end with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Don't push until Dave says so.

## Rules (decided here; logged as Proposed in Task 4)

- **D-047, main menu:** when the sim's `Sim` state changes to stopped and a flight is open, log
  `flight_interrupted` with reason `sim_stopped`, then reset the detector like a lost session (D-044). This also
  covers restarting a flight and Escape → Quit, which go through the same state. A stop with no flight open
  resets the detector too (same effect as the D-045 invalid stretch).
- **D-048, recorder killed:** at startup, after the single-instance check (D-037) and after `recorder_started`,
  scan the log for an open flight:
  - Look at files named `events-YYYY-MM-DD.jsonl`, newest first, at most **7** files.
  - In the newest file that has any `block_out`, `block_in` or `flight_interrupted` line, take the last one. If it
    is a `block_out`, that flight is open: log `flight_interrupted` with its `flight_id` and reason
    `recorder_crash`, and print a console line.
  - Lines that don't parse (a torn last line, D-036) are skipped.
  - **Known gap:** if the sim is still in that flight, the restarted recorder starts from scratch. A later brake
    set, release and 10 m of taxi opens a new flight there, which the main menu or session end then interrupts.
    It only follows a recorder crash, so it's accepted.
- **D-049, failed writes:** block events (`block_out`, `block_in`, `flight_interrupted`) go through an
  `EventOutbox`:
  - A failed write is held, with a console warning that shows the event's JSON.
  - Held events are retried every 5 s, oldest first. A new event waits behind them, so the order is kept.
  - At exit, the outbox is flushed one last time. Anything still held is printed to stderr as `LOST:` with its
    JSON.
  - The line's `ts` is when it was finally written; the event's own times are in `data` (D-043).
  - Telemetry and connection events are not retried.

## Review Focus

1. **Second recorder:** a second copy must not close the running copy's open flight. The scan runs only after the
   mutex check. Checked by code order in Task 2 and sim step 5.
2. **Flight across UTC midnight, then a crash:** `block_out` is in yesterday's file and today's has only
   telemetry. Must still be found. Pinned in Task 2.
3. **Torn last line after a crash:** a half-written line after `block_out` must not hide it or throw. Pinned in
   Task 2.
4. **Escape menu, active pause and Ready to Fly** during a flight must not end it. They don't change the `Sim`
   state (logs above). Sim step 2.
5. **Order after a failed write:** held events go out before newer ones, and a flush stops at the first failure.
   Pinned in Task 3.
6. **A junk sample during a flight** still doesn't end it. The existing `Session.InvalidSampleKeepsOpenFlightState`
   must stay green.

---

### Task 1: End an open flight when the sim stops (D-047)

**Files:**
- Modify: `recorder/src/block_detector.h/.cpp`, `recorder/tests/block_detector_test.cpp`,
  `recorder/src/sim_connection.h/.cpp`, `recorder/src/main.cpp`

**Interfaces (produces):**
- `std::optional<FlightInterrupted> BlockDetector::onSimStopped();` returns `onSessionEnded("sim_stopped")`.
- `SimConnection(EventLog& log, SampleCallback onSample, SimStateCallback onSimState = {});` with
  `using SimStateCallback = std::function<void(bool running)>;`. Called from `setSimRunning` only when the state
  actually changes, right after `sim_running`/`sim_stopped` is logged.

- [ ] **Step 1: Write failing tests** (add `void simStopped()` to `Script`, collecting the result into `events`):
  - `Session.SimStoppedInterruptsOpenFlight`: `blockOut()`, `simStopped()` → one `FlightInterrupted` with the
    block-out `flightId` and `reason == "sim_stopped"`; `!flightOpen()`.
  - `Session.SimStoppedWithoutFlightReportsNothing`: `brake(true)`, `simStopped()` → no events.
  - `Session.AfterSimStoppedNextFlightStartsFresh`: `blockOut()`, `simStopped()`, then the first sample has the
    brake off at another airport, then `north(50 m)` → no `BlockOut`. Then set, release and `north(20 m)` → a
    `BlockOut` with a new `flightId`.
- [ ] **Step 2:** Build and run the tests. Expect a compile failure (no `onSimStopped`).
- [ ] **Step 3:** Implement `onSimStopped`. Add the callback to `SimConnection`. In `main.cpp`, pass a callback:
  on `running == false`, `endSession(log, detector, ...)` through `onSimStopped()`.
- [ ] **Step 4:** `ctest --preset x64-release`. Expect all to pass.
- [ ] **Step 5:** Commit: `Recorder: exiting to the main menu ends an open flight (D-046, D-047)`.

### Task 2: Close a flight left open by a killed recorder (D-048)

**Files:**
- Create: `recorder/src/open_flight.h`, `recorder/src/open_flight.cpp`, `recorder/tests/open_flight_test.cpp`
- Modify: `recorder/CMakeLists.txt` (core + tests), `recorder/src/main.cpp`

**Interfaces (produces):**
- `inline constexpr std::size_t kOpenFlightLookbackFiles = 7;`
- `std::optional<std::string> findOpenFlight(const std::filesystem::path& dir, std::size_t maxFiles = kOpenFlightLookbackFiles);`
  returns the `flight_id` of the open flight, or `std::nullopt`. A missing directory gives `std::nullopt`.
  Only lines containing `"type":"block_out"`, `"type":"block_in"` or `"type":"flight_interrupted"` are parsed
  (`json::parse(line, nullptr, false)`).

Test fixture: a fresh directory under `std::filesystem::temp_directory_path()`, removed in `TearDown`. A helper
writes a file from a list of lines; `blockOut(id)`, `blockIn(id)`, `interrupted(id)` and `telemetry()` build log
lines in the real format.

- [ ] **Step 1: Write failing tests** (`OpenFlight.*`):
  - `EmptyOrMissingDirHasNone`.
  - `BlockOutAloneIsOpen` → its id.
  - `BlockInClosesIt` and `InterruptedClosesIt` → `nullopt`.
  - `SecondFlightOpenAfterFirstClosed`: out A, in A, out B → `"B"`.
  - `FoundInYesterdaysFile` (RF 2): `events-2026-11-21.jsonl` has out A. `events-2026-11-22.jsonl` has only
    telemetry → `"A"`.
  - `NewestFileWins`: the 21st has out A, the 22nd has interrupted A → `nullopt`.
  - `TornLastLineIsSkipped` (RF 3): out A, then `{"ts":"2026-11-22T17:0` with no newline → `"A"`.
  - `OtherFilesIgnored`: `notes.txt` that contains a block_out line → `nullopt`.
  - `LookbackIsLimited`: out A in the 1st, then 7 telemetry-only daily files → `nullopt`.
- [ ] **Step 2:** Build and run the tests. Expect a compile failure, then FAIL.
- [ ] **Step 3:** Implement `findOpenFlight`. In `main.cpp`, right after `recorder_started` (which is already after
  the mutex check), call it on `log.dir()`. If it finds a flight, log `FlightInterrupted{id, "recorder_crash"}`
  and print `Previous flight <id> was left open (the recorder didn't close cleanly); logged as interrupted.`
- [ ] **Step 4:** `ctest --preset x64-release`. Expect all to pass.
- [ ] **Step 5:** Commit: `Recorder: close a flight left open by a killed recorder on the next start (D-048)`.

### Task 3: Hold and retry block events whose write failed (D-049)

**Files:**
- Create: `recorder/src/event_outbox.h`, `recorder/src/event_outbox.cpp`, `recorder/tests/event_outbox_test.cpp`
- Modify: `recorder/CMakeLists.txt`, `recorder/src/main.cpp`

**Interfaces (produces):**
```cpp
class EventOutbox {
public:
    using WriteFn = std::function<bool(std::string_view type, const nlohmann::json& data)>;
    explicit EventOutbox(WriteFn write);
    bool send(std::string type, nlohmann::json data);  // queue, then flush(); true if nothing is held after
    bool flush();                                      // write held events oldest first; stop at the first failure
    std::size_t pending() const;
    std::vector<std::pair<std::string, nlohmann::json>> held() const;  // for the LOST: lines at exit
};
```
Tests use a fake `WriteFn` that records calls and fails while a flag is set.

- [ ] **Step 1: Write failing tests** (`EventOutbox.*`):
  - `WritesStraightThroughWhenLogWorks`: `send` → true, `pending() == 0`, one write recorded.
  - `HoldsEventWhenWriteFails`: failing → `send` false, `pending() == 1`.
  - `FlushWritesHeldEventsInOrder` (RF 5): failing, send A, send B; recovered, `flush()` → true. Successful writes
    are A then B.
  - `NewEventWaitsBehindHeldOnes`: failing, send A; recovered, send B → successful writes A then B.
  - `FlushStopsAtFirstFailure`: two held; the fake succeeds once, then fails → `pending() == 1`, the held one is B.
- [ ] **Step 2:** Build and run the tests. Expect a compile failure, then FAIL.
- [ ] **Step 3:** Implement `EventOutbox`. In `main.cpp`:
  - `logBlockEvent` sends through the outbox. If `send` returns false, print
    `WARNING: <type> could not be written to the log; retrying every 5 s. <json>`.
  - The main loop calls `flush()` when `pending() > 0` and 5 s have passed since the last try. On success it prints
    `Held events written to the log.`
  - At exit, after `endSession`, flush once more, then print each still-held event as `LOST: <type> <json>` to
    stderr.
- [ ] **Step 4:** `ctest --preset x64-release`. Expect all to pass.
- [ ] **Step 5:** Commit: `Recorder: hold and retry block events whose log write failed (D-049)`.

### Task 4: Docs, version, review, final commit

- [ ] Bump the version to 0.3.0.
- [ ] `docs/recorder.md`:
  - Mark milestone 3 built.
  - Update "No resume" (the main menu is built) and the `flight_interrupted` reasons (`sim_stopped`,
    `recorder_crash`).
  - Describe the outbox under the event log.
- [ ] `recorder/README.md`:
  - Status.
  - `flight_interrupted` reasons.
  - The `LOST:` / `WARNING:` console lines.
- [ ] `docs/decisions.md`: D-047, D-048 and D-049 (Proposed, decided by Claude).
- [ ] `superpowers:requesting-code-review` on the milestone diff, then `superpowers:receiving-code-review`. Fix the
  accepted findings with RED→GREEN tests, and record the deferred ones.
- [ ] `superpowers:verification-before-completion`: a clean configure and build of release and debug with no
  warnings, and `ctest` all green.
- [ ] `docs/progress.md` entry with the sim checklist below. Commit:
  `Recorder 0.3.0: milestone 3, flights always end (D-047..D-049)`.

## Verification

**Without the sim (Claude):** from a VS dev shell in `recorder/`:
1. `cmake --preset x64-release`
2. `cmake --build --preset x64-release`
3. `ctest --preset x64-release`

Expect no warnings and all tests passing. Do the same with `x64-debug`. Then a smoke run with MSFS closed:
- A log dir holding a lone `block_out` → on start, the `flight_interrupted` `recorder_crash` line and the console
  message.
- A second start → nothing more.

**In the sim (Dave), one MSFS load, two flights from the main menu:** PMDG 737 at a KOAK GSX gate. Start the
recorder first.

*Flight 1: main menu ends it.*
1. Push and taxi ≥ 10 m. Expect `block_out`.
2. Open and close the Escape menu, then turn active pause on and off. Expect no `flight_interrupted`.
3. Escape → exit to the main menu. Expect `sim_stopped`, then `flight_interrupted` with reason `sim_stopped`,
   within a few seconds.

*Flight 2: a killed recorder (load the 737 at a KOAK gate again from the main menu).*

4. Push and taxi ≥ 10 m. Expect `block_out`.
5. Start a second recorder. Expect it to say it's already running and close. The first keeps recording, and there's
   no `flight_interrupted`.
6. In PowerShell, run `taskkill /F /IM blocktime-recorder.exe`. Then start the recorder again. Expect:
   - the console line about the previous flight
   - in the log, `recorder_started`, then `flight_interrupted` with that `flight_id` and reason `recorder_crash`
   - no new `block_out` while you keep taxiing (don't stop and set the brake: see the D-048 gap)
7. Last: exit to the main menu and quit MSFS. Expect `sim_stopped` with no `flight_interrupted` (no flight is
   open), then `sim_disconnected` (`sim_quit`).
