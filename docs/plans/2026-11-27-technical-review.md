# Technical review of 0.16.0 — brief

For a fresh Claude Code session. Agreed with Dave in the brainstorm of 2026-11-27 (roadmap, "Next, after the MVP").
The MVP is done (milestone 15, 0.16.0); this review comes before any new feature work.

## Goal

An honest, in-depth read of the whole code base (about 43,000 lines across `src/`, `qml/` and `tests/`) as it
stands after fifteen milestones built quickly, one session each. Find what would hurt the next stretch of work
(dispatch, the money item, Journeys, later rotations and background flying), not polish for its own sake.

This is a review, not a refactor: **write findings, don't fix them.** Dave decides what gets fixed and when.

## What to look at, in priority order

1. **What can't be undone (D-174):** settling a flight, the ledger, stored measurements, migrations, the event
   log's "nothing lost" rule and crash recovery. Transactions, rounding to $5 (D-080), writes that could happen
   twice or not at all, history rows that get updated.
2. **The database against the rules in `CLAUDE.md` (D-078, D-090):** schema, foreign keys and `ON DELETE`,
   `STRICT`/`NOT NULL`/`CHECK`, indexes on foreign keys and real queries, polymorphism, money and units. Does
   `docs/database.md` still match the migrations?
3. **Architecture and boundaries:** `blocktime-core` free of Qt and SimConnect; the recorder thread never touching
   Qt objects; logic in plain C++ and QML only displaying. Look for logic that has crept into QML or Qt models, and
   models that have grown too large (the planned flight, header and in-flight models in particular).
4. **Readiness for what's next:** how New Flight, the planned flight and pricing are wired, since dispatch (D-214)
   rebuilds them and the money item (D-216 to D-218) changes the rates; whether airplane state and flights can carry
   journeys (design.md §16) and later rotations without a rewrite.
5. **Threading, lifetime and failure handling:** the recorder thread, queued calls, the web calls (SimBrief,
   SayIntentions, Claude's debrief) and what happens when they fail or are slow.
6. **Tests:** where coverage is thin on the priorities above; tests that test the mock, not the code; slow or
   flaky tests.
7. **Duplication and dead code:** parsing done twice, helpers copied between models, code left from superseded
   decisions.
8. **Docs against code:** `design.md`, `recorder.md`, `dashboard.md`, `database.md` and `README.md` statements that
   are no longer true.
9. **Performance budget:** anything that risks the 100 MB / 1% CPU limits in flight.

Don't reopen settled decisions in `docs/decisions.md`; if one looks like it now causes trouble, say so as a
finding and name the decision.

## The Principal Engineer's read (Dave, 2026-11-27)

On top of the priorities above, review the code and the design as a Principal Engineer would: someone who has seen
many code bases age and has no stake in this one. Dave wants that engineer's honest assessment, not a polite one.

- **Design principles:** DRY, SOLID (single responsibility, open/closed, Liskov, interface segregation,
  dependency inversion), cohesion and coupling. Name the classes and functions that break them and say what it costs.
- **Object-oriented design and patterns:** are the right patterns used where they help (and only there)? Are class
  hierarchies, ownership and lifetimes clear? Value types where they fit, RAII, `const`-correctness, modern C++20
  used well.
- **Algorithms and data structures:** the right container for each job, needless copies and allocations, linear
  scans that should be lookups, work repeated every sample or every frame that could be done once.
- **Balance, both ways:** flag over-abstraction (interfaces with one implementation and no reason, layers that only
  forward calls, generality nobody uses) as firmly as under-abstraction. Flag places where a design rule (the
  database rules, "QML only displays", the library split, the event log) has been followed so literally that the
  code became tangled or hard to follow; say which rule and what a sensible engineer would do instead.
- **Efficiency:** CPU and RAM, beyond the 100 MB / 1% budget: hot paths during a flight (the recorder loop, the 1 Hz
  models, gauge and map updates), startup, database queries, QML bindings that re-evaluate too often, memory held
  that needn't be.
- **Everything else an experienced engineer watches for:** naming, function and file size, error handling that
  hides failures, magic numbers, hidden global state, testability, build hygiene.

In the review document, add a section **"Principal Engineer's assessment"** before the findings: an honest overall
assessment in plain prose (what's good, what's worrying, what they'd change first if this were their team's code),
and a **letter grade (A to F)** for the code base overall, plus a grade for each of: architecture, code quality and
design principles, data model, efficiency, tests, and docs. Give one or two sentences of reasoning per grade. The
findings from this read go in the findings list like any other, with the same severities. Print the assessment and
the grades in the reply to Dave as well.

## How

- Build and run the tests first (debug and release) and note any warnings, qmllint output or failures.
- Read by area, not file by file. Use `requesting-code-review` (CLAUDE.md, D-033) and check each finding against the
  code before writing it down.
- No sim test. Replays and `--demo` are fine for checking a suspicion.

## Output

`docs/reviews/2026-11-27-technical-review.md`, linked from `docs/README.md`:

- A short summary: overall health, the three to five things that matter most.
- Findings grouped by the areas above, each with: severity (**Critical** = can lose or corrupt money or history;
  **Important** = will cost real time in the next milestones or hides a bug; **Minor**), the file and line, what's
  wrong, a concrete failure scenario where there is one, and a suggested fix with a rough size (small / medium /
  large).
- A suggested order for fixing, as a short list Dave can approve or cut.

Then a `docs/progress.md` entry, and in the reply to Dave the summary and the Critical and Important findings,
printed in full (CLAUDE.md, "Print it, don't point to it"). Commit the review (with this brief and the 2026-11-27
brainstorm's doc changes, D-214 to D-218 and the roadmap, if they aren't committed yet) and push `main`.
