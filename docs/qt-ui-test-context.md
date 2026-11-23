# Qt UI test: why we're running it

Context from a brainstorming chat with Dave on 2026-11-22. Read this before working on the Qt UI test builds
(D-050, D-051) or before deciding what comes after them. It records how we got here, what Dave cares about, and
what the test has to answer, so a future session doesn't have to rediscover it.

Status: **UI test 1 passed in the sim (2026-11-22). UI test 2 built (2026-11-22), waiting on Dave's sim checks
and the decision.** The work is on the branch `qt-ui-spike` (Dave's name; D-051 first said `spike/qt-ui`). It's
built inside the recorder project so it can be merged as it is if Qt wins (D-052). Plans:
`docs/plans/2026-11-22-qt-ui-test-1.md`, `docs/plans/2026-11-22-qt-ui-test-2.md`. D-001 (two programs) and D-004
(local web app + SQLite) stay in force until the test's decision point.

**Test 2 findings so far (without the sim):**
- **Route map: yes, Qt can draw a decent one.** Qt Location with CARTO Dark Matter tiles (D-059) shows the flown
  track, start and end dots, and the credit line, in the dark theme. Getting there took one probe (how to pass the
  key), one cache gotcha (a wrong key's tiles would outlive a fixed key), and one QML bug (a change handler read a
  binding that hadn't updated yet, so the map fitted the previous flight).
- **Data screens:** the Flights list and the sortable Fleet table (TableView + HorizontalHeaderView, from SQLite)
  each took one pass plus one layout fix. The data logic is plain C++ with 31 new GoogleTest tests; QML only
  displays.
- **Memory:** Task Manager's Memory column shows 31 MB on Live and 44–46 MB after the map and the Fleet table
  (release, `--demo`). The map adds about 15 MB. CPU stays at 0%.
- **Back-and-forth:** 4 fixes across the 7 tasks, each found by Claude from screenshots and fixed in one pass. The
  four were Live tile labels cut off by the sidebar, a person-like Flights icon, the stale-binding map fit, and the
  Fleet error layout. None needed Dave.

**Test 1 result: passed all four criteria** (Dave's flight, KSAN → KSAN in the 737-800, 2026-11-22):
- **Memory:** never above **30 MB** in Task Manager during the flight (limit 100 MB). Task Manager's Memory column
  is the private working set. The 87–92 MB measured at idle without the sim was working set / private bytes,
  which also count shared and committed pages, so the two aren't the same measure.
- **CPU:** never above **0.2%** during the flight (limit 1%). At idle, 0% over 10 s. Nothing animates; only the
  1 s block clock ticks.
- **Look:** Dave: "beautiful and exactly what I was looking for."
- **Recording:** block out, block in and the clean stop on window close all logged exactly as the console
  recorder does.
- **Change → see-it loop:** in debug builds, saving a QML file reloads the window in about 1.5 s with no rebuild
  (D-055). A C++ change needs a rebuild of the app (not timed yet).
- **QML back-and-forth:** the first full build of the screen needed 4 fixes: a property named `onAccent` (QML
  reads `on<Capital>` as a signal handler), `width`/`height` on an item inside a layout (qmllint caught it), and
  C4702 warnings in Qt's generated code. The fourth was hot reload not seeing the module's C++ types until each
  file imported `BlockTime`. Each was found and fixed in one pass.

## The question

Should BlockTime stay two programs (the C++ recorder plus a local Node/TypeScript web app shown in a browser,
D-001), or become **one native Windows app** built in **C++ with Qt 6 and a QML interface**, with the recorder
running inside it?

## How we got here

1. **Rails + React + Postgres (dropped).** Dave first wanted the web app in Rails, so he'd work in a language he
   knew. Dropped because:
   - Dave decided familiarity doesn't matter: it's been long enough that Rails would feel new anyway, and Rails is
     slowly leaving the industry.
   - Rails really wants WSL2 on Windows, and **Dave won't run a VM while he's in the sim.**
   - The idea of the recorder writing straight into a shared database was also questioned: it couples the
     recorder to a schema the app owns and migrates, and needs the database up during every flight. The
     log-first rule (D-008) avoids that. A shared SQLite file also can't be used across the Windows/WSL2 boundary
     (WAL mode needs both programs on the same machine).
   - Postgres itself wouldn't hurt frame rates (it's idle at this workload); that wasn't the reason.
2. **One app, like Sim Toolkit Pro.** Dave asked whether the recorder and app need to be separate at all, and
   whether Claude Code could build a single native app with everything in it. Yes, it can.
3. **Options compared** (memory figures are rough estimates, not measured):

   | Option | Memory | Notes |
   |---|---|---|
   | C++ + Qt 6 + QML, one app | ~40–80 MB | Lightest. Keeps the milestone 1–3 recorder code. Slower UI work; Claude is less fluent in QML than React; Qt map support unproven for us |
   | C# + .NET (WPF/WinUI), one app | ~100–150 MB+ | Easier language, but garbage-collector pauses and a rewrite of the recorder |
   | Native shell + WebView2 + React | ~150–300 MB | Fast UI work and a proven map (MapLibre), but Chromium-level memory, two toolchains, and a C++↔JS message bridge to maintain. Basically the current plan in one window |
   | Current plan (recorder + Node app in a browser) | ~60–100 MB Node + browser tab | Fastest UI work and lowest risk. Either half restarts without touching the other, so the app can be reworked mid-flight. Needs the recorder↔app link built |

4. Claude's recommendation was the **current plan**: every option is tiny next to MSFS, and being able to restart
   the app while flying matters a lot for a project built while it's being used. **Dave is leaning toward Qt**
   and wants to find out with a real build rather than decide on paper.

## What Dave wants (in his words, condensed)

- **Least resources** possible next to the sim, PMDG and SayIntentions. This was his first priority.
- A UI in the style of **FlyHub and Sim Toolkit Pro**: modern, polished. It must not look like an
  "Intro to Programming" desktop app.
- **Dark mode only.**
- His hesitation about Qt: it will **take longer to reach an MVP**, and **Claude Code is less proficient** in
  C++/QML than in web tech.

## The test

Two builds on `qt-ui-spike`, light on tests and with no code review until Dave decides. They're written to be kept:
if Qt wins they get their tests and a review, then merge (D-051). Milestone 4 (phases) can
carry on on `main` in parallel; merge `main` into the branch now and then.

**UI test 1: Qt shell and live data**
- A Qt 6 app linking the existing `blocktime-core` library, with the recorder on its own thread.
- A dark-only theme file and the first building blocks (card, stat tile, button).
- A live flight panel: altitude, speed, fuel, parking brake, and block events as they happen.
- Answers: how well Claude handles QML; how slow the change/rebuild loop feels; CPU and memory next to the sim
  (Dave checks Task Manager during a real flight).

**UI test 2: map and data screens**
- A post-flight route map: the track flown, drawn from the recorded positions. This is the map that matters.
  A live position on the same map is a bonus if it comes cheaply.
- A fleet list read from SQLite.
- A side menu between screens, so it feels like FlyHub / Sim Toolkit Pro.
- Answers: can Qt draw a decent route map; how much work are data-heavy screens.

**Setup:** Qt from the official Qt online installer (free Qt account), not vcpkg (building Qt takes hours).

**Ways to keep the risk low if Qt wins** (Claude's proposals):
- Airline logic (money, rotations, loads, detection) in a plain C++ library with no Qt UI code, tested with
  GoogleTest. QML only displays. If QML ever disappoints, the UI can be replaced without rewriting the logic.
- A small QML component kit first (theme + building blocks), with QML conventions added to CLAUDE.md, and Qt's
  live QML preview for faster UI changes.

## Deciding afterwards

Pass criteria (confirmed by Dave, 2026-11-22):
- Claude produces working, good-looking QML screens without endless back-and-forth.
- The change → see-it loop is tolerable for day-to-day UI work.
- During a real flight the app stays **under 100 MB of memory and under 1% CPU** (Task Manager).
- Dave likes how it looks.

The map is **not a must-have**. Dave uses Navigraph for live position. What he wants from a map is **the route he
flew, shown in the post-flight summary**; a live map is a bonus. If Qt can't do a route map, that's acceptable but
annoying, so it counts against Qt without failing it.

Then:
- **Qt wins:** add tests and run a code review on the branch, merge it, write a decision superseding D-001 and
  D-004, and rework milestones 4–8 (milestone 8, "delivery to the app", mostly disappears).
- **Qt loses:** delete the branch, keep the current plan, and record what was learned here and in the log.

## Other topics from the same chat (for later)

- **Recorder → app link (only matters if the current plan stays):** Claude suggested the app reads the JSONL log
  as it grows (the log stays the only record of events), plus a localhost WebSocket with the recorder as server,
  for 1 Hz live data and for the app to send the recorder the leg being flown. Log lines would need a sequence
  number or event ID. Not decided. Who fetches SimBrief (recorder or app) is also open.
- **GSX Pro:** integration looks possible. GSX exposes service states (boarding, deboarding, refuelling,
  catering, departure/pushback) and passenger counts as L:vars, which SimConnect can read since SU12. Ideas:
  a turnaround timeline per leg, measured turn times feeding background rotations, a cleaner pushback phase,
  "deboarding complete" as an end-of-flight trigger, and debrief flavour (never penalties). Read only. Writing
  loads to GSX isn't needed if loads go through SimBrief. Needs a probe flight first. Not decided.
- **Navigraph:** considered for airport data and procedures, then **dropped** as too complicated. Notes: the
  developer API is free but needs Navigraph's approval; copying the navdata file installed for Sim Toolkit Pro
  (or PMDG, or Little Navmap) is against Navigraph's terms. Airport basics (coordinates, runways, elevation) can
  come from free data such as OurAirports.
