# Docs index

| File | What it's for |
|---|---|
| [design.md](design.md) | Full product design: how the airline works, rules, numbers |
| [recorder.md](recorder.md) | Spec for the flight recorder inside the app: events, phases, milestones, event log format |
| [database.md](database.md) | The airline database: tables, keys, rules and the entity diagram (D-078) |
| [debrief.md](debrief.md) | The debrief (milestone 10): procedure checks, landing stars, the score and Claude's review (D-156 to D-162) |
| [dashboard.md](dashboard.md) | The dashboard (Live Flight Screen) redesign after the MVP, with sketches in `images/` (D-150 to D-154) |
| [roadmap.md](roadmap.md) | Milestones from here to the MVP (the complete flight flow) and after |
| [decisions.md](decisions.md) | Append-only log of decisions and why they were made |
| [progress.md](progress.md) | Session-by-session progress log (newest first) |
| [qt-ui-test-context.md](qt-ui-test-context.md) | Why the Qt UI test builds were run, what Dave wanted, and the result: Qt won (D-050, D-063) |
| [plans/](plans/) | Implementation plans, one per milestone or test build |
| [notes/](notes/) | Small how-to and quirk notes discovered while building |

## Notes

_Add a link here for every file created in `notes/`._

- [simconnect-setup.md](notes/simconnect-setup.md) — where the SDK lives, linking (/MD!), pack(1), dispatch quirks
- [little-navmap-reference.md](notes/little-navmap-reference.md) — what we learned from Little Navmap's SimConnect code
- [msfs-observed-behaviour.md](notes/msfs-observed-behaviour.md) — facts from our own sim tests (loading, pause, scenery freezes, PMDG fuel/brake, AGL quirks)
- [msfs-junk-data.md](notes/msfs-junk-data.md) — junk samples while loading / at 0,0 and the filter for them
- [qt-setup.md](notes/qt-setup.md) — Qt install, build quirks (C4702, windeployqt), map tiles, QML gotchas, hot reload
- [airport-data.md](notes/airport-data.md) — the airport import's sources, licences, columns and filter results
- [simbrief-airframes.md](notes/simbrief-airframes.md) — what a SimBrief airframe holds, where its ID is, and N814SX's (BBJ2) checked weights
- [pmdg-variables-from-spad.md](notes/pmdg-variables-from-spad.md) — PMDG 737/777 data values and switch events from Dave's SPAD.neXt profiles; the milestone 6 probe's target list
- [simbrief-api.md](notes/simbrief-api.md) — the dispatch page's parameters, the OFP fetch (don't poll it), JSON v2 fields
- [sayintentions-api.md](notes/sayintentions-api.md) — the two keys, getWX's real answer, assignGate and the ATC log for later
- [probe-flight.md](notes/probe-flight.md) — `--probe`: GSX, PMDG switches and airport facility data for milestones 7–10; how to build it with the PMDG SDK headers
