# BlockTime

A personal virtual airline for **Microsoft Flight Simulator 2020** — a lightweight alternative to tools like FlyHub,
built around the parts that make flying feel meaningful and none of the busywork.

You lease aircraft, build rotations, and fly legs yourself while the rest of your fleet keeps flying in the background.
Every flight you fly is recorded automatically, from parking brake release to parking brake set.

> **Status:** early development, version 0.6.0. The app records flights (block out / block in), shows live flight
> data and past flights with the route flown on a dark map, and runs the airline's foundation: your airline and
> leased airplanes from config files, the monthly lease charges, and the money ledger. New Flight sets up a leg with
> its loads and money preview, opens a pre-filled SimBrief dispatch page with SayIntentions' runways, and imports
> the plan. Recording that planned flight and settling its money come next (`docs/roadmap.md`).

This project is built for one pilot's own use and shared in case others want to tinker. It isn't designed to be easy to
set up, and there's no support — but you're welcome to clone it and play.

## How it works

- **Start small.** $5M in the bank and one aircraft slot. Lease your first plane and take delivery from the factory —
  or pay for instant delivery.
- **Planes stay where they land.** You can only fly from where one of your aircraft is parked. No hubs, no gate leases:
  fly anywhere, as long as the runway is long enough.
- **Rotations run in the background.** Put a plane on a loop of legs (e.g. KSAN–KSLC–KPDX–KOAK–KLAS–KSAN) and it flies
  them on its own, in real time, earning money while you're away.
- **Jump in any time.** Take over a plane's next leg and fly it yourself, or grab an idle plane for a free flight
  anywhere.
- **See the outcome before you commit.** Passenger loads, revenue and costs are shown before you push back.
- **Forgiving by design.** No crew scheduling, no maintenance checks, no surprise penalties.
- **Grow by flying.** New aircraft slots unlock with the hours you fly yourself — the airline can't run on autopilot.
- **SimBrief-native.** Aircraft weights and limits come from your SimBrief airframes; fuel is tracked in pounds, plane
  by plane, between flights.

Supported fleet (hand-picked to match what the author flies): PMDG 737-600, 737-800, 737-800 BBJ2, 737-800 BCF,
777-200ER, 777-300ER and 777F.

## Architecture

One native Windows app, `BlockTime.exe`, in **C++20 with Qt 6 and a QML interface** (D-063):

- **The recorder** runs on its own thread inside the app. It connects to MSFS through SimConnect, detects block out and
  block in, and writes every event to a local log **before** anything else, so no flight is lost (`docs/recorder.md`).
- **The airline logic** is plain C++ with unit tests. Data lives in one SQLite file on your PC. There's no hosting and
  no login.
- **The QML interface** only displays: a dark theme, a side menu, and the Live, Flights, Fleet and Money screens.

## Requirements

