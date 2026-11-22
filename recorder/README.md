# BlockTime Recorder

A small Windows console program that connects to Microsoft Flight Simulator 2020 through SimConnect and writes what
it observes to a local JSON Lines event log. The spec is in [`docs/recorder.md`](../docs/recorder.md).

**Current status: milestone 3 ("Ending flights cleanly"), version 0.3.0.** It connects, reconnects, logs sim state
and telemetry, and detects when a flight starts and ends from the parking brake (D-031, D-032). Every flight ends
with `block_in` or `flight_interrupted`; a flight is never resumed (D-046). It doesn't detect phases yet.

**Branch `qt-ui-spike`:** this branch also builds **`blocktime-app.exe`**, the Qt UI test app (D-050, D-052). It runs
the same recorder inside a dark Qt Quick window. See [Qt UI test app](#qt-ui-test-app-branch-qt-ui-spike) below.
On this branch Qt is required to build.

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
- **On the `qt-ui-spike` branch: Qt 6.8 or newer** (developed with 6.12.0), from the official Qt online installer
  (free Qt account) with the **MSVC 2022 64-bit** component. The build looks for the kit in this order:
  1. `-DBLOCKTIME_QT_DIR=<kit folder>`, if you pass it
  2. `%BLOCKTIME_QT_DIR%`
  3. `C:\Qt\6.12.0\msvc2022_64`

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
| `flight_interrupted` | A flight ended without block in; `flight_id`, `reason`. The reason is the session's lost reason (`sim_quit`, `sim_process_gone`, ...), `recorder_exit`, `sim_stopped` (the flight was left: main menu, restart, or MSFS killed) or `recorder_crash` (logged on the next start for a flight a killed recorder left open). The flight is never resumed (D-046) |
| `simconnect_exception` | SimConnect reported an exception; `call` names the request that caused it |
| `warning` | Something unexpected but not fatal (e.g. `FlightSimulator.exe` not found) |

A copy of each event is also printed to the console in a short form.

If a block event (`block_out`, `block_in`, `flight_interrupted`) can't be written, the console prints
`WARNING: <type> could not be written to the log; retrying every 5 s.` with the event's data. It is retried until it
succeeds (`Held events written to the log.`); while waiting for the sim, only at each reconnect try. Any still unwritten at exit are printed to stderr as `LOST: <type>
<data>` (D-049).

At startup, if the log shows a flight that a killed recorder left open, the console prints `Previous flight <id> was
left open (the recorder didn't close cleanly); logged as interrupted.` (D-048).

## Qt UI test app (branch qt-ui-spike)

`blocktime-app.exe` is built by the same presets as the recorder, next to it in `build\<preset>\`. A post-build
step copies Qt's DLLs next to it, so it runs from there (D-056).

```powershell
.\build\x64-release\blocktime-app.exe
```

- It records exactly like the console recorder, to the same log (D-053). Use it **instead of** the console recorder,
  not alongside it. Only one can record at a time: if the console recorder is running, the app's pill shows
  "Recorder already running" and it records nothing.
- Closing the window stops recording cleanly, like Ctrl+C. An open flight is logged as interrupted
  (`recorder_exit`), then `recorder_stopped`.
- `--log-dir <folder>` works as for the recorder.
- `--demo` shows made-up values (an open flight at cruise) and records nothing. Use it to look at the UI without the
  sim (D-057).
- Use the **release** build for flights and for memory/CPU readings. The debug build is much heavier.

### Changing the UI

The QML is in `recorder/qml/`, and every colour and size is in `Theme.qml`. Run the **debug** build:

```powershell
.\build\x64-debug\blocktime-app.exe --demo
```

In debug builds the app reads QML straight from `recorder/qml/` and reloads the window about 200 ms after you save
a `.qml` file, with no rebuild (D-055). If the QML has an error, a small error window says so. Fix and save, and
the real window comes back. C++ changes still need a rebuild.

To see QML errors, start it with `QT_FORCE_STDERR_LOGGING` set and stderr captured:

```powershell
$env:QT_FORCE_STDERR_LOGGING = "1"; .\build\x64-debug\blocktime-app.exe --demo 2> qml-errors.txt
```

To lint the QML:

```powershell
cmake --build --preset x64-debug --target blocktime-app_qmllint
```

Quirks found while setting this up are in [`docs/notes/qt-setup.md`](../docs/notes/qt-setup.md).
