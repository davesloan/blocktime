# Technical review of 0.16.0 (2026-11-27)

The brief is `docs/plans/2026-11-27-technical-review.md`. This is a review, not a refactor: nothing in the code was
changed. Dave decides what gets fixed and when.

## How it was done

- **Build and tests first.** Clean builds of `x64-debug` and `x64-release`: no warnings in either. 953 tests pass in
  each: 105 s in debug, 26 s in release; the slowest are the Qt tests that wait on real timers, about 2 s each in
  debug. `blocktime-app_qmllint` gives one info line: an unused `import QtQuick.Controls.Basic` in
  `qml/ReplayBar.qml:12`.
- **Read by area.** Seven independent reviews ran at the same time, one per area of the brief: money and history; the
  recorder, event log and threading; the database schema; the Qt and QML layer; planning, pricing and the web calls;
  the debrief and cockpit logic; and tests, duplication and docs. Each worked from the code alone, without the
  others' conclusions.
- **Every finding below was checked against the code** before it was written down. Where reviewers disagreed or got
  a line number wrong, the code wins. Each finding is marked **confirmed** (the code path was read end to end) or
  **plausible** (the mechanism is real; how often it fires, or how much it costs, isn't measured).
- No sim test. Dave's event logs were looked at for size only (`%LOCALAPPDATA%\BlockTime\events`: 6 files, 31 MB,
  the largest 17.6 MB).

## Summary

BlockTime is in better shape than most code built this fast. The things that can't be undone are mostly guarded by
the right things:
- the log is written first;
- a flight settles in one transaction;
- `UNIQUE` and `CHECK` constraints make a double settlement or a non-$5 line fail at the database;
- one pure `pricePreview` prices both the preview and the settlement;
- the detectors are pure and tested against real flights.

Nothing in the normal flight path loses money or history. The risks are around that core, and the next stretch of
work walks straight into them.

The five things that matter most:

1. **Development runs use Dave's real airline (1.1, Critical).** The debug build, `--demo` and `--grab` open
   `%LOCALAPPDATA%\BlockTime\blocktime.db` read-write. They migrate it, settle leases every minute and settle flights.
   The dispatch milestone will add a migration, and the first debug run of a draft migration would apply it to the
   real database for good.
2. **Failures that should stop a flight from being lost go unseen or are allowed (1.2 to 1.9).**
   - Settlement can fail or wait, and Cancel then discards the flown flight.
   - A disk error mid-settlement could post money twice.
   - A failed read counts as $0 cash, and the lease run then returns airplanes.
   - One `null` in a log line silently drops a flight's debrief.
   - Flight events whose write failed aren't retried.
   - Since `--dev` hid the event feed, the recorder's own warnings reach nobody.
   - Closing the window mid-flight throws the flight away without asking.

   Each fix is small.
3. **The schema can't change shape (1.10, 2.1, 2.2).** SQLite ignores `PRAGMA foreign_keys` inside a transaction, so
   the migration runner can't rebuild a table. A rebuild of a table with `CASCADE` children would silently delete its
   debrief rows. Several planned changes need exactly that kind of rebuild: flight kinds, lease terms, return
   reasons.
4. **Settlement freezes the window (5.1).** At block in it reads and parses the event log on the GUI thread, which is
   up to 30 daily files once Claude's review starts. The freeze grows every day, and Journeys and the money item add
   more work at that moment.
5. **The middle of the app is where dispatch will hurt (3.1 to 3.4, 4.1 to 4.5).**
   - There is no single "where is Live" state; visibility is decided in six places.
   - `PlannedFlightModel` does seven jobs.
   - Flight state is copied across four models that depend on the order `main()` builds them in.
   - Nothing records what money was previewed or accepted, or how each line was worked out.

   D-214 rebuilds exactly this.

## Principal Engineer's assessment

This is a code base written by someone disciplined who was moving fast, and both show.

**What's good.** The boundaries are excellent and have held for fifteen milestones:
- `blocktime-core` has no Qt and no SimConnect.
- The recorder thread shares three things, each behind a mutex or an atomic, and posts copies to the GUI by queued
  call.
- The detectors (`BlockDetector`, `PhaseDetector`, `TouchdownCapture`, `CockpitMonitor`) are pure functions of their
  samples, tested against CSVs from real flights.
- Measurement is separated from scoring, so a rule change re-scores history without rewriting it.
- The replay drives the same bridge and models as a live flight, with an injected clock. That's the best single
  design decision in the code, and it is why D-174's "few sim tests" can work.
- The schema is cleaner than most production schemas I've seen. It is `STRICT`, every FK has an explicit
  `ON DELETE`, and the ledger is a proper supertype with subtypes. The rules that matter (no double settlement, $5
  lines) are enforced by the database, not by code.
- The decision log is the reason this review was possible at all.

**What's worrying.** Each milestone added a new reader of the same event stream instead of extending a shared one,
so the same rule is now written several times:
- about ten hand-written JSON accessor sets, with different semantics, and some of them throw;
- two state machines over the same GSX and phase events (`FlightStages`, `StatusLines`);
- the debrief's check windows, written again for the lamps;
- five log scanners with three different lookbacks;
- `PriceInput` built in two places;
- `hhmm` written five times and `kMetresPerNm` five times.

None of these is a bug today. Each is a place where the next change has to be made twice and will one day be made
once.

The second pattern is that the rules were applied unevenly:
- "nothing lost" protects block events but not the touchdown or the phases, though they are history too;
- "log first" made the log the database, so every screen re-parses it;
- "QML only displays" was followed so literally that C++ formats `"13,439 lb"` and QML splits it back at the space.

The third is that the middle layer between the recorder and QML (`RecorderBridge`, `PlannedFlightModel`,
`DashboardHeader`, `InFlightModel`) grew by accretion. Each is a reasonable class that does several jobs. Together
they hold flight state four times and depend on the order `main()` builds them in.

**What I'd change first if this were my team's code:**
1. Give development its own data folder.
2. Make every failure on the money and history path loud and refuse the destructive action.
3. Teach the migration runner to rebuild a table safely, with a backup taken before it migrates.
4. Before dispatch, cut `PlannedFlightModel` along the seam D-214 already draws: a plain-C++ planned-flight lifecycle
   with an accepted state, a dispatch model, a narrow read-only view for the panels, and settlement run by a plain
   coordinator, not a QObject.
5. Before Journeys, a plain-C++ `LiveFlight` fed once by the bridge, which every Live model reads, and the tracks
   stored in the database at settlement.

I would not rewrite anything. The core is sound. It needs consolidation, not replacement.

### Grades

| | Grade | Why |
|---|---|---|
| **Overall** | **B−** | The core guarantees hold and the boundaries are right. But one critical exposure (dev runs on real data), a cluster of silent failures on the history path and widespread duplication mean the next three milestones would pay for it without a groundwork pass first. |
| Architecture | B | The library split, the thread hop, the pure detectors and the replay reuse are textbook. The layer between the bridge and QML has no domain object for the live flight, so state and Live's stage are scattered across models held together by construction order. |
| Code quality and design principles | C+ | Small files, good names, RAII and comments that explain why. But DRY is broken in a dozen places, three classes break single responsibility badly (`RecorderBridge`, `PlannedFlightModel`, `Recorder::run`), the replay subclass disables behaviour with no-op overrides (Liskov), and state crosses C++ and QML as display strings. |
| Data model | B+ | Normalised, strict, keyed and indexed, with the money rules in the schema; better than most. It can't evolve yet: no safe table rebuild, growing value sets as `CHECK`s, and no stored tracks or line workings, which Journeys and the money item need. |
| Efficiency | B− | The recorder's hot path is light (about 10 wakeups a second, one 500-byte line a second, low MB). The GUI thread isn't: multi-second log parses at block in, an O(n²) regex rebuild per ATC message, redundant 1 Hz work, and needle glide that keeps rendering at vsync all flight (unmeasured). |
| Tests | B | 953 tests on real SQLite and real flight logs. Settlement, migrations, pricing and detection are well covered. The two foundations of "nothing lost", the `EventLog` writer and the recorder loop, have no tests, and the money tests read JSON they wrote by hand. |
| Docs | B− | The decision log is exceptional and the specs are true in most details. But the README's Run section is four milestones stale, "logs are kept 30 days" (in CLAUDE.md itself) describes a pruning that doesn't exist, and the specs are turning into changelogs. |

## Findings

Severity: **Critical**: can lose or corrupt money or history. **Important**: will cost real time in the next milestones
or hides a bug. **Minor**: everything else. Sizes: small (under an hour or two), medium (about half a session), large
(a session or more).

Counts: 1 Critical, 30 Important, 40 Minor.

