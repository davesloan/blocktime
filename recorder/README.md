# BlockTime Recorder

A small Windows console program that connects to Microsoft Flight Simulator 2020 through SimConnect and writes what
it observes to a local JSON Lines event log. The spec is in [`docs/recorder.md`](../docs/recorder.md).

**Current status: milestone 2 ("Block out / block in"), version 0.2.0.** It connects, reconnects, logs sim state
and telemetry, and detects when a flight starts and ends from the parking brake (D-031, D-032). It doesn't detect
phases yet, and an open flight isn't kept across a recorder or sim restart (milestone 3).

## Prerequisites

- Windows 10/11 x64.
- **Visual Studio 2022 or newer** (developed on VS 18 / 2026 Community) with the **Desktop development with C++**
  workload. That workload includes MSVC, CMake, Ninja and vcpkg; nothing else needs installing.
- **MSFS 2020 SDK**, which includes the SimConnect SDK. Its default install location is `C:\MSFS SDK\`. The build looks
  for SimConnect in this order:
  1. `-DSIMCONNECT_SDK_DIR=<path to the "SimConnect SDK" folder>`, if you pass it
  2. `%MSFS_SDK%\SimConnect SDK` (the SDK installer sets `MSFS_SDK`)
  3. `C:\MSFS SDK\SimConnect SDK`
- An internet connection on the first build, because vcpkg downloads `nlohmann-json` and `gtest`.

## Build

Open a **Developer PowerShell for VS** (Start menu → "Developer PowerShell for VS"), then:

```powershell
cd <repo>\recorder
```

```powershell
cmake --preset x64-release
```

```powershell
cmake --build --preset x64-release
```

The exe is written to `recorder\build\x64-release\blocktime-recorder.exe`. Use `x64-debug` in place of
`x64-release` for a debug build.

To build from a normal PowerShell instead, load the dev environment first. Adjust the path if you use a different
Visual Studio edition or version:

```powershell
$vs = "C:\Program Files\Microsoft Visual Studio\18\Community"; $env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"; Import-Module "$vs\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"; Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64"
```

## Test

The unit tests cover the logic that can run without the sim: block detection, timestamps and distances. After a
build:

```powershell
ctest --preset x64-release
```

The test exe is `build\x64-release\blocktime-tests.exe`. The tests also show up in Visual Studio's Test Explorer.
Pass `-DBLOCKTIME_BUILD_TESTS=OFF` to the configure step to skip building them.

Notes:
- The presets need `VCPKG_ROOT`. The developer shell sets it to Visual Studio's bundled vcpkg.
- `vcpkg.json` pins a `builtin-baseline`. The vcpkg bundled with VS won't install anything without one.

## Run

From the `recorder` folder:

```powershell
.\build\x64-release\blocktime-recorder.exe
```

From anywhere else, use the full path, e.g.:

```powershell
& "$HOME\Documents\Projects\blocktime\recorder\build\x64-release\blocktime-recorder.exe"
```

- You can start it before or after MSFS. While MSFS is still starting up, it prints
  `Sim found, waiting for it to finish starting...` and then `Connected to sim.` once MSFS answers. That can take
  several minutes. While MSFS isn't running it prints `Waiting for sim...` and retries
  every 10 s. It slows to every 20 s after 30 min and every 40 s after 60 min.
- Press **Ctrl+C** to stop it cleanly.
- Only one recorder can run at a time. A second copy prints `BlockTime Recorder is already running` and exits
  with code 3.
- `--log-dir <folder>` writes the log somewhere else.

### Output

The log goes to `%LOCALAPPDATA%\BlockTime\events\events-YYYY-MM-DD.jsonl`, one file per UTC day. Each line is one
JSON object:

```json
{"ts":"2026-11-21T14:03:07.123Z","type":"telemetry","data":{"parking_brake":true,"on_ground":true,"fuel_lb":10512.0,...}}
```

Event types logged so far:

| Type | When |
|---|---|
| `recorder_started` / `recorder_stopped` | The program starts / exits cleanly |
| `sim_connected` | MSFS answered the connection; includes MSFS and SimConnect versions, and `process_watch` (whether `FlightSimulator.exe` was found) |
| `sim_disconnected` | The session ended; `reason` is `sim_quit`, `sim_process_gone`, `too_many_errors` or `recorder_exit`. Only logged after a `sim_connected` |
| `sim_unresponsive` / `sim_responsive` | No messages for 30 s or more (common on loading screens) / messages are back; `silent_s` says how long. The session is kept |
| `sim_running` / `sim_stopped` | The sim enters / leaves a flight (MSFS "Sim" state). Flips several times while loading |
| `paused` / `unpaused` | The pause state changes. `flags` is the raw `Pause_EX1` value. `kinds` names its bits: `full` (main menu, loading screens), `active` (active pause), `sim` (Escape menu), `legacy` (unused), or `bit_<n>` for an unknown bit |
| `aircraft_loaded` | Path of the loaded `aircraft.cfg`; logged only when the aircraft changes |
| `telemetry` | A valid sample: on change, at most once a second; identical repeats are skipped. Includes `engines`, `eng_running` (per engine) and `fuel_flow_pph` (total) |
| `telemetry_invalid` | Samples start being rejected as junk; `reason` says why |
| `block_out` | The flight started: the parking brake release that was followed by 10 m of movement. `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` are from the release; also `flight_id` (= that time), `moved_m`, `atc_id`, `title` |
| `block_in` | The flight ended: after landing, a brake set held until every engine was off. `time`, `lat`, `lon`, `fuel_lb`, `fuel_gal` are from the set; also `flight_id`, `shutdown_time`, `shutdown_fuel_lb`, `block_time_s` |
| `flight_interrupted` | The sim session ended, or the recorder exited, with a flight open; `flight_id`, `reason`. The flight is dropped (no resume until milestone 3) |
| `simconnect_exception` | SimConnect reported an exception; `call` names the request that caused it |
| `warning` | Something unexpected but not fatal (e.g. `FlightSimulator.exe` not found) |

A copy of each event is also printed to the console in a short form.
