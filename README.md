# BlockTime

A personal virtual airline for **Microsoft Flight Simulator 2020** — a lightweight alternative to tools like FlyHub,
built around the parts that make flying feel meaningful and none of the busywork.

You lease aircraft, build rotations, and fly legs yourself while the rest of your fleet keeps flying in the background.
Every flight you fly is recorded automatically, from parking brake release to parking brake set.

> **Status:** early development. The flight recorder is being built first; the web app comes after.
> Nothing here is usable yet.

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

| Component | What it does | Tech |
|---|---|---|
| **Recorder** (`/recorder`) | Native Windows app that connects to MSFS via SimConnect, detects flight events and phases, and writes them to a local log | C++, CMake, MSFS SimConnect SDK |
| **Web app** (`/app`) | Runs the airline: fleet, rotations, background flights, money, dashboards. Runs locally — no hosting, no accounts | Node.js, TypeScript, SQLite |

The recorder writes every event to disk first, so no flight is lost if the web app isn't running. The web app catches
up on background flights whenever it's opened.

## Requirements

- Windows 10/11
- Microsoft Flight Simulator 2020
- MSFS 2020 SDK (for the SimConnect headers and library) — enable Developer Mode in the sim and use *Help → SDK
  Installer*
- Visual Studio with the **Desktop development with C++** workload (includes CMake and vcpkg)
- Node.js LTS (for the web app, later)
- A SimBrief account

## Building

Build and run instructions live with each component:

- Recorder: [`recorder/README.md`](recorder/README.md) *(coming soon)*
- Web app: [`app/README.md`](app/README.md) *(later)*

## Documentation

| | |
|---|---|
| [`docs/design.md`](docs/design.md) | Full product design: rules, numbers and rationale |
| [`docs/recorder.md`](docs/recorder.md) | Flight recorder spec |
| [`docs/decisions.md`](docs/decisions.md) | Log of design and implementation decisions |
| [`docs/progress.md`](docs/progress.md) | Development progress log |

This project is developed with [Claude Code](https://claude.com/claude-code); see [`CLAUDE.md`](CLAUDE.md) for the
working rules it follows.

## Acknowledgements

- [Little Navmap](https://github.com/albar965/littlenavmap) by Alexander Barthel — a reference for SimConnect handling
  and flight detection edge cases.
- [OurAirports](https://ourairports.com/data/) — public airport and runway data.
- [SimBrief](https://www.simbrief.com/) — flight planning and airframe data.

Not affiliated with Microsoft, Asobo, PMDG, Navigraph/SimBrief, FlyHub, or any airline.

## License

GPL-3.0 — see [`LICENSE`](LICENSE).
