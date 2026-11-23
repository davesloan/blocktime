# SimConnect setup (MSFS 2020 SDK)

## Where it lives
- The SDK installs to `C:\MSFS SDK\`, and its installer sets the `MSFS_SDK` env var to that path, with a
  trailing backslash.
- The `SimConnect SDK` folder contains:
  - `include\SimConnect.h`
  - `lib\SimConnect.lib` + `lib\SimConnect.dll` (dynamic)
  - `lib\static\SimConnect.lib` and `lib\static\SimConnect_debug.lib`
  - `lib\managed\` (C#, not used)
  - `VS\*.props` (MSBuild property sheets; we read them for the link settings)

## Linking statically
- The static lib also needs `shlwapi.lib`, `user32.lib` and `Ws2_32.lib` (from
  `VS\SimConnectClient-static.props`).
- The static lib is built with **/MD (DLL CRT)**. If you link it into a `/MT` program you get
  `LNK2038 mismatch detected for 'RuntimeLibrary'`. So the recorder uses `/MD` and the vcpkg triplet
  `x64-windows-static-md`. See D-019.

## Data definitions
- SimConnect returns the values of a data definition **back to back with no padding**. The C++ receiving struct
  must be wrapped in `#pragma pack(push, 1)`, and its fields must be in exactly the order they were added with
  `AddToDataDefinition`.
  `src/sim_vars.h` keeps the struct and the list next to each other, with a `static_assert` that checks
  the sizes match.
- Bools are requested as `INT32` with units `"bool"`. Strings use `SIMCONNECT_DATATYPE_STRING256` / `STRING64`
  with units `nullptr`.
- With `SIMCONNECT_DATA_REQUEST_FLAG_CHANGED`, the `fEpsilon` argument of `AddToDataDefinition` sets how much a
  value must change before a new sample is sent. Never put a value that always changes (e.g. `ZULU TIME`) in a
  CHANGED definition, or it will send every period.

## Dispatch
- `SimConnect_GetNextDispatch` returns `E_FAIL` when the queue is **empty**, so that isn't an error.
- `SimConnect_Open` fails quickly when MSFS isn't running, so trying again is cheap.
- **While MSFS is starting**, `SimConnect_Open` *succeeds*, but the `SIMCONNECT_RECV_ID_OPEN` reply can take
  minutes. Treat the session as connected only once OPEN arrives (D-028).
- **During loading screens**, requests such as `RequestSystemState("Sim")` can go unanswered for 30 s or more.
  Silence is not proof of a crash.
- **If MSFS crashes, no `QUIT` arrives.** The recorder keeps a `SYNCHRONIZE` handle to `FlightSimulator.exe`
  (found with a Toolhelp32 snapshot when it connects). It checks that handle with `WaitForSingleObject(h, 0)`
  every loop. *Unverified with a real crash. Also unverified whether the exe name is the same for Steam installs.*

## Matching exceptions to calls
- `SIMCONNECT_RECV_EXCEPTION.dwSendID` identifies the packet that failed, and `dwIndex` the parameter (1-based).
- Right after each successful call, `SimConnect_GetLastSentPacketID` returns that call's send ID. The recorder
  keeps the last 32 (send ID → label) pairs and logs the label as `call`.
- Seen 2026-11-22: `UNRECOGNIZED_ID`, index 3, right after connecting. That is most likely the user-aircraft object
  ID in a snapshot request sent before the aircraft existed. It is now logged as `call: "snapshot"`,
  `ignored: true`.