- Windows 10/11 x64
- Microsoft Flight Simulator 2020
- **MSFS 2020 SDK** (SimConnect). Enable Developer Mode in the sim and use *Help → SDK Installer*. The default location
  is `C:\MSFS SDK\`.
- **Visual Studio 2022 or newer** with the **Desktop development with C++** workload (it includes MSVC, CMake, Ninja
  and vcpkg). Developed on VS 18 / 2026 Community.
- **Qt 6.8 or newer** (developed with 6.12.0) from the official Qt online installer (free Qt account), with the
  **MSVC 2022 64-bit**, **Qt Positioning** and **Qt Location** components.
- An internet connection on the first build (vcpkg downloads `nlohmann-json`, `gtest` and `sqlite3`) and for map
  tiles.
- A free CARTO key for the dark map (see [Keys and IDs](#keys-and-ids)). Without one, tracks are drawn on a plain background.

The build finds the SDKs in this order:

- **SimConnect:** `-DSIMCONNECT_SDK_DIR=<"SimConnect SDK" folder>`, then `%MSFS_SDK%\SimConnect SDK`, then
  `C:\MSFS SDK\SimConnect SDK`.
- **Qt:** `-DBLOCKTIME_QT_DIR=<kit folder>`, then `%BLOCKTIME_QT_DIR%`, then `C:\Qt\6.12.0\msvc2022_64`.

## Build

Open a **Developer PowerShell for VS** (Start menu → "Developer PowerShell for VS"), go to the repo, and run:

```powershell
cmake --preset x64-release
```

```powershell
cmake --build --preset x64-release
```

The app is `build\x64-release\BlockTime.exe`. A post-build step copies Qt's DLLs and plugins next to it, so it runs
from there with nothing else installed (D-056). Use `x64-debug` in place of `x64-release` for a debug build.

To build from a normal PowerShell, load the developer environment first (adjust the path for your Visual Studio
edition or version):

```powershell
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community"; $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"; Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"; Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
```

Notes:
- The presets need `VCPKG_ROOT`. The developer shell sets it to Visual Studio's bundled vcpkg.
- `vcpkg.json` pins a `builtin-baseline`, because the vcpkg bundled with VS won't install anything without one.

## Test

```powershell
ctest --preset x64-release
```

Two test programs run: `blocktime-tests` (the plain C++ logic: block detection, the event log, flight history,
settings, the airline database, config files, airport import, money and leasing) and `blocktime-qt-tests` (the Qt
models and the recorder bridge, without a window). Pass
`-DBLOCKTIME_BUILD_TESTS=OFF` to the configure step to skip them.

## Set up your airline

The airline comes from two config files in `%LOCALAPPDATA%\BlockTime\` (D-076). Copy the repo's examples there:

```powershell
New-Item -ItemType Directory -Force "$env:LOCALAPPDATA\BlockTime" | Out-Null; Copy-Item config\catalog.yaml "$env:LOCALAPPDATA\BlockTime\catalog.yaml"; Copy-Item config\airline.example.yaml "$env:LOCALAPPDATA\BlockTime\airline.yaml"
```

- **`airline.yaml`:** your airline's name, ICAO code and callsign, the day it was founded, its starting cash, its
  fleets, and the airplanes you lease. Each airplane has its own registration and its own SimBrief airframe (the
  "Internal ID" on SimBrief's airframe page, D-087). An airplane is leased the first time the app sees it; from then
  on the database owns it, and editing or removing it in the file changes nothing (a warning goes to `errors.log`,
  D-082).
  Optional per airplane: `flight_number`, the default number of its free flights (re-read at every start, D-106), and
  `limits`, its own values in place of its airframe's (read only when it's leased, D-100). Optional for the airline:
  `fuel_price_per_100lb` (default 41), locked on each planned flight.
- **`catalog.yaml`:** aircraft types (lease rate, family, role) and airframes (seats, holds and `limits`: minimum
  runway, MLW, passenger and bag weight, and max payload, which must match the airplane's SimBrief airframe). Numbers
  marked `estimate` are placeholders until they're read from the PMDG load pages. **Upgrading from 0.5:** copy the
  repo's `catalog.yaml` again, since airframes now need `limits` (keep your own edits).
- Then import the airport data ([Airport data](#airport-data)). Until both are in place, the Fleet page says what's
  missing.

The first month of each lease is charged when it starts, and each later month on the same day of the month, also
while the app is open. A month you can't pay returns the airplane (design.md §5).

## Run

```powershell
.\build\x64-release\BlockTime.exe
```

- Start it before or after MSFS. The pill in the header goes from "Waiting for sim" to "Sim starting" to "Connected".
  MSFS can take several minutes to answer while it loads.
- **Live:** live flight values, the block clock, and an event feed. A flight starts at the parking brake release that's
  followed by 10 m of movement, and ends at a brake set held until every engine is off (`docs/recorder.md`).
- **Flights:** every flight in the last 30 days of the log, newest first, with block times, fuel used, distance,
  highest altitude and the route flown on a dark map. It updates after each block out and block in.
- **Fleet:** your airplanes, with type, airframe, fleet, location, fuel on board and status (Parked, Awaiting
  delivery, Returned), and any notices about the setup above them. Click a column header to sort by it, and click it
  again to reverse. Select an airplane and click **New Flight** to plan its next leg.
- **New Flight:** type the destination's ICAO code (its longest runway must fit the airplane), check the flight
  number, and choose VIP airliner or charter on the BBJ2, or belly cargo on an airliner. An airplane awaiting delivery
  only offers its delivery flight. The loads are rolled once and the money is shown before you commit; fuel is added
  once the plan is in. **Plan flight** gets the active runways from SayIntentions and opens SimBrief's dispatch page,
  filled in, in your browser.
- **Live, before departure:** the planned flight, "Waiting for plan" until you press Generate on SimBrief and then
  **Import Active SimBrief Plan** (the app never polls SimBrief, D-101), then "Waiting for sim". It shows where to load
  in, the route with a copy button, the plan and the money estimate. **Cancel flight** is free.
- **Money:** the cash and every ledger line, newest first: the opening cash and each lease month.
- **Closing the window** stops recording cleanly. An open flight is logged as interrupted (`recorder_exit`), then
  `recorder_stopped`.
- Only one copy records at a time (D-037). A second copy shows "Recorder already running" and records nothing.
- `--log-dir <folder>` writes the event log somewhere else.
- `--demo` shows made-up live values and records nothing, to look at the UI without the sim. The Flights, Fleet and
  Money screens still show your real data, and leases are charged as usual.
- Use the **release** build for flights. The debug build is much heavier.

### Files it uses

Everything lives in `%LOCALAPPDATA%\BlockTime\`, outside the repo:

| Path | What |
|---|---|
| `events\events-YYYY-MM-DD.jsonl` | The event log, one JSON object per line, one file per UTC day (D-020). Event types: `docs/recorder.md` |
| `settings.json` | Your keys and IDs ([Keys and IDs](#keys-and-ids)). Never commit it |
| `airline.yaml`, `catalog.yaml` | Your airline and the aircraft catalog ([Set up your airline](#set-up-your-airline)) |
| `blocktime.db` | The airline database: airplanes, the money ledger, airports (`docs/database.md`). Back it up by copying it while the app is closed. An old Qt test database is renamed `blocktime.test-schema.db` on first start (D-086) |
| `airport-data\` | The downloaded airport files, kept for the next refresh ([Airport data](#airport-data)) |
| `tiles\` | Map tile cache, at most 100 MB. Files older than 30 days are deleted at startup. Safe to delete |
| `errors.log` | Errors that have nowhere else to go: block events that could never be written (`LOST:`, D-049) and event log or SimConnect failures. Normally just one "started" line per run (D-067) |

### Keys and IDs

`settings.json` holds the keys the app uses, read at start. Open it in Notepad (it's created if missing):

```powershell
New-Item -ItemType Directory -Force "$env:LOCALAPPDATA\BlockTime" | Out-Null; notepad "$env:LOCALAPPDATA\BlockTime\settings.json"
```

```json
{
  "carto_key": "...",
  "simbrief_user_id": "...",
  "sayintentions_api_key": "...",
  "sayintentions_developer_key": "..."
}
```

| Key | What it's for, and where to find it |
|---|---|
| `carto_key` | The map's CARTO Dark Matter tiles (D-059). A free personal key from https://carto.com/basemaps/apikey (no account; the key arrives by email link). Choose non-commercial use and don't restrict it to websites. A wrong key shows grey "API KEY REQUIRED" tiles, which the app can't tell from real ones |
| `simbrief_user_id` | Importing your latest SimBrief plan. Your Pilot ID, on SimBrief's Account Settings page. Not the first part of an airframe's Internal ID (`docs/notes/simbrief-api.md`) |
| `sayintentions_api_key` | The active runways for planning. Your pilot API key, on your SayIntentions portal account page |
| `sayintentions_developer_key` | The same. Request a personal developer key at https://p2.sayintentions.ai/p2/#developer-key (`docs/notes/sayintentions-api.md`) |

Without the SayIntentions keys, planning still works and SimBrief chooses the runways. The keys are never logged.

### Airport data

The airline needs airports (where planes park, runway lengths, time zones). They come from three public files, which
`blocktime-import.exe` filters and writes into the database (D-079, D-084, D-094). Download them (about 25 MB in all):

```powershell
$d = "$env:LOCALAPPDATA\BlockTime\airport-data"; New-Item -ItemType Directory -Force $d | Out-Null; Invoke-WebRequest https://davidmegginson.github.io/ourairports-data/airports.csv -OutFile "$d\airports.csv"; Invoke-WebRequest https://davidmegginson.github.io/ourairports-data/runways.csv -OutFile "$d\runways.csv"; Invoke-WebRequest https://raw.githubusercontent.com/mwgg/Airports/master/airports.json -OutFile "$d\airports.json"
```

Then import them:

```powershell
.\build\x64-release\blocktime-import.exe "$env:LOCALAPPDATA\BlockTime\airport-data"
```

It prints how many airports it kept and why the rest were left out. To refresh the data later, run both steps again:
airports are updated in place and never deleted. `--db <file>` imports into another database file.

## Changing the UI

The QML is in `qml/`, and every colour and size is in `qml/Theme.qml`. Run the **debug** build:

```powershell
.\build\x64-debug\BlockTime.exe --demo
```

In debug builds the app reads QML straight from `qml/` and reloads the window about 200 ms after you save a `.qml`
file, with no rebuild (D-055). If the QML has an error, a small error window says so. Fix and save, and the real
window comes back. C++ changes still need a rebuild.

To see QML errors, start it with `QT_FORCE_STDERR_LOGGING` set and stderr captured:

```powershell
$env:QT_FORCE_STDERR_LOGGING = "1"; .\build\x64-debug\BlockTime.exe --demo 2> qml-errors.txt
```

To lint the QML:

```powershell
cmake --build --preset x64-debug --target blocktime-app_qmllint
```

Quirks found while setting this up are in [`docs/notes/qt-setup.md`](docs/notes/qt-setup.md).

## Documentation

| | |
|---|---|
| [`docs/design.md`](docs/design.md) | Full product design: rules, numbers and rationale |
| [`docs/recorder.md`](docs/recorder.md) | Flight recorder spec, milestones and event log format |
| [`docs/decisions.md`](docs/decisions.md) | Log of design and implementation decisions |
| [`docs/progress.md`](docs/progress.md) | Development progress log |
| [`docs/README.md`](docs/README.md) | Index of all docs, including the how-to notes |

This project is developed with [Claude Code](https://claude.com/claude-code); see [`CLAUDE.md`](CLAUDE.md) for the
working rules it follows.

## Acknowledgements

- [Qt](https://www.qt.io/) — the app's interface (LGPL-3.0).
- [CARTO](https://carto.com/attributions) and [OpenStreetMap](https://www.openstreetmap.org/copyright) contributors —
  map tiles and data.
- [SQLite](https://sqlite.org/) — the airline database (public domain).
- [Little Navmap](https://github.com/albar965/littlenavmap) by Alexander Barthel — a reference for SimConnect handling
  and flight detection edge cases.
- [OurAirports](https://ourairports.com/data/) — public airport and runway data (public domain).
- [mwgg/Airports](https://github.com/mwgg/Airports) — airport time zones (MIT License, Copyright (c) 2014 mwgg).
- [SimBrief](https://www.simbrief.com/) — flight planning and airframe data.

Not affiliated with Microsoft, Asobo, PMDG, Navigraph/SimBrief, FlyHub, CARTO, or any airline.

## License

GPL-3.0 — see [`LICENSE`](LICENSE).
