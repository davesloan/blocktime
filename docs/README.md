# Docs index

| File | What it's for |
|---|---|
| [design.md](design.md) | Full product design: how the airline works, rules, numbers |
| [recorder.md](recorder.md) | Spec for the flight recorder inside the app: events, phases, milestones, event log format |
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
