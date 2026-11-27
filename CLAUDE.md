# CLAUDE.md

Guidance for Claude Code working in this repository. Read this file first, every session.

## What this project is

A personal virtual airline (VA) for Microsoft Flight Simulator 2020, built for one user (Dave) and published
as open source (GPL-3.0 compatible). Name: **BlockTime**.

**One native Windows app** (D-063), `BlockTime.exe`, built from the repo root: C++20, Qt 6 with a QML interface.

- **The recorder** (`Recorder`, D-053) runs on its own thread inside the app. It connects to MSFS through the
  SimConnect SDK, detects flight events (block out/in; phases next) and writes them to a local event log first.
  Milestones 1–3 are built.
- **The airline** (fleet, rotations, background flights, money) is plain C++ on a SQLite database
  (`docs/database.md`), built milestone by milestone (`docs/roadmap.md`). Milestone 4 built the foundation: config
  files, the database, the airport import (`blocktime-import.exe`), leasing and the ledger. Milestone 5 added New Flight
  (loads, the money preview, `pricing.h`), the SimBrief dispatch page and plan import, and SayIntentions' runways,
  behind a `Planner` interface for the two web calls. The app has four screens: Live (with the planned flight
  before departure), Flights (past flights with the route on a dark map), Fleet (with New Flight) and Money.
- **QML only displays.** Logic lives in plain C++ with GoogleTest tests, so the UI could be replaced without
  rewriting it.

The full product design is in `docs/design.md`. The recorder's spec and milestones are in `docs/recorder.md`.
Do not re-decide things that are already in `docs/decisions.md` without asking Dave.

## Start-of-session checklist

1. Read `CLAUDE.md` (this file).
2. Read the latest entries in `docs/progress.md` to see where work stopped and what's next.
3. Skim `docs/decisions.md` for anything relevant to today's task.
4. Read the spec doc for the area you're touching (`docs/recorder.md`, `docs/design.md`).

## Documentation duties (non-negotiable)

This repo documents itself as it is built, so that any future session, with no memory of earlier ones, can
pick up the work. Treat the docs as part of the deliverable, not an afterthought.

### 1. Decision log — `docs/decisions.md`

Add an entry whenever a choice is made that a future developer would otherwise have to rediscover or might
undo by accident. That includes:

- Choices Dave makes in conversation.
- Choices **you** make on your own while implementing (library picks, file formats, data structures,
  algorithms, thresholds, naming schemes, error-handling strategies, anything with a plausible alternative).
- Reversals: never delete an old decision; add a new entry that supersedes it and mark the old one
  `Superseded by D-NNN`.

Use the template at the top of that file. Keep entries short: what was decided, why, what else was
considered, and who decided (`Dave`, `Claude`, or `Dave + Claude`). If you made a call Dave hasn't seen,
mark it `Status: Proposed` and mention it in your end-of-task summary so he can confirm or change it.

### 2. Progress log — `docs/progress.md`

At the end of every working session (or after any significant chunk of work), append an entry: what was
done, what's working, what's broken or unverified, and the concrete next step. Newest entries at the top.

### 3. Keep the specs true

If implementation changes how something works, update `docs/design.md` or `docs/recorder.md` in the same
change. A spec that disagrees with the code is a bug.

### 4. How-to knowledge