### 1. What can't be undone

**1.1 Critical: development runs use Dave's real airline database, read-write.** Confirmed.
- Where: `src/app_main.cpp:419-420` (`AirlineSession::start(defaultAirlinePaths(), …)`), `:489` (`checkFlown()`),
  `:370` (`--demo` skips the single-instance mutex), `:510` (the demo window gets the real `PlannedFlightModel`).
- What's wrong: every launch opens `%LOCALAPPDATA%\BlockTime\blocktime.db` and the real event log folder. That
  includes the debug build, `--demo` ("records nothing") and `--grab`, which CLAUDE.md tells Claude to use for every
  UI check. Each launch:
  - runs `migrateAirlineDatabase`;
  - runs `settleLeases`, at start and every minute;
  - settles any flown flight with the build's own pricing code.

  In `--demo`, Cancel, Import and New Flight act on the real planned flight, and it can run beside the recording
  copy.
- Failure scenario: the dispatch milestone adds migration 10 (`accepted_at`, say). The first debug run applies the
  draft to Dave's database, which is now at `user_version` 10.
  - The installed 0.16 refuses to open it ("newer BlockTime").
  - When the draft is revised, the new version never runs on his file, because the version already says 10.
  - If a flown flight or a lease month was waiting, it is posted for good with work-in-progress pricing.

  Dave's manual backups (`blocktime.before-m9.db`) show this has been handled by care so far, not by design.
- Fix (small):
  - A `--data-dir` option, and a default data folder for debug builds (`BlockTime-dev`).
  - `--demo` and `--grab` work on a scratch copy (`VACUUM INTO` a temp file), or open read-only as `--replay` does
    (D-175).

**1.2 Important: "flown but not settled" isn't a state, and Cancel discards it.** Confirmed.
- Where: `src/planned_flight_model.cpp:226-235` (Cancel is refused only while `flightOpen()`), `:91-98`, `:99-105`;
  `src/recorder.cpp:103-113`; `src/flight_settlement.cpp:57-58` (7-file lookback).
- What's wrong: after block in, `flightOpen()` is false. If settlement didn't happen, the planned flight is still
  there and Cancel is offered. Settlement can fail to happen in three ways:
  - `rates.yaml` doesn't read. The notice says "Fix rates.yaml and restart", which is likely during the money item,
    when Dave hand-edits rates.
  - A settle error.
  - The block_in write was held (D-049). `logBlockEvent` tells the app anyway, `checkFlown` finds nothing, and
    nothing runs it again when the held line is finally written.

  Cancel deletes the planned flight, and the flown flight can never settle: `findFlownFlight` only looks for the
  current planned flight's id. It also only looks back 7 log files, so after about 7 days with the app open, an
  unsettled flight is silently "never flown".
- Fix (small):
  - `cancelFlight` refuses when `findFlownFlight` finds a block out/in pair.
  - Live shows "Flown, waiting to settle: <reason>".
  - `checkFlown` re-runs on the minute tick, or when held events are flushed.

**1.3 Important: after the debrief savepoint, settlement doesn't check that its transaction is still open.**
Confirmed (the trigger is rare).
- Where: `src/flight_settlement.cpp:230-233`, then `:236-250`.
- What's wrong: the return codes of `ROLLBACK TO debrief` and `RELEASE debrief` are ignored. Some errors make SQLite
  roll back the whole transaction (`SQLITE_FULL`, `SQLITE_IOERR`, `SQLITE_NOMEM`). If `writeMeasurements` hits one,
  the ledger inserts that follow run in autocommit:
  - the first `ledger_line` commits on its own;
  - its `flight_money_line` fails the foreign key, because the flight row is gone;
  - the next start settles the flight again and posts its money again.

  The orphan line is in cash but never shown on Flights.
- Fix (small): after the savepoint block, check `sqlite3_get_autocommit(db) == 0` and fail if it isn't; better, a
  `Transaction::stillOpen()`. Add a test with a trigger that aborts after the savepoint.

**1.4 Important: a failed read counts as $0 cash, and the lease run then returns airplanes for good.** Confirmed (the
trigger is rare).
- Where: `src/sqlite_db.cpp:98-104` (`next()` returns false for both "no row" and "error"),
  `src/airline_queries.cpp:84-88` (`cashCentsAsOf` returns 0), `src/leasing.cpp:264` and `:323-334`.
- What's wrong: the "unpaid" return can't be undone, and it fires on any read error: `SQLITE_BUSY` after the 5 s
  timeout, or a future migration that breaks `ledger_line`. `fleetSize`, `hoursFlown` and `settledLogFlightIds`
  hide errors the same way.

  Separately, at startup the leases are settled (`app_main.cpp:419`) before a pending flight (`:489`). A flight that
  blocked in on the 30th and settles on the 1st posts its income dated the 30th, but after the 1st's lease was
  already checked against cash without it.
- Fix (small):
  - Money and state reads return `std::optional`, or check `error()`.
  - `settleLeases` aborts on any read error.
  - At start, flights settle before leases.

