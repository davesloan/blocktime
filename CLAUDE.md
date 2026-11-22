# CLAUDE.md

Guidance for Claude Code working in this repository. Read this file first, every session.

## What this project is

A personal virtual airline (VA) for Microsoft Flight Simulator 2020, built for one user (Dave) and published
as open source (GPL-3.0 compatible). Name: **BlockTime** (the recorder is "BlockTime Recorder").

Two programs live in this repo:

| Folder | What | Language | Status |
|---|---|---|---|
| `/recorder` | Native Windows app that connects to MSFS through the SimConnect SDK, detects flight events and phases, and writes them to a local log | C++ | **Building first** |
| `/app` | Local web app (runs on localhost, no hosting, no login) that runs the airline: fleet, rotations, background flights, money, dashboards | Node.js + TypeScript | Later |

The full product design is in `docs/design.md`. The recorder's spec is in `docs/recorder.md`.
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

`/recorder/README.md` (and later `/app/README.md`) must always contain working, copy-pasteable steps to
build and run from a clean checkout. Update them whenever the steps change.

## Working style

- **Ask before big, hard-to-reverse choices** (overall architecture, a heavy dependency, changing a data
  format that's already written to disk). Make small reversible choices yourself and log them.
- **Small, verifiable steps.** Get something running end-to-end early, then grow it.
- **Never invent sim behaviour.** If you aren't sure how a SimVar or SimConnect call behaves, say so, write a
  small probe or test, and record the result in `docs/notes/`.
- **Dave tests in the sim.** You can't run MSFS. When a change needs in-sim verification, say exactly what
  to check and what result to expect, and record unverified items in `docs/progress.md`.
- **Commit messages** explain why, not just what. Reference decision IDs (e.g. `D-014`) when relevant.

## Recorder ground rules (summary — details in `docs/recorder.md`)

- The recorder **reports facts**; it does not do airline logic (money, loads, schedules). Those belong to
  the app.
- **Write every event to the local log file first**, then (later) deliver it to the app. Nothing may be lost
  if the app isn't running.
- **Be light on the sim.** Request only the SimVars needed, at the lowest rate that works; go fast only
  briefly (e.g. around touchdown).
- Flights start at **parking brake release at the gate** and end at **parking brake set at the destination**.
- Phase detection follows the rules in `docs/recorder.md` (forward-only phases, plan-aware climb/cruise).
- Must survive a sim crash or recorder restart mid-flight without corrupting or silently dropping the flight.

## Tech conventions

### Recorder (C++)
- C++20, MSVC (Visual Studio 2022 toolchain), CMake as the build system.
- Third-party libraries via vcpkg (manifest mode, `vcpkg.json`) unless a decision says otherwise.
- SimConnect from the MSFS 2020 SDK (typically `C:\MSFS SDK\SimConnect SDK`). Don't commit SDK files;
  locate them via a CMake variable/environment variable and document it in `/recorder/README.md`.
- Little Navmap (GPL-3.0, C++) may be used as a reference or source for SimConnect handling and edge cases.
  If code is copied or adapted, keep the license header/attribution and log it in `docs/decisions.md`.
- Keep the UI minimal (tray icon / small status). All rich visuals live in the web app.

### App (Node/TypeScript) — later
- TypeScript, runs locally, SQLite single-file database.
- Airline catalog/configuration in YAML; SimBrief is the source of truth for airframe weights/limits.

## Repo map

```
CLAUDE.md            ← you are here
docs/
  README.md          ← index of all docs
  design.md          ← the full product design (what the airline does and why)
  recorder.md        ← recorder spec (events, phases, rules)
  decisions.md       ← decision log (append-only)
  progress.md        ← session log (newest first)
  notes/             ← small how-to / quirk notes discovered while building
recorder/            ← C++ Windows recorder (to be created)
app/                 ← Node/TS web app (later)
```