When you figure out something non-obvious about the environment (SimConnect quirks, PMDG behaviour,
build setup, where the MSFS SDK lives, a SimVar that doesn't behave as documented), write it down in
`docs/notes/` as a short topic file (e.g. `docs/notes/simconnect-quirks.md`) and link it from
`docs/README.md`. Prefer many small, findable notes over one giant file.

### 5. Build and run instructions

The root `README.md` must always contain working, copy-pasteable steps to build, test and run from a clean
checkout. Update it whenever the steps change.

## Working style

- **The MVP is for Dave, not production (D-099).** Code that reads the hand-edited config files gets the happy path
  only: no drift warnings, extra validation or edge-case fixes. Spend tests and error handling on what can go wrong
  in a real flight (the sim, network calls, money).
- **Ask before big, hard-to-reverse choices** (overall architecture, a heavy dependency, changing a data
  format that's already written to disk). Make small reversible choices yourself and log them.
- **Small, verifiable steps.** Get something running end-to-end early, then grow it.
- **Never invent sim behaviour.** If you aren't sure how a SimVar or SimConnect call behaves, say so, write a
  small probe or test, and record the result in `docs/notes/`.
- **Avoid in-sim tests after the MVP (D-174).** You can't run MSFS, and Dave's time in the sim is expensive. Ask for a
  sim test only for a SimVar or SDK field nobody has read yet (a probe), the first run of an entirely new system that
  talks to the sim, or a bug that only shows up there. Otherwise test against recorded flights: the fixtures in
  `tests/data/` and Dave's event logs (kept 30 days; copy a flight that shows something new into `tests/data/`).
  Check app changes yourself first (`--demo`, `--grab`, `blocktime-debrief`). Instead of a test run, list "on your next
  flight you should see …" in the reply, and record unverified items in `docs/progress.md`.
- **High tolerance for bugs, except money and history (D-174).** Dave reports what looks odd and you fix it from the
  event log. In the app or the sim, test the normal flow and the happy path; edge cases belong in unit tests, and
  only where a bug would cost more than a quick fix. Keep careful tests for what can't be undone: settling, the
  ledger, stored measurements, migrations and the event log's "nothing lost" rule.
- **Print it, don't point to it.** When work is ready for Dave to test (in the sim or not), print the numbered test
  steps with expected results in the reply itself. When decisions or questions wait on him, list each one there
  with enough context to answer it, not just its D-number. They also go in `docs/progress.md`, but the reply must
  stand on its own.
- **Commit messages** explain why, not just what. Reference decision IDs (e.g. `D-014`) when relevant.

## Superpowers skills (D-033)

This file overrides the plugin's "invoke a skill on any 1% chance" rule. Use only these:

- **Now (milestone 1 fixes):** `systematic-debugging` for bugs found in sim tests.
- **From milestone 2 on:** `writing-plans` and `executing-plans` for each milestone (save plans under
  `docs/plans/`), `test-driven-development` for logic that can be tested without the sim (phase and block
  detection, log writing, crash recovery), and `verification-before-completion` before saying work is done.
  Without the sim, "done" means it builds with no warnings and the tests pass; in-sim checks still go to
  Dave via `docs/progress.md`.
- **Before each milestone's final commit:** `requesting-code-review`, then `receiving-code-review` for the
  findings. No pull requests and no feature branches: work is committed straight to `main` (D-038).
- **One Claude Code session per milestone (D-121).** The session that finishes a milestone pushes `main` to `origin`;
  the next milestone starts in a new session from the latest `docs/progress.md` entry.
- **Sometimes:** `brainstorming` for undesigned areas (mainly the airline side of the app), but start from
  `docs/decisions.md` and don't reopen settled decisions.
- **Skip:** `using-git-worktrees`, `finishing-a-development-branch`, `subagent-driven-development`,
  `dispatching-parallel-agents`. This is a one-person project that works on `main`.

## Recorder ground rules (summary — details in `docs/recorder.md`)

- The recorder **reports facts**; it does not do airline logic (money, loads, schedules). Those belong to
  the app.
- **Write every event to the local log file first** (D-008); the rest of the app reads it back (D-060). Nothing
  may be lost, even if the UI misbehaves.
- **Be light on the sim.** Request only the SimVars needed, at the lowest rate that works; go fast only
  briefly (e.g. around touchdown).
- Flights start at **parking brake release at the gate** and end at **parking brake set at the destination**.
- Phase detection follows the rules in `docs/recorder.md` (forward-only phases, plan-aware climb/cruise).
- Must survive a sim crash or app restart mid-flight without corrupting or silently dropping the flight.
- **Only the PMDG 737 and 777 families on MSFS 2020** (D-039). Don't test or add support for other aircraft.

## Tech conventions

### C++ and build
- C++20, MSVC (Visual Studio 2022 or newer; developed on VS 18 / 2026), CMake presets + Ninja, one build at the
  repo root (D-064). No warnings at `/W4 /permissive- /utf-8`.
- Libraries are split by what they may depend on: `blocktime-core` (plain C++, no SimConnect, no Qt: logic,
  event log, flight history, settings, the airline database, config files, airport import, leasing), `blocktime-sim` (the recorder loop and SimConnect), and the app
  (Qt models and QML). New airline logic goes in `blocktime-core` or a new plain library, with tests.
- Third-party libraries via vcpkg (manifest mode, `vcpkg.json`) unless a decision says otherwise. SQLite through
  the sqlite3 C API, not Qt SQL (D-061).
- SimConnect from the MSFS 2020 SDK (typically `C:\MSFS SDK\SimConnect SDK`). Don't commit SDK files;
  locate them via a CMake variable/environment variable and document it in `README.md`.
- Little Navmap (GPL-3.0, C++) may be used as a reference or source for SimConnect handling and edge cases.
  If code is copied or adapted, keep the license header/attribution and log it in `docs/decisions.md`.
- Airline data in one SQLite file; airline catalog/configuration in YAML; SimBrief is the source of truth for
  airframe weights/limits.

### Qt and QML
- Qt 6 from the official installer (not vcpkg), found through `BLOCKTIME_QT_DIR` (D-052). windeployqt runs after
  each build (D-056).
- Dark only. Every colour, size and font is in `qml/Theme.qml` (D-054); Qt Quick Controls Basic style. No
  continuous animations: the budget is under 100 MB and under 1% CPU in flight. The exceptions are a lamp flashing
  while its switch is wrong (D-195) and the gauge needles gliding from one 1 Hz sample to the next (D-205).
- Every QML file starts with the GPL header and `import BlockTime` (needed by hot reload, D-055). Use
  `pragma ComponentBehavior: Bound` and `required` properties in delegates. `qmllint`
  (`blocktime-app_qmllint`) must report nothing.
- C++ talks to QML through models and the `RecorderBridge`, passed in as initial properties; the recorder thread
  never touches Qt objects directly (queued calls only).
- Check UI changes with the debug build's hot reload and `--demo`. QML gotchas are in `docs/notes/qt-setup.md`.
- Secrets (the CARTO key) live in `%LOCALAPPDATA%\BlockTime\settings.json`: never commit, log or print them.

### Database (D-078)

Proper relational design matters to Dave. The schema enforces the rules; code doesn't stand in for the database.

**Apply the rules for what they protect, not to the letter (D-090).** They exist to stop real bugs: relations the
database can't check, values whose meaning is ambiguous (a NULL that could mean "unknown" or "doesn't apply"), rows
that contradict each other, money counted twice. Before adding structure for a rule (extra tables, composite keys,
triggers), ask whether it stops a bug the code could realistically make. A value written by one validated code path
is better guarded by a test than by schema machinery. A plain "no" is a 0, not a NULL, and an attribute that applies
to every row (a type's role) isn't polymorphism.

- **Normalised** (at least third normal form). **Atomic columns:** no lists, delimited strings or JSON blobs for
  data that is queried or joined. Exception: a verbatim copy of a source document, such as a stored SimBrief OFP,
  kept as-is next to the normalised fields taken from it.
- **Keys:** every table has a primary key (`INTEGER PRIMARY KEY` surrogate unless a natural key is truly stable).
  Natural identifiers get `UNIQUE` constraints (airplane registration, airport ICAO code).
- **Relations are foreign keys,** with an explicit `ON DELETE` rule. `PRAGMA foreign_keys = ON` on every
  connection (SQLite has it off by default); a test checks it.
- **No code-enforced polymorphism.** No `owner_type` + `owner_id` columns, and no "type" column with columns that
  only apply to some types. Use separate tables, or a supertype table with subtype tables keyed by the same id. If
  a row may point to one of several parents, use separate foreign keys with a `CHECK` that exactly one is set.
- **Types and rules in the schema:** `STRICT` tables; `NOT NULL` by default; `CHECK` for ranges and small fixed
  sets; a lookup table with a foreign key for sets that may grow.
- **Units:** money in integer cents, weights in integer pounds, times in UTC (one representation, chosen in
  milestone 4's plan and recorded in `docs/database.md`). No floating-point money. Every money line is a multiple of $5, rounded when posted (D-080).
- **Indexes:** on every foreign key column (SQLite doesn't add them) and on the columns real queries filter or sort
  by. Check important queries with `EXPLAIN QUERY PLAN` in tests.
- **History is facts:** completed flights and ledger lines are written once and never updated. Corrections are new,
  reversing lines.
- **Atomic changes:** anything that changes several tables together (settling a flight: money, location, fuel,
  hours) runs in one transaction.
- **Migrations:** numbered, each in a transaction, tracked with `user_version`. Tests build an empty database
  through every migration.
- **`docs/database.md`** holds the schema overview and an entity diagram. Keep it true, like the other specs.

## Repo map

```
CLAUDE.md            ← you are here
README.md            ← build, test and run steps
CMakeLists.txt       ← the one build (CMakePresets.json, vcpkg.json next to it)
config/              ← catalog.yaml and airline.example.yaml (copied to %LOCALAPPDATA%\BlockTime)
src/                 ← C++: recorder, airline logic, Qt models, app_main.cpp, import_main.cpp (blocktime-import)
qml/                 ← the QML interface (Theme.qml holds the look)
icons/               ← the app icon (.ico for the exe, .svg for the sidebar; D-169)
tests/               ← GoogleTest: blocktime-tests (plain C++) and blocktime-qt-tests (Qt models)
docs/
  README.md          ← index of all docs
  design.md          ← the full product design (what the airline does and why)
  database.md        ← the airline database schema and its processes
  roadmap.md         ← milestones to the MVP
  recorder.md        ← recorder spec (events, phases, milestones, event log format)
  decisions.md       ← decision log (append-only)
  progress.md        ← session log (newest first)
  plans/             ← implementation plans
  notes/             ← small how-to / quirk notes discovered while building
```
