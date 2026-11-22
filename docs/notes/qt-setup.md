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
- QML files live in `recorder/qml/`, but each is given a `QT_RESOURCE_ALIAS`, so it sits at the module root
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

## Errors from the windowed app

`blocktime-app.exe` is a Windows GUI program, so `qWarning` and stderr go nowhere by default. To see QML errors, set
`QT_FORCE_STDERR_LOGGING=1` and redirect stderr, or run it under the Visual Studio debugger. `EventLog` and
`SimConnection` still write a few errors straight to `std::cerr` (a failed log write, a failed SimConnect call). The
app doesn't show those; that's accepted until Dave decides on Qt. Block events that fail to write still show in the
event feed as `WARNING` lines, through the observer.

## Measured (no sim)

Release build, idle, Windows 11: about 87–92 MB (working set and private bytes), 0% CPU over 10 s.