**1.5 Important: the migration runner can't rebuild a table, and nothing is backed up before migrating.** Confirmed
(SQLite's documented behaviour).
- Where: `src/airline_db.cpp:389` (`PRAGMA foreign_keys = ON` before the loop), `:400-409` (each migration in
  `BEGIN IMMEDIATE`).
- What's wrong: SQLite ignores `PRAGMA foreign_keys` inside a transaction, so no migration can turn foreign keys off.
  That rules out SQLite's 12-step rebuild, the only way to change a `CHECK`, relax a `NOT NULL` or drop a column.
  - Dropping a parent with `RESTRICT` children fails: `flight`, `ledger_line`, `airplane`. The airline then won't
    open.
  - Dropping a parent with only `CASCADE` children silently deletes them. `flight_debrief` loses its check values,
    skips, landing and review; `flight_review` loses its items. Those rows can't be measured again once the log is
    no longer read.
  - The "rename the old table" variant rewrites the children's `REFERENCES` to point at the old name (SQLite 3.26+).

  Findings 2.1 and 2.2 list the rebuilds that are coming.
- Fix (small to medium):
  - A per-migration "rebuild" flag: foreign keys off before `BEGIN`, `PRAGMA foreign_key_check` must return nothing
    before `COMMIT`, then foreign keys back on.
  - An automatic `VACUUM INTO blocktime.before-v{N}.db` before any pending migration.
  - A test that rebuilds a table with `CASCADE` children.

**1.6 Important: one malformed field silently drops a flight's debrief, and could crash the app at landing.**
Confirmed (the trigger is plausible).
- Where:
  - `src/debrief_measure.cpp:47` (`flag` calls `get<bool>()` unchecked), `:249`, `:420-428` (`d.value("vs_fpm", 0.0)`,
    `d["sim_pitch_deg"].get<double>()`);
  - `src/flight_settlement.cpp:224-229` (`catch … measured.reset()`);
  - `src/inflight_model.cpp:191` (`landingFacts` on the GUI thread, with no catch).
- What's wrong: nlohmann's `value(key, default)` and `get<T>()` throw when the key is present but `null` or the
  wrong type. NaN is written to the log as `null`, and `telemetry_filter.cpp:49` checks only the altitudes for
  finiteness.
  - At settlement the exception drops the whole measurement, with nothing logged. `ReviewWriter::state` then says
    "none", which means "flown before 0.11.0", and that's wrong.
  - In the in-flight panel the same throw happens inside a Qt slot. That terminates the app at the bounce count, and
    on restart the open flight is logged `recorder_crash` and discarded (D-072).

  `MeasurementErrorStillSettles` asserts the silent outcome, and no test feeds `null` fields to `measureFlight`. The
  review has `NullFieldsFromSayIntentionsDontThrow`; the measurement has nothing like it.
- Fix (small):
  - Tolerant accessors (`find` plus `is_number`, as in `flight_stages` and `telemetry_json`).
  - The exception text goes to `errors.log`.
  - A test with `null` fields.
  - A re-measure command in `blocktime-debrief` while the log is still there (it is: see 8.2).

**1.7 Important: flight events whose write failed aren't retried, though they are history now.** Confirmed.
- Where: `src/recorder.cpp:193-199` (`writeFlightEvents`), `:203-209` (cockpit); also the app events.
- What's wrong: the return value of `log.write` is ignored, and the app is told anyway, so Live and the log can
  disagree. D-049 retries only block events. Since then, `phase_changed`, `touchdown`, `bounces`, `landing_runway`
  and `cockpit` have become history: settlement's phases, the debrief's measurements and the landing rating are all
  read back from the log.
- Failure scenario: a write blip at touchdown. Live shows the landing; the stored measurement and the score never
  see it.
- Fix (small): everything with a `flight_id` goes through the outbox. **D-049 now under-protects history.**

**1.8 Important: the recorder's warnings reach nobody outside `--dev`.** Confirmed.
- Where:
  - `src/recorder.cpp:105-109` (held-write `WARNING` to the feed), `:171` (`recorder_started` fails: `run()` returns
    1 and never retries);
  - `src/recorder_bridge.cpp` `applyStopped` (its notice goes to the feed);
  - `qml/LivePage.qml:148-152` (the feed shows only with `--dev`).
- What's wrong: since D-211 hid the feed, D-049's "never lost unseen" warning, "nothing is being recorded" and the
  crash-recovery notice are invisible. If the log folder is locked at startup, the recorder thread ends, the pill
  stays "Waiting for sim", and Dave flies a whole leg that is never recorded. `AppEventQueue` also grows without
  bound once nothing drains it.
- Fix (small):
  - A recorder-health state on the bridge (not recording / events held), shown in `NotRecordingBanner` or the header.
  - Retry opening the log instead of returning. **D-211 is the decision that caused this.**

**1.9 Important: closing the window mid-flight throws the flight away without asking.** Confirmed.
- Where: `src/app_main.cpp:404-410`; `qml/Main.qml` has no `onClosing` handler.
- What's wrong: X → `aboutToQuit` → `recorder_exit` → `flight_interrupted` → discarded (D-046, D-072). One click on a
  10-hour 777 leg loses its money and its history. The decisions are settled; together they make a guard
  necessary before Journeys and long-hauls.
- Fix (small): when `recorder.flightOpen`, closing asks first.

**1.10 Important: the event log writer has no tests, and every reader depends on its exact text.** Confirmed.
- Where:
  - `src/event_log.cpp:106-124`. Its only tests are of `formatUtc` (`tests/event_log_test.cpp`).
  - The text pre-filters: `src/open_flight.cpp:19-23`, `src/flight_settlement.cpp:26-37`,
    `src/flight_history.cpp:24-26`. They match `"type":"block_out"` in the raw line.
  - The block event JSON is read four ways by hand: `telemetry_json.cpp:119`, `flight_settlement.cpp:95-120`,
    `flight_history.cpp:111-140`, `open_flight.cpp:31`.
  - The settlement tests build their own JSON literals (`tests/flight_settlement_test.cpp:37-46`).
- What's wrong: nothing tests the torn-line repair (D-036), the daily rollover, reopening after a failure or the UTF-8
  replacement. The pre-filters depend on compact `dump(-1)` and the key order ts/type/data.
- Failure scenario: someone pretty-prints the log, or renames `shutdown_fuel_lb` in `toJson`. `Json.BlockInFields`
  fails and gets updated; the settlement tests still pass. Production then reads 0 lb on board and 0 h block time
  (`number()` defaults to 0) and posts crew and maintenance at zero, permanently.
- Fix (small):
  - An `EventLog` round-trip test in a temp folder: write, tear the tail, write again, roll past midnight, and read
    back through `findOpenFlight`, `findFlownFlight` and `loadFlightEvents`.
  - The settlement tests build lines with `toJson(BlockEvent{…})`, and `findFlownFlight` reads them with
    `blockEventFromJson`.
  - The event names and one `hasType()` in one header.

**1.11 Important: Claude's review keeps the first copy of a re-logged ATC exchange; the log's contract says the
last.** Confirmed.
- Where: `src/debrief_review.cpp:316` (`if (id >= 0 && !seen.insert(id).second) continue;`) against
  `src/atc_log.h:7` ("a reader keeps the last line per id").
- What's wrong: when ATC's reply arrives after the pilot's request, the review shows only "Pilot: …" and Claude judges
  compliance without the clearance. Reviews are written once. The fixtures have no repeated ids, so no test sees it.
  The review also parses the SayIntentions entry by hand instead of through `atcMessage()` (D-168).
- Fix (small): collect `atcMessage()` results into a map by id (the last wins), then print them in order.

**1.12 Minor: `secondsUntil` doesn't stop at the window's end.** Confirmed. `src/debrief_measure.cpp:73-80`. The header
says "a value never reached counts to block in", but the loop has no break at `end`, so for example strobes switched
off after brake set are measured past block in. Bound the loop or fix the comment, before more flights are stored.
Small.

**1.13 Minor: no single place posts a ledger line.** Confirmed.
- `postLeaseMonth` (`leasing.cpp:60-76`), `postOpeningCash` and `settleFlight` each insert `ledger_line` themselves,
  and each rounds to $5 its own way.
- Category labels live in both `pricing.cpp` and migration 4's `ledger_category` rows. The preview shows the code's
  labels and Flights shows the database's.
- The test that every priced category exists (`airline_db_test.cpp:396-405`) is a hand-written list.

Fix (small): one `postLine(db, category, at, cents)` that asserts the $5 multiple, and the test's list built from
`pricePreview` output over every kind.

**1.14 Minor: nothing in the schema stops history being updated or deleted.** Confirmed. Only tests do it today. Two
triggers (`BEFORE UPDATE OR DELETE ON ledger_line` and `ON flight`, `RAISE(ABORT)`) cost about ten lines. They're
optional under D-090, but they'd turn 1.3-style accidents into loud failures. Small.

**1.15 Minor: settlement mixes concatenated and bound SQL.** Confirmed. `src/flight_settlement.cpp:184-198, 258`
build integer ids into the SQL next to bound statements elsewhere. It's harmless, but inconsistent. Small.

### 2. The database against the rules

**2.1 Important: value sets that are planned to grow are `CHECK`s on the most-referenced tables.** Confirmed.
- Where: `src/airline_db.cpp:70` (`return_reason IN ('unpaid')`; `database.md` already says `'returned'` comes with
  the settings screen), `:138` and `:185` (`kind IN (…)` on `planned_flight` and `flight`), `:65`
  (`monthly_lease_cents NOT NULL CHECK (> 0)`).
- What's wrong: CLAUDE.md says a set that may grow is a lookup table. Each of these needs a rebuild of `airplane` or
  `flight`, which 1.5 blocks. Background flights, positioning legs and per-hour leases all touch them.
- Fix (medium): one rebuild migration, after 1.5, that turns them into lookups.

**2.2 Important: the lease terms are part of `airplane`, so D-218's "lease by use" needs a rebuild and a decision on
D-082.** Confirmed.
- Where: `src/airline_db.cpp:56-72`, `src/leasing.cpp:233-250, 323-351`.
- What's wrong:
  - One airplane row is one lease, with a monthly rate that must be above zero.
  - `settleLeases` charges every month of every airplane that hasn't been returned.
  - A per-block-hour lease needs either a nullable monthly rate (a rebuild) or a separate `lease` table. Otherwise the
    monthly charge keeps running.
  - D-082 keeps the copied rate "for the life of the lease", so re-rating airplanes already leased needs Dave's call.
- Fix (medium): a `lease` table (`airplane_id`, `started_on`, `ended_on`, `end_reason` as a lookup, monthly or hourly
  terms with a `CHECK` that exactly one is set). `lease_charge` points at it, and an hourly charge is a
  `flight_money_line` in a new category.

**2.3 Important: tracks, distance flown and max altitude exist only in the log, and the app reads only the last 30
log files.** Confirmed.
- Where: `src/flight_history.cpp:79-82`, `src/flight_history.h:43` (`kFlightHistoryFiles = 30`),
  `src/flight_list_model.cpp:301`; D-111 ("the tracks still come from the log").
- What's wrong: the Flights map and Replay go grey for a flight once 30 newer log files exist. A log file is written
  for each day the app is open, so in practice that's about a month of flying. The Journey page's map of actual tracks
  is meant to be "the record of the trip" (design.md §16), and a journey can run longer than that. The files
  themselves are never deleted (8.2), so nothing is lost yet: only unread.
- Fix (medium): a `flight_track_point` table (`flight_id`, `seq`, `lat`, `lon`, `alt_ft`; written once, in the
  settling transaction, thinned to at most about 2,000 points), with a backfill from the logs on disk. Do it before
  Journeys.

**2.4 Minor: the code treats `planned_flight` as a singleton, though the schema allows one per airplane.** Confirmed.
`src/planned_flight.cpp:148` (a second one refused), `:217` (`ORDER BY … LIMIT 1` with no airplane), `:297` and `:329`
(`DELETE FROM planned_flight` with no `WHERE`). This fits D-103 today. A queued journey or rotation leg beside a free
flight would be cancelled with it, and settlement would only look at the lowest id. Pass the airplane and planned
flight ids through `currentPlannedFlight`, `cancelPlannedFlight` and `settleIfFlown` when that comes. Small.

**2.5 Minor: the planned and settled tables are kept in step by hand.** Confirmed.
- `ViewTables` (`src/planned_flight.cpp:189-217`) builds one query from table names.
- Settlement copies with hand-written `INSERT … SELECT` column lists (`src/flight_settlement.cpp:155-198`).
- A column added to `planned_flight` and forgotten in the copy is silently missing from history, and dispatch adds
  columns.

Fix (small): a test comparing `pragma_table_info` across each pair, with a list of intended differences.

**2.6 Minor: foreign keys, WAL and `synchronous = FULL` are set by `migrateAirlineDatabaseTo`, not by
`Database::open`.** Confirmed. `src/airline_db.cpp:389`, `src/sqlite_db.cpp:13-26`. Every production path migrates,
so it works today. A new tool that opens the file without migrating gets foreign keys off. Move the pragmas into
`open()`. Small.

**2.7 Minor: no schema-wide test of the rules.** Confirmed. The query-plan list is kept by hand. Nothing runs
`foreign_key_check` or `integrity_check` after a full migration, and no test checks that every FK column has an
index. One test walking `pragma_foreign_key_list` and `pragma_index_list` for every table would cover tables not yet
written. Small.

**2.8 Minor: background flights won't fit `flight.log_flight_id NOT NULL`, and the UI keys flights by it.** Confirmed.
`src/debrief_store.cpp:75-136`, `FlightListModel`'s track and measurement maps, `ReviewWriter`. Background flights
have no log. The design-level answer is a `background_flight` table with its own money link; unlocks (D-075) count
only Dave's `flight` rows. Key the UI by `flight_id`. Medium, when rotations come.

**2.9 Minor: rates are `double` dollars.** Plausible. `src/rates.h`, `src/pricing.cpp:18`
(`llround(dollars * 100)`). Stored money is integer cents, and preview and settlement share the code, so they can't
drift apart. But a value exactly on a $2.50 boundary can round either way, against CLAUDE.md's "no floating-point
money". The rates rework (D-218) is the cheap moment to store rates as integer tenths of a cent. Small.

### 3. Architecture and boundaries

The library split holds: no `blocktime-core` file includes Qt or SimConnect, and the recorder thread touches no Qt
object (checked across the core sources and the observer). The findings are in the layer between the bridge and QML.

**3.1 Important: there's no single "Live state"; what shows is decided in six places.** Confirmed.
- Where:
  - `DashboardHeader::visible` (`src/dashboard_header.cpp:288`), `PreFlightModel::visible`
    (`src/preflight_model.cpp:198`), `InFlightModel::visible` (`src/inflight_model.cpp:378`);
  - `showSummary` in JavaScript (`qml/LivePage.qml:38`), and `PlannedFlightPanel visible: hasFlight` (`:140`);
  - the `preFlightOrigin` lambda in `main` (`src/app_main.cpp:494-501`), which compares state-name strings.

  `PlannedFlightModel::state()` (`src/planned_flight_model.cpp:237-242`) is a string derived on each call, not a
  stored state.
- What it costs: D-214 adds "imported, not accepted", and every one of the six has to learn it. There's also a design
  question in it. `reload()` hands the flight to the recorder as soon as it's planned (`:56-61`), so today a flight
  waiting for its plan is recorded and settled on the estimate. Should an unaccepted flight record?
- Fix (medium): one `LiveStage` enum in core (planned / reviewing / accepted / flying / arrived / summary), computed in
  one place. The panels' `visible` become one-line reads of it.

**3.2 Important: `PlannedFlightModel` does seven jobs, and the replay subclass disables behaviour with no-ops.**
Confirmed.
- Where: `src/planned_flight_display.h`, `src/planned_flight_model.h/.cpp`.
- What's wrong: one QObject holds:
  - the setup actions (import, dispatch page, cancel, busy, message);
  - the reference text (title, runways, route, estimate);
  - the "not recording" diagnosis, which runs SQL and a full airport scan from a property getter (`:149-151`,
    `:159`);
  - the recorder target hand-off;
  - the settlement orchestration (`checkFlown`);
  - the deboarding recording (`:42-48`);
  - the summary lifecycle.

  `ReplayPlannedFlight` inherits virtual no-op `Q_INVOKABLE`s plus a `readOnly` flag QML checks, which breaks Liskov
  and interface segregation. The header, pre-flight and in-flight models depend on all of it to get `view()`,
  `state()` and `changed`. Eighteen properties share one `changed` signal, and the money preview is computed twice
  per emission.
- Fix (medium), along the seam D-214 draws:
  - a narrow `FlightSource` (view, stage, changed) for the panels;
  - a `DispatchModel` (setup → import → review → accept);
  - a plain-C++ settlement coordinator, so the money flow isn't run from a QObject and can be tested without Qt.

**3.3 Important: flight state is held four times, and the models depend on the order `main()` builds them in.**
Confirmed (the duplication); plausible (that it breaks).
- Where:
  - Each of the header, the in-flight and the pre-flight models keeps its own `PlannedFlightView` and its own idea of
    "a new flight" (`dashboard_header.cpp:162` keys on six fields, `inflight_model.cpp:274` on two).
  - The phase is in both `RecorderBridge` and `InFlightModel`; the touchdown in both the header and the in-flight
    model; `PreFlightLog` twice.
  - `InFlightModel` reads predictions from another view model (`const DashboardHeader*`). That works only because the
    header connects to the sample signal first (`app_main.cpp:470` before `:479`).
  - `DashboardHeader::refreshFlight` relies on `PlannedFlightModel` reloading first (its comment says so).
- Failure scenario: Journeys, or a reorder in `main`, quietly makes the panels disagree.
- Fix (medium): one plain-C++ `LiveFlight` fed once by the bridge (planned view, phase, liftoff and touchdown, route
  progress, plan timeline, pre-flight log, predictions), which every model reads. It's testable without Qt and also
  answers 3.1.

**3.4 Important: `RecorderBridge` is a god object, and flight events cross the thread as JSON → text → QVariantMap →
JSON.** Confirmed.
- Where: `src/recorder_bridge.h` (the thread hop, 15 telemetry properties, `FlightStages`, `StatusLines`, the pill and
  checklist, the airport cache, the match state, the clock, the event feed, feed lines classified by text prefix at
  `.cpp:67-81`); `src/recorder_bridge.cpp:330-334`; `src/inflight_model.cpp:35-38`; `src/preflight_model.cpp:41`.
- What's wrong:
  - No QML reads `flightEvent`, yet each event is dumped to text on the recorder thread and parsed into a
    `QVariantMap`. Then the bridge, the in-flight and the pre-flight models each parse it back to JSON, and integers
    become doubles on the way.
  - The dump uses nlohmann's strict handler, so the first event carrying a non-UTF-8 sim string throws on the
    recorder thread. `run()` has no try/catch (`app_main.cpp:395-399`), so the app terminates mid-flight and the
    flight is discarded on restart.
- Fix: dumping with `error_handler_t::replace` and a try/catch around the recorder's callbacks is small. Passing
  `std::shared_ptr<const nlohmann::json>`, and moving stages, status and the pill into 3.3's `LiveFlight`, is medium.

**3.5 Important: `Recorder::run` is a 200-line function of lambdas, so the recorder's wiring can't be tested.**
Confirmed.
- Where: `src/recorder.cpp:168-376`. About ten lambdas capture everything by reference. A raw `simPtr` is set after
  construction to break a cycle. `SimConnection` callbacks are registered three different ways.
- What's wrong: the ordering rules aren't tested: log then tell, the tracker after the block event, the cockpit
  snapshot at block out, the fast-sampling toggles, crash recovery's `recorder_crash`, the outbox's `LOST:` lines at
  exit. Settlement already works around one of them: `phase_changed arrived` is written after `block_in`
  (`flight_settlement.cpp:117-119`).

  No test can feed one of Dave's `.jsonl` logs back through TelemetryLogger → BlockDetector → FlightTracker, though
  `telemetryFromJson` exists. Every new feed (dispatch, Journeys) means editing this function.
- Fix (medium): a `FlightSession` that owns the detectors and the tracker and writes to an `EventSink` (the log plus
  the observer, with the durability rules of 1.7 in one place). `SimConnection` only delivers decoded records. Then add
  a "re-record a log" test.

**3.6 Minor: C++ and QML talk in display strings.** Confirmed.
- `qml/PreFlightPanel.qml:23-27` splits `"13,439 lb"` at the space and maps `"Main deck"` to `"MAIN"`.
- `qml/FleetPage.qml:186-187` picks a badge tone from `"Parked"`/`"Returned"`.
- `qml/MoneyLines.qml:44,86` colours an amount by its leading `-`.
- `qml/PlannedFlightPanel.qml:243` rewrites `title` with `.replace(" · ", …)`.
- In C++, `bridge_.connection() == "connected"` (`src/planned_flight_model.cpp:133, 241`,
  `src/flight_watcher.cpp:197`), and `main` compares state names.

This is "QML only displays" followed so literally that the formatting is undone in JavaScript. Expose typed value,
unit and tone fields, and `Q_ENUM`s for states. Small.

**3.7 Minor: dead code left from D-134/D-139 (superseded by D-180).** Confirmed.
- `RecorderBridge::stages` (`recorder_bridge.h:62`, `.cpp:271-292`) is read by no QML file. Of `FlightStages`' 334
  lines, about 200 (the stage line texts, `readyLine`, the checklist, the landed and gate text) feed only that
  property and about 20 tests in `flight_stages_test.cpp`.
- Unused by QML: `onGround`, `enginesRunning`, `engineCount`, `atcId`, `showingFlight`, `hasNextLegServices`,
  `FlightListModel.loading`, `PreFlightModel.cleared`.
- `PlannedFlightModel::flightChanged` is never connected, though its comment says the Fleet screen refreshes on it.
- `parseNavlog` and `airportRunwaysFromJson` are used only by tests.

Medium.

**3.8 Minor: `FlightStages` and `StatusLines` are two state machines over the same events.** Confirmed. Each has its own
`GsxTimeline` fed the same events, a copied `updateReady` (D-135; `status_lines.cpp:103` against
`flight_stages.cpp:173`) and copied landed and gate texts. `pill()` and `pillMarker()` each rebuild the whole `view()`
on every change. Fold into one, around one `GsxTimeline`, with 3.7. Medium.

**3.9 Minor: `FlightListModel` does five jobs.** Confirmed. The list, the tracks, the debrief scoring, the review state
and the replay launch. The landing line's text is built in Qt (`flight_list_model.cpp:348-372`), where
`blocktime-tests` can't reach it. Move the debrief text to `debrief_*.cpp`. Small.

### 4. Readiness for what's next

**4.1 Important: the preview prices fuel as the planned burn, settlement as the fuel added.** Confirmed.
- Where: `src/planned_flight.cpp:398-401` (`fuelBurnLb = plan->enrouteBurnLb`) against
  `src/flight_settlement.cpp:148` (`max(0, fuel_out − fuel_on_record)`).
- What's wrong: on the first leg after a delivery, little fuel is on record, so the settled fuel line is about the
  whole ramp fuel (taxi, contingency and reserves too), roughly 1.5–2× the preview on a short leg. Later legs drift by
  whatever was left over. Once D-214 shows "how each line was worked out", the fuel line would explain a number
  settlement doesn't use. **D-104 now conflicts with D-109.**
- Fix (small): preview `max(0, plan ramp fuel − fuel on record)`. `PlannedFlightModel` already holds
  `fuelOnRecordLb_`, so it only needs passing into `PriceInput`.

**4.2 Important: nothing records what money was previewed or accepted, or how a line was worked out.** Confirmed.
- Where: `src/flight_settlement.cpp:146-153` and `:236-250` (category and amount only); `src/pricing.h:38-42`
  (`MoneyLine` is category, label, cents); `src/app_main.cpp:254` and `src/replay_planned_flight.cpp` (a replay's
  estimate uses today's rates).
- What's wrong: settlement re-prices from inputs that can change between planning and block in:
  - `rates.yaml` (by D-109's own rule);
  - the airplane's seats and limits;
  - the catalog's cruise speed, when there's no plan;
  - the SimBrief plan, which can be re-imported mid-flight (4.6).

  Dave accepts a breakdown under D-214, tunes `rates.yaml` against real flights under D-218, and the flight settles
  at amounts he didn't accept. A summary that explains each line can only recompute it from today's rates, so after a
  rate change the explanation disagrees with the stored amount.
- Fix (medium): at Accept, store the priced lines, each with the quantity and unit rate behind it (nullable columns
  added with `ALTER … ADD COLUMN`; NULL = settled before this change). Settlement copies them and recomputes only
  D-109's actuals (crew, maintenance, fuel added). **This needs Dave: D-109's "rates as at settlement" against
  D-214's "accept".**

**4.3 Important: `PriceInput` is built in two places, with no test that they agree.** Confirmed.
- Where: `src/flight_draft.cpp:42-57` and `src/planned_flight.cpp:387-405`.
- What's wrong: both add up the seats and set the family, role, MLW, fuel price and block time. Only the shared
  `pricePreview` keeps them honest. Every new input the next stretch brings (lease per block hour, a fare curve,
  Maximize payload) must be added to both. If one is missed, New Flight and Live or settlement disagree silently.
- Fix (small): one builder used by both, plus a test that a draft's preview equals the stored planned flight's
  preview right after `createPlannedFlight`.

**4.4 Important: money lines carry no explanation, and they're converted to QML three times.** Confirmed.
`src/pricing.cpp:16-20` (`add()` receives dollars already multiplied out); `src/new_flight_model.cpp:24-31`,
`src/planned_flight_display.cpp:80-90`, `src/flight_list_model.cpp:159`. The panel also passes income lines in as
`costs` (`qml/PlannedFlightPanel.qml:217`). D-214's breakdown ("140 × $157.50", "2.6 h × 2 pilots × $200") needs
`pricing.cpp`, the only place that knows the quantities, to return them. Add quantity, unit and rate (or a basis
string) to `MoneyLine`, with one adapter to QML. Small to medium.

**4.5 Important: dispatch needs two things the core can't do yet: an accepted state, and reopening New Flight on an
existing planned flight.** Confirmed.
- Where: `src/planned_flight_model.cpp:56-61` (`reload` sets the recorder target unconditionally);
  `src/flight_draft.h:26` (a draft is always built by rolling loads); `src/planned_flight.cpp:143-150` (a second
  planned flight is refused, and there's no update function).
- What's needed:
  - `planned_flight.accepted_at` (additive, no rebuild), and `flightTarget()` returning nothing until accepted;
  - a `FlightDraft` built from the stored loads with no re-roll (D-089);
  - `updatePlannedFlight` in one transaction;
  - a rule that changing the destination, kind or loads drops the imported plan.
- Size: medium. It belongs to the dispatch milestone itself; listed here so the plan doesn't miss it.

**4.6 Minor: a plan can be re-imported mid-flight.** Confirmed.
- Where: `qml/PlannedFlightPanel.qml:181-185` (Import is enabled while flying) and
  `src/planned_flight_model.cpp:191-192` (no `flightOpen()` guard; Cancel has one).
- What happens: a new OFP replaces the plan, so settlement uses its block time for the catering band. Its new
  `requestId` also makes `DashboardHeader::refreshFlight` reset `airborne_` and `liftoff_`, and the arc and the
  predictions freeze for the rest of the flight.
- Fix (small): guard it. D-214's accept freezes it anyway.

**4.7 Minor: Import doesn't compare the OFP's payload with the planned loads.** Plausible. `src/simbrief.cpp:206-224`
(`planMismatch` checks units, airports, airframe and time). Dave can edit passengers or cargo on SimBrief's page, and
SimBrief caps cargo at the airframe's maximum (`docs/notes/simbrief-api.md`). With Maximize payload (D-215) at the
limits, BlockTime would pay for cargo the sim never carried. `PlanBrief` already reads `weights.cargo`. Warn, or
refuse at Accept. Small.

**4.8 Minor: `rates.yaml` is loaded five ways.** Confirmed.
- `new_flight_model.cpp:64` shows the error and blocks Plan.
- `planned_flight_model.cpp:49, :57` and `app_main.cpp:254, :478` fall back silently to the built-in defaults.
- `planned_flight_model.cpp:93` refuses to settle, which is right.

A broken file makes Live's estimate quietly show defaults while New Flight shows an error. Separately,
`vipFlightAttendantsTimes` is a `double` priced as 2.5× but shown as 2× on SOULS (`app_main.cpp:480` casts it), and
the crew rule exists twice (`pricing.cpp:85-90` against `crewFor`). One loader that returns the rates or a notice,
integer crew counts and one crew function. Small.

**4.9 Minor: role, family and kind are strings.** Confirmed. `"executive"`, `"freighter"` and `"777"` are compared in
`pricing.cpp:41-42, 86, 100-101` and `planned_flight.cpp:93-94`. `airline_session.cpp:108` turns the `FlightKind`
enum back into a string to call `spotUseFor`, and a `Family` enum already exists in `cockpit_reading.h`. Use enums
from the catalog boundary in. Small.

**Readiness, in short.**
- **Dispatch (D-214, D-215): medium.**
  - Ready: pure pricing, async import, the stored plan, the mismatch check, `maxPayloadLb` in `AirplaneLimits`.
    Maximize payload is a flag in `FlightDraft` and a branch beside `rollLoads`.
  - First: 4.3, so the new inputs land in one place.
  - Then: 3.1 and 3.2 as the milestone's own structure, plus 4.4, 4.5, 4.1, 4.6 and 4.7.
- **The money item (D-216 to D-218): small for fares, medium for a per-hour lease.**
  - Fares are `rates.h`, `rates.cpp`, `rates.yaml` and `pricing.cpp`. A fare curve needs a list type in YAML.
    Because `pricePreview` is pure, D-216 and D-217 can be held by grid tests (profit per Dave-hour across distances)
    cheaply.
  - A per-hour lease needs 2.2, 1.5, a new category and a change to `settleLeases`.
  - Decide 4.2 before any rate changes, or the flights settled before it won't have their workings.
- **Journeys: medium. Rotations and background flying: large.**
  - Journeys fit D-103's one-planned-flight-at-a-time, since Dave flies one leg at a time. They need:
    - `journey` and `journey_leg` tables;
    - a `journey_leg_flight` link written in the settling transaction, so `flight` itself never changes;
    - a nullable leg reference on `planned_flight`;
    - "On journey" derived from `journey.airplane_id` (there is no airplane state at all today);
    - New Flight seeded with the next leg;
    - and, before any of it, 2.3 (the tracks).
  - Rotations reuse `pricePreview` but need an input builder from leg templates (4.3 again). `settleFlight` is tied to
    the event log and the planned-flight row, so background settlement is new code (2.8). The catch-up engine is the
    large part, and it turns the lease queue into one dated timeline of money events (1.4's back-dating becomes
    routine; D-096 accepted "rare back-dated cases aren't rechecked").

### 5. Threading, lifetime and failure handling

Threading is disciplined:
- three shared objects, each behind a mutex or an atomic, with copies posted by queued call;
- one writer of the log;
- database access only on the GUI thread;
- `StopAndJoin` in case `aboutToQuit` is skipped;
- `QPointer` in the network callbacks;
- `FlightListModel`'s worker joined in its destructor.

No data race on a Qt object was found. Web calls never block the GUI. Each has a transfer timeout (8, 15 and 300 s)
and calls back exactly once. Stale New Flight answers are dropped by a request counter, and `importPlan` re-checks the
flight when the reply lands. The API keys appear only in headers or URLs and are never logged.

**5.1 Important: settling reads the event log on the GUI thread, and the review then parses up to 30 daily files in
full.** Confirmed (the path); plausible (the size of the freeze).
- Where: `src/planned_flight_model.cpp:85-112` → `recordDeboardings` and `findFlownFlight` (7 files each,
  pre-filtered by text) and `loadFlightEvents` (7 files, every line parsed as JSON, `src/flight_events.cpp:32`); then
  the `settled` handler → `ReviewWriter::start` → `loadFlightEvents(…, kFlightHistoryFiles)` (`src/review_writer.cpp:65`),
  which parses every line of up to 30 files up to the flight's block in.
- What it costs: Dave's 2026-11-24 file is 17.6 MB, about 28k lines. D-061's own measurement was about 4.8 s for
  450k lines, and that was with a pre-filter. The window freezes at block in, as the summary appears, and grows with
  every day of logs until 30 files are kept. The same happens on Retry, at startup and on every deboarding
  `gsx_service` event (`planned_flight_model.cpp:42-48`). `measureFlight` also runs while holding the
  `BEGIN IMMEDIATE` write lock. No data is lost: the recorder thread writes first.
- Fix:
  - Small: hand the lines settlement already loaded (`flown->lines`) to the review, and give `loadFlightEvents` a
    type pre-filter and a start at the block-out file.
  - Medium: run the read phase on a worker, as `FlightListModel` does, and apply the transaction on the GUI thread.
  - 2.3 removes the tracks' share of the work.

**5.2 Important: the ATC card redoes all its regex work on every message.** Confirmed (the cost model); plausible (the
size of the pause).
- Where: `src/atc_card.cpp:81-102` (`rebuild` runs `readInstructions` over every stored message on each new message,
  go-around and touchdown, plus a fresh `std::regex` at `:100`), and `src/atc_instructions.cpp` (22 `std::regex`, only
  one of them `static`, built per sentence).
- What it costs: MSVC's `std::regex` is slow to build. A flight with about 60 COM exchanges rebuilds thousands of
  regexes on the GUI thread per late message. A replay seek feeds every message, so the cost is O(n²).
- Fix (small): `static const` patterns, and each message's `SlotChange`s cached beside it, so `rebuild` only replays
  them.

**5.3 Minor: the Planner interface is wide, and its reply handling is untested.** Confirmed (the captive-portal case is
plausible).
- `src/planner.h` has eight methods for three users, and `FakePlanner` answers synchronously, so a cancel during an
  OFP import is never tested.
- The status, error and body classification lives only in `QtPlanner`, copied twice (`qt_planner.cpp:42-62` and
  `:123-141`).
- A 200 with a non-JSON body gives no runways and no note, which looks like "no ATIS", and D-105 says failures mustn't.
- A SimBrief timeout reads "Operation canceled".

The abstraction earns its keep (the model tests depend on it). Move the classification into a tested core function;
split the interface when the next consumer comes. Small.

**5.4 Minor: the last ATC exchanges miss the debrief.** Confirmed. `src/flight_watcher.cpp:61-63` fetches the ATC log
at block in, asynchronously, so those lines are written after `block_in`. `loadFlightEvents` stops at block in
(`src/flight_events.cpp:60-63`). The taxi-in exchanges never reach the review. Small.

**5.5 Minor: errors.log is written from both threads without a lock.** Plausible. `src/error_log.cpp:13-15` swaps
`std::cerr`'s buffer for a file buffer that isn't synchronised, and both threads write to it. Qt's messages go to C
`stderr` (`app_main.cpp:188`), which isn't redirected, so they likely reach nowhere in a GUI-subsystem exe. Small.

**5.6 Minor: a touchdown followed by a go-around is recorded as the landing.** Confirmed.
`src/touchdown_capture.cpp:11-30`, `src/flight_tracker.cpp:187-201`. The phase stays Landing (phases only move
forward), the first contact is stored as the touchdown, and fast sampling stays off for the real landing. This is
rare in airline flying; a test that documents the behaviour is enough. Small.

### 6. Tests

The strengths are real:
- 953 tests;
- settlement covered by 38 tests, including a failure that writes nothing, settling twice, midnight, a diversion,
  fuel added and a real-log settle;
- each migration tested from N to N+1 with rows in place;
- `EXPLAIN QUERY PLAN` over 21 real queries;
- foreign keys checked on both kinds of connection;
- real-flight fixtures, and injected clocks.

The gaps on the riskiest paths are 1.10 (the log writer) and 3.5 (the recorder loop), plus:

**6.1 Minor: the real-log settle test checks only the measurements.** Confirmed.
`WritesMeasurementsInTheTransaction` (`tests/flight_settlement_test.cpp:684`) settles the real KLAS–KSAN log but never
checks the money, `fuel_in_lb`, the arrival airport or the block time. Those four checks would catch 1.10's rename on a
real log. Small.

**6.2 Minor: the stored measurements are tested mostly through one fixture with loose bounds.** Confirmed.
- Examples of loose bounds: `strobes_on_after_landing_s <= 60`, origin baro `<= 3`, `NEAR` on 250 kt and STD.
- Missing unit tests: the 777 flap table and clamping, inHg against hPa in `hpa()`, "the ATIS before it, whichever is
  closer", the taxi window edges, and engines started, stopped and started again in `flightStart`.

Per D-174 this is where careful tests belong. Medium.

**6.3 Minor: `tests/qt_models_test.cpp` is 3,249 lines, with about 155 tests over 15 components.** Confirmed. It has its
own helper namespaces per section and a global `g_target`. Every edit rebuilds all of it. Split it into one file per
model with a shared `tests/qt_test_support.h`. Medium, mechanical.

**6.4 Minor: Qt tests wait on real timers.** Confirmed (the timers); plausible (the flakiness).
- Instances: `WatcherRig::wait` (used 21 times, with 30 ms intervals and `EXPECT_GT(calls, 1)` after 100 ms),
  `FlashRunsOnlyWhileWrong` (a 5 ms timer in 60 ms windows), `TimerPlaysUntilPaused`.
- They make up most of the 105 s debug run. Windows' 15.6 ms timer resolution on a busy machine can starve them.
- Drive `tick()` directly, as the injected clock already allows. Small.

**6.5 Minor: two tests share a temp folder.** Confirmed. `OpenFlight.EmptyOrMissingDirHasNone` and
`FlightHistory.EmptyOrMissingDirHasNone` both use `%TEMP%\blocktime-test-EmptyOrMissingDirHasNone`
(`tests/open_flight_test.cpp:37-38`, `tests/flight_history_test.cpp:55-56`). The presets run serially, so it's latent;
under `ctest -j` one test's TearDown deletes the other's folder. Add the suite name. Small.

**6.6 Minor: test helpers are copied.** Confirmed. The temp-dir fixture appears in 9 files. `blockOut`/`blockIn`/
`interrupted` line builders appear in 4, with different signatures. `tests/replay.h:29` `parseTs` duplicates
`parseUtc`. One `tests/log_lines.h` built on `toJson` (which also fixes 1.10). Small.

### 7. Duplication and dead code

**7.1 Important: the lamps re-implement the debrief's check windows.** Confirmed.
- Where: `src/lamps.cpp:94-186` against `src/debrief_measure.cpp:180-350`.
- What's wrong: D-195 promises a lamp flashes exactly when the debrief would mark the check down, but only the
  readers and tolerances in `cockpit_reading.h` are shared. Each window is written twice:
  - belts from taxi out to the transition altitude (else descent, else touchdown);
  - landing lights restarting after a go-around;
  - the beacon start;
  - the APU from the transition altitude;
  - STD.

  The checks have changed four times already (D-172, D-173, D-200 and the go-around rule).
- Fix (medium): one `CheckWindows` value computed by one function from the markers and phases. `Measurer` uses it on
  the whole flight; `LampBoard` uses it on the markers so far.

**7.2 Important: events are read through about ten hand-written JSON accessor sets, and phases are compared as
strings.** Confirmed.
- Accessors: `str`, `text`, `field`, `number`, `num`, `integer`, `whole`, `flag` and `when` are each defined again in
  `debrief_measure`, `debrief_review`, `flight_stages`, `status_lines`, `gsx_timeline`, `feed_text`, `atc_message`,
  `atc_log`, `preflight_log`, `inflight_readouts`, `cockpit_reading`, `telemetry_json`, `lamps`, `simbrief`,
  `plan_brief`, `sayintentions` and `flight_settlement`.
  - Their semantics differ, and some throw on `null`: that's the root of 1.6.
  - The "data.time, else ts" rule is copied three times (`debrief_measure.cpp:31`, `debrief_review.cpp:38`,
    `lamps.cpp:48`).
  - The debrief reads raw keys (`"ias_kt"`, `"beacon"`) where replay uses `telemetryFromJson`, so a renamed key
    silently reads its default in a stored measurement.
- Phases: `enum class Phase` and `phaseCode()` exist (`phase_detector.h`), but about eight display-side files test
  phase strings, and the set descent / approach / go-around alone appears four times.
- Fix:
  - small: one `event_json.h` with tolerant accessors and `eventTime(const LogLine&)`;
  - small: `phaseFromCode()` with the stage and panel mapping in one table;
  - large, optional, later: typed event structs parsed once at the bridge.

**7.3 Minor: the log is scanned by five functions with three lookbacks.** Confirmed. `open_flight.cpp:19`,
`flight_settlement.cpp:26-37` and `:266-301`, `flight_history.cpp:24`, `flight_events.cpp`. Each has its own text
pre-filter and its own copy of the `files.erase(…)` window: 7 files (settle), 30 (review, Flights) and 1000
(`debrief_main.cpp:38`). One `forEachLogLine(files, typeFilter, fn)`. Medium.

**7.4 Minor: the OFP is read by three parsers, and the stored OFP is parsed twice per view.** Confirmed. `parseOfp` and
`parsePlanFixes` (`simbrief.cpp`) and `parsePlanBrief` (`plan_brief.cpp`) each have their own helpers, and their
duration readers disagree (`hms` for `est_block`, `ofpSeconds` elsewhere). `readFlightView` parses `ofp_json` twice
(`planned_flight.cpp:262, 264`) on every `currentPlannedFlight()`, including the gate-context lambda and the
existence check in `createPlannedFlight`. Parse once into one `Ofp` struct. Small to medium.

**7.5 Minor: small helpers copied.** Confirmed.

| Helper | Copies |
|---|---|
| `hhmm` / "HH:MMZ" | 5 (`planned_flight_display.cpp`, `planned_flight_model.cpp`, `review_writer.cpp`, `flight_stages.cpp`, `debrief_main.cpp`) |
| `q()` | 6 |
| `utcNowInstant` | 2, identical (`new_flight_model.cpp:20`, `planned_flight_model.cpp:21`); `review_writer.cpp:100` makes its own |
| thousands separators | 5, though `formatNumber` exists (`debrief_score.cpp:54`, `debrief_review.cpp:144`, `header_text`, `simbrief`, `fleet_table_model.cpp:15`) |
| `kMetresPerNm` / `kMetersPerNm` | 5, both spellings |
| Earth models | 3 flat-earth projections with different constants (`route_progress.cpp:27`, `runway_match.cpp:17`, `parking.cpp:16`), `lined_up.cpp`'s 111,320 m per degree, and a 6,371 km sphere |
| inHg or hPa? | 3 rules: `v > 800` (`debrief_measure.cpp:96`), `> 100` (`departure_conditions.cpp`), `< 100` (`inflight_model.cpp`); `kInHgToHpa` twice; `hpa()` uses locale-dependent `std::stod` |
| 18,000 ft default transition altitude | 3 inline (`header_text.cpp:24`, `inflight_readouts.cpp:77`, `preflight_model.cpp:112`) |
| 10,000 ft / 260 kt | 2 inline (`debrief_measure.cpp:249`, `debrief_review.cpp:233`), though `debrief_score.h` says every tolerance lives there |
| `engineCount` / clamp(0, 4) | 4 |

Small, a little at a time.

**7.6 Minor: the Qt model sources are compiled twice, and the Qt tests link SimConnect.** Confirmed. The same 17
source pairs are listed by hand in `qt_add_qml_module(blocktime-app …)` and `blocktime-qt-tests`
(`CMakeLists.txt:320-345` and `:380-397`). They're compiled and moc'd twice, and the two lists drift.
`blocktime-qt-tests` links `blocktime-sim` only because `recorder_bridge.cpp` uses `enginesRunning` and
`RecorderObserver`. A `blocktime-qt` static library linked by both. Small.

**7.7 Minor: build hygiene.** Confirmed. "No warnings" isn't enforced (no `/WX`, `CMakeLists.txt:53`). `vcpkg.json`
still says version `0.8.0`. qmllint's unused import in `qml/ReplayBar.qml:12`. `find_package(yaml-cpp)` sits inside the
core target's block (`:129`). Small.

### 8. Docs against code

**8.1 Important: README.md's Run section describes the app before milestone 12.** Confirmed.
- `README.md:9`: "version 0.10.0". It's 0.16.0.
- `README.md:21`: "Live shows the stages, the live values and the event log in three columns". The stages went in
  D-180.
- `README.md:159`: "Live: live flight values, the block clock, and an event feed". The tiles and the feed show only
  with `--dev` (D-211).
- `README.md:164`: "Flight progress shows the six stages". There's no such card.
- `README.md:171`: "Live shows its summary" at block in. It waits for Finish Flight (D-207).
- `README.md:215`: `--demo` shows "a made-up departure in Flight progress".

The header, the Pre-Flight panel, the in-flight panel and the predictions aren't described at all. `README.md:65`
leaves yaml-cpp out of vcpkg's list, and the Files table leaves out `BlockTime.ini`. CLAUDE.md duty 5. Small.

**8.2 Important: "logs are kept 30 days" is stated everywhere, but nothing deletes event logs.** Confirmed.
- Where it's said: `CLAUDE.md:89` ("kept 30 days"), `README.md:220`, `docs/design.md:516`, `docs/debrief.md:168`,
  D-137 ("the event log is deleted after 30 days"), D-174, `src/review_writer.cpp:65`, `src/flight_replay.h:29`,
  `src/flight_list_model.cpp:301`.
- What's true: only map tiles are pruned (`app_main.cpp:198`). The logs grow forever, about 3–17 MB a flying day, and
  the readers mean "the last 30 files" (or 7), not 30 days.
- That's good for history: every flight's log is still on disk, which makes 1.6's re-measure and 2.3's backfill
  possible. But the docs say the opposite, and the "copy a flight to `tests/data/` before its log expires" advice
  rests on it.
- **Dave's call:** prune them (and then do 2.3 first), or keep them and fix the statements. Small either way.

**8.3 Minor: `database.md` against the migrations.** Confirmed.
- Line 7 says "built through milestone 8 (schema version 5)". It's schema 9.
- Line 135 says the current-state columns and `returned_on`/`return_reason` "are the only columns in the database
  that are ever updated". `airplane.free_flight_number` is reset at every start (`leasing.cpp:391-399`),
  `aircraft_type` and `airport` rows are upserted, and `simbrief_plan` is `INSERT OR REPLACE`.
- Lines 15 and 25 say foreign keys, WAL and `synchronous = FULL` are on for "every connection". See 2.6.
- The entity diagram leaves out `airport_import`.

Everything else matches column by column: types, `CHECK`s, `ON DELETE` rules, indexes and seed rows. Small.

**8.4 Minor: the other specs.** Confirmed.
- `docs/recorder.md`:
  - `:342` and `:348` say held block events show as a `WARNING` line in the feed, but the feed is hidden (1.8);
  - `:330` lists `go_around` as an event type, though it's a `phase_changed`;
  - the event table leaves out `arrival_gate` and `pmdg_data`.
- `docs/replay.md:115` names `replay.h` in `blocktime-core`; it's `src/flight_replay.h`.
- `docs/dashboard.md`:
  - `:132` says "with nothing planned, Live is the tiles and the feed as today", which is false without `--dev`;
  - `:732` says the page switches to the summary at block in, which is no longer true since D-207;
  - `:759` is a "docs to change when it's built" list that's done.
- `docs/debrief.md`: check 9 is skipped on a diversion "unless the ATIS logged is for the airport landed at", but
  `destinationBaro()` always skips it.

Small.

**8.5 Minor: the specs are turning into changelogs.** Confirmed. `design.md` §13.5 and §13.7 keep "as it was until
milestone 12" bullets ("Three columns", "The card spans the gate", `:437-470`) beside the current text, so the next
reader has to reconcile two layers. Rewrite those sections to the current state; the decision log already holds the
history. Medium.

### 9. Performance budget

The recorder's hot path is well inside the budget:
- about ten wakeups a second;
- one change-based 1 Hz definition with epsilons;
- sim-frame sampling for about 30 s a flight;
- about one 500-byte line a second;
- memory in the low MB.

The risks are on the GUI thread: 5.1 and 5.2 above, plus:

**9.1 Important: needle glide keeps the window rendering at vsync for the whole flight, hidden panels included.**
Confirmed (the mechanism); plausible (the cost).
- Where: `qml/Theme.qml:243` (`needleGlideMs: 1000`), `qml/GaugeNeedle.qml:29-35`, `qml/InFlightPanel.qml`
  (`glide: panel.model.animate`).
- What's wrong: with 1 Hz samples and a 1 s linear glide, a needle is always moving. Fuel changes every second, even
  at the gate with the APU running, so Qt's render loop never idles: it renders 60–144 frames a second all flight.
  `glide` follows only `animate`, not whether the panel is visible or its page is current. `InFlightPanel` is
  `visible: false` before block out but still animates. D-205 allows the glide; this is about its cost, not the
  decision.
- Fix (small): `glide: animate && panel visible && page current`. Measure CPU in a 1× replay. If the result is over
  1%, a shorter glide (say 400 ms) lets the loop idle most of each second.

**9.2 Minor: 1 Hz updates re-evaluate everything.** Confirmed. Likely within budget, but unmeasured.
- Coarse signals: `RecorderBridge::sampleChanged` (15 properties), `DashboardHeader::changed` (21) and
  `InFlightModel::changed` (22) are single notify signals.
- The header emits twice a second airborne: per sample, and per timer tick.
- Each header emission runs `predict()` about 11 times, through `destinationLine`, `punctualityText` twice and so on.
- `InFlightModel::onSample` rebuilds the lamps, ATC slots, readouts and the departure view on every sample, including
  the ATIS parse with a fresh regex (`departure_conditions.cpp:42`) and three `istringstream` METAR splits.
- QML reads `model.lamps[index]` per delegate, converting the whole list each time.
- `FlightHeader.qml:268` binds `shares` to a new array per emission, so the Canvas arc repaints about twice a second
  with nothing new.
- `FlightPanel` is built with `visible: page.devMode` and keeps evaluating outside dev mode.

Fix (small): split signals, cache the predictions per tick, cache the ATIS view per ATIS line, and put `FlightPanel`
in a `Loader`.

**9.3 Minor: the PMDG data area is requested whole.** Plausible. `src/pmdg_probe_ng3.cpp:24-27` and the 777 file
request the full `PMDG_NG3_Data` struct with ON_SET + CHANGED to read about seven fields. Needles that change all the
time may re-send several KB at frame rate. Define only the needed byte ranges with
`AddToClientDataDefinition(offset, size)`. Unmeasured. Small.

**9.4 Minor: fast samples aren't logged.** Confirmed. `src/sim_connection.cpp` (fast sampling) and
`src/flight_tracker.cpp:203`. The touchdown, G and bounce figures are stored measurements, but only the 1 Hz
telemetry is in the log, so they can't be re-derived or debugged. Frames are also stamped by a 100 ms poll loop (about
±0.1 s, `recorder.md:211`) against a 0.5 s bounce threshold. Log the raw frames while fast sampling (about 1–2k short
lines a flight). Small.

**9.5 Minor: work repeated on the recorder's 100 ms loop.** Confirmed. `src/recorder.cpp` builds the cockpit JSON
(`CockpitMonitor::due`) and copies `FlightTarget`, with its six strings, twice per pass. The CPU cost is negligible,
but it's an allocation for nothing. A version counter on the slot. Small.

## Decisions this review touches

None is reopened here. Each is named where it now causes trouble, for Dave to confirm or change:

- **D-109** (settle at the rates as they are at settlement) against **D-214** (accept a breakdown): 4.2.
- **D-104** (the preview's fuel is the planned burn) against **D-109** (settle the fuel added): 4.1.
- **D-049** (only block events are held and retried): 1.7.
- **D-211** (the event feed only with `--dev`) hid D-049's warning: 1.8.
- **D-072 / D-046** (an interrupted flight is discarded) make a close guard necessary: 1.9.
- **D-060 / D-111** (the log is the database; tracks from the log): 2.3 and 5.1.
- **D-082** (the lease rate is copied for the life of the lease) and **D-218** (lease by use): 2.2.
- **D-096** (back-dated cases aren't rechecked) stops holding with background flying: 1.4 and §4.
- **D-137 / D-174** ("logs are kept 30 days"): 8.2.

## Suggested order for fixing

Each step stands alone; cut any of them.

1. **Protect what can't be undone** (one session; every item is small):
   1. Development gets its own data folder; `--demo` and `--grab` use a copy (1.1). An automatic backup before
      migrating (part of 1.5).
   2. Refuse Cancel on a flown flight, show "waiting to settle", retry on the tick (1.2). Check the transaction after
      the savepoint (1.3). Make read errors not mean $0, and settle flights before leases (1.4).
   3. Tolerant accessors in the debrief, the error logged, a `null` test (1.6). The bridge's dump with `replace`, and a
      try/catch around the recorder's callbacks (3.4, small part).
   4. Every flight-id event through the outbox (1.7). Recorder health shown outside `--dev` (1.8). Confirm before
      closing mid-flight (1.9).
   5. The `EventLog` round-trip test, and settlement reading block events through `blockEventFromJson` (1.10, 6.1).
   6. The review keeps the last ATC copy (1.11); fix or bound `secondsUntil` (1.12).
2. **Make block in fast:** the settled lines handed to the review, and a pre-filter for `loadFlightEvents` (5.1, small
   part). The ATC card's cache (5.2). Measure the needle glide in a 1× replay (9.1).
3. **Groundwork for dispatch and the money item** (before or at the start of the dispatch milestone):
   1. Dave decides 4.2 (lock the accepted money, or keep D-109) and 4.1 (preview the fuel added).
   2. One `PriceInput` builder (4.3), lines with their workings (4.4).
   3. The migration runner's rebuild support (1.5), then one rebuild migration for the lookups (2.1).
   4. Inside the dispatch milestone itself: `LiveStage` and the `PlannedFlightModel` split (3.1, 3.2), the accepted
      state and reopening New Flight (4.5), and 4.6 and 4.7.
4. **Before Journeys:** tracks in the database with a backfill (2.3), the `LiveFlight` object (3.3), the
   `Recorder::run` split with a re-record test (3.5). The `lease` table if D-218 goes per-hour (2.2).
5. **Any time, a little at a time:** the docs (8.1 to 8.5; 8.2 needs Dave's call first); the dead code and the second
   state machine (3.7, 3.8); the shared windows and accessors (7.1, 7.2); the rest of §6 and §7.
