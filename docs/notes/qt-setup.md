# Qt setup and quirks

Found while building Qt UI test 1 (branch `qt-ui-spike`, 2026-11-22).

## The install

- **Qt 6.12.0**, from the official online installer (free Qt account), at `C:\Qt`. The kit is
  `C:\Qt\6.12.0\msvc2022_64`. The build finds it through `BLOCKTIME_QT_DIR` (D-052).
- Components ticked: MSVC 2022 64-bit, Qt Positioning, Qt Location (for UI test 2's map), plus Qt Graphs, Qt Quick 3D,
  Qt Quick Timeline and Qt Shader Tools. The last four aren't used.
- The msvc2022 kit works with the VS 18 / 2026 compiler: MSVC keeps binary compatibility across versions.
- Qt's DLLs use the DLL C runtime (`/MD`, `/MDd`), which is what the recorder already builds with (D-019).
- Qt Creator, CMake and Ninja were also installed under `C:\Qt\Tools`. The build uses Visual Studio's CMake and
  Ninja instead, through the presets.

## Build quirks

- **Configure warning about `Qt6QmlAssetDownloaderPrivate`** (it needs `Qt6TaskTree`, which isn't installed). It
  comes from an optional Qt plugin and is harmless. It's printed every configure.
- **C4702 "unreachable code" in `qjsengine.h` / `qvariant.h`.** It comes from the C++ that qmlcachegen generates from
  our QML (`build\<preset>\.rcc\qmlcache\*.cpp`). The optimizer raises it, so `/external:W0` can't hide it.
  `CMakeLists.txt` turns it off for those generated files only, by name (`blocktime-app_qml_<Name>_qml.cpp`). If
  Qt changes that naming, the warnings come back rather than being silently hidden.
- `qt_standard_project_setup()` turns AUTOMOC on for every target defined after it, so the Qt section is the last
  one in `CMakeLists.txt`.
- QML files live in `qml/`, but each is given a `QT_RESOURCE_ALIAS`, so it sits at the module root
  (`qrc:/qt/qml/BlockTime/<name>.qml`) next to the module's `qmldir`.
- `windeployqt` runs after each build (D-056) and copies Qt's DLLs and QML plugins next to the exe. With them,
  `build\x64-debug` is about 430 MB (the debug DLLs are large). It also copies some modules we don't use, such as
  Quick 3D. They're only loaded if QML imports them.

## QML gotchas

- **Never name a property `on<Capital>…`** (e.g. `onAccent`). QML treats it as a signal handler, and the error
  ("Cannot assign a value to a signal") only shows at runtime, as "Type Theme unavailable" further up.
  `qmllint` didn't catch it.
- **`state` is taken:** every Item has a `state` property, so a component's own status property needs another
  name (`StatusPill.status`).
- **Inside a Layout**, size items with `Layout.preferredWidth/Height`, not `width/height`. `qmllint` warns about this.
- **Hot reload (D-055):** a QML file loaded from the source folder isn't part of the module any more, so it can't see
  C++ types (`RecorderBridge`) or the `Theme` singleton unless it has `import BlockTime`. Every QML file has it.
- `QQmlEngine::clearSingletons()` is needed on reload, or Theme keeps its old values.
- **In a property's change handler, bindings that depend on that property may not have updated yet.** `RouteMap`'s
  `onTrackChanged` read `trackPoints` (bound to `track.path`) and got the previous flight's points. Read the
  property itself in the handler.
- **`Map.fitViewportToGeoShape` applies after it returns**, so reading `zoomLevel` straight after it gives the old
  value. Clamp the zoom in `Qt.callLater`.
- Items for a `MapView` (`MapPolyline`, `MapQuickItem`) are declared inside it and added with
  `map.addMapItem(...)`; being children of `MapView` alone doesn't put them on the map.
- **Hiding items in a `ColumnLayout`:** if nothing visible has `Layout.fillHeight`, the layout spreads the spare
  height between the items, and a title floats to the middle. Give the remaining item `Layout.fillHeight: true`.

## Map tiles (UI test 2, D-059)

Probed 2026-11-22 with Qt 6.12.0 and CARTO Dark Matter:
- **CARTO's raster URL is** `https://basemaps.cartocdn.com/rastertiles/dark_all/{z}/{x}/{y}.png?key=<key>`. It
  serves 256 px PNGs with `Cache-Control: public, max-age=2592000` (30 days).
- **A missing or wrong key still gets HTTP 200**, with a grey "API KEY REQUIRED" image in place of the map. The
  app can't detect it from the response, so a bad key shows up as those tiles.
- **`osm.mapping.custom.host` accepts a full template with the key after it:**
  `…/dark_all/%z/%x/%y.png?key=<key>`. Qt fills in `%z/%x/%y` wherever they are; it only appends its own
  `%z/%x/%y.png` when there are none. The alternative, a local provider file via
  `osm.mapping.providersrepository.address` with a `file:///` URL, also works but isn't needed.
- **The custom URL map type is the last entry** of `supportedMapTypes` ("Custom URL Map"). Select it after the
  plugin loads.
- `osm.mapping.providersrepository.disabled: true` stops the plugin asking Qt's server for its provider list.
- **The tile cache's file names don't include the URL** (`osm_100-l-8-10-177-412.png`). Tiles from an old or wrong
  key would be reused after the key changes, so the app gives each URL its own cache folder (a hash of the URL).
- **Qt's copyright notice** is a light-grey box whose background can't be set from QML. The app hides it
  (`copyrightsVisible: false`) and draws CARTO's credit line itself.
- **windeployqt and position plugins:** with Qt Positioning linked, windeployqt tries to deploy the NMEA position
  plugin, which needs Qt SerialPort (not installed) and warns on every build. The app reads no position source,
  so the build passes `--skip-plugin-types position`.

## Errors from the windowed app

`BlockTime.exe` is a Windows GUI program, so `qWarning` and stderr go nowhere by default. To see QML errors, set
`QT_FORCE_STDERR_LOGGING=1` and redirect stderr, or run it under the Visual Studio debugger. `EventLog` and
`SimConnection` write a few errors straight to `std::cerr` (a failed log write, a failed SimConnect call); the app
sends `std::cerr` to `%LOCALAPPDATA%\BlockTime\errors.log` (D-067), together with any `LOST:` block events. Block events that fail to write still show in the
event feed as `WARNING` lines, through the observer.

## Checking the screens without showing a window (`--grab`)

`BlockTime.exe --grab <file.png> [--page <n>]` renders the app, saves the window to the PNG after 6 s and quits
(`--page`: 0 Live, 1 Flights, 2 Fleet, 3 Money). With the offscreen platform nothing appears on the desktop, which
matters when Dave is using the machine. Point `LOCALAPPDATA` at a scratch copy of the data folder so the real airline
isn't touched:

```powershell
$env:LOCALAPPDATA = "C:\path\to\scratch"; $env:QT_QPA_PLATFORM = "offscreen"; $env:QT_QUICK_BACKEND = "software"; $env:QT_QPA_PLATFORM_PLUGIN_PATH = "C:\Qt\6.12.0\msvc2022_64\plugins\platforms"; $env:QT_QPA_FONTDIR = "C:\Windows\Fonts"; build\x64-release\BlockTime.exe --grab C:\path\to\live.png --page 0
```

- windeployqt deploys only `qwindows.dll`, so the offscreen plugin comes from the Qt kit
  (`QT_QPA_PLATFORM_PLUGIN_PATH`). Without it the app dies before `errors.log` is opened (exit code 0xC0000409).
- The offscreen platform has no fonts of its own: without `QT_QPA_FONTDIR` every glyph is a box.
- The debug build has only `qwindowsd.dll` too: without the plugin path it stops in a "no Qt platform plugin" assert
  dialog (idle, no window title), not with an exit code.
- `--replay … --grab` needs no scratch folder: the replay opens the database read-only and writes no log (D-175).
- Map tiles load as usual (from the scratch folder's `tiles\` and the CARTO key in its `settings.json`).
- An event log written by hand must be compact JSON (`"type":"block_out"`, no spaces): the log readers find event
  lines with a plain substring before parsing them, as `EventLog` writes them.

## Measured (no sim)

Release build, idle, Windows 11: about 87–92 MB (working set and private bytes), 0% CPU over 10 s (UI test 1).

UI test 2, release, `--demo`, 10 s per page:

| Page | Task Manager Memory (private working set) | Working set | Private bytes | CPU |
|---|---|---|---|---|
| Live | 31 MB | 95 MB | 100 MB | 0% |
| Flights, after five flights on the map | 44 MB | 106 MB | 114 MB | 0% |
| Fleet | 46 MB | 110 MB | 117 MB | 0% |

Dave's limit is read in Task Manager (its Memory column is the private working set). Working set and private bytes
also count the shared Qt DLLs and committed but unused memory, so they read about 2.5 times higher.

**Grabs from a script (milestone 12):**
- PowerShell's `&` doesn't wait for a GUI program: `& BlockTime.exe --grab …` returns at once and the window grabs on
  its own 6 s later. Use `Start-Process -FilePath … -ArgumentList … -Wait -NoNewWindow` to wait for the PNG.
- On the laptop the Qt kit is 6.12.1, so the plugin path is `C:\Qt\6.12.1\msvc2022_64\plugins\platforms`.
- `--replay` reads the database without migrating it. On a database older than the build (before Dave first runs the
  new version), the flight's view can't be read; grab against a scratch copy that the app has opened once.
- `QT_FORCE_STDERR_LOGGING=1` sends QML errors to `errors.log`, which shows why a grab produced no window.
