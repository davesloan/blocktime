# Little Navmap as a reference

Little Navmap (GPL-3.0-or-later, by Alexander Barthel) reads MSFS through SimConnect. We studied its code
on 2026-11-21 from the `master` branches. **No code was copied** (D-026). If any is copied in future, keep its
license header and the line `Copyright 2015-2026 Alexander Barthel` with it, and log it in `decisions.md`.

## Where to look
- atools: `src/fs/sc/simconnecthandler.cpp` (connect, dispatch, data definitions, validity checks),
  `src/fs/sc/datareaderthread.cpp` (reconnect loop), `src/fs/sc/simconnectaircraft.h/.cpp`.
- littlenavmap: `src/mapgui/mapwidget.cpp` → `simDataCalcTakeoffLanding` (takeoff/landing detection),
  `src/logbook/logdatacontroller.cpp`, `src/geo/aircrafttrail.cpp`.

## What they do
- **Open:** `Open(name, nullptr, 0, nullptr, 0)`: no window, no event handle, config index 0.
- **Reconnect:** tries to open again every 10 s (atools default) or 15 s (Little Navmap's setting). It slows down
  after 30 and 60 min "to avoid loss of internal SimConnect handles" (their issue #891). We copied the idea (D-024).
- **System events:** `"Sim"` (1 = running) and `"Pause"`, plus `RequestSystemState("AircraftLoaded")`.
- **Exceptions:** `WEATHER_UNABLE_TO_GET_OBSERVATION` is ignored. After 10 errors Little Navmap stops and needs a
  restart; we reconnect instead.
- **Data:** one-shot `RequestDataOnSimObjectType(..., SIMCONNECT_SIMOBJECT_TYPE_USER)` every ~500 ms from its own
  thread, read through `CallDispatch`. We use one periodic CHANGED request on a single thread (D-023).
- **SimVars they use:** `Plane Latitude/Longitude` (degrees), `Plane Altitude`, `Plane Alt Above Ground`,
  `Indicated Altitude` (feet), `Vertical Speed` (*feet per second* ×60), `Ground Velocity`, `Airspeed Indicated`
  (knots), `Sim On Ground` (bool), `Fuel Total Quantity` (gallons) **and** `Fuel Total Quantity Weight` (pounds),
  `Eng Fuel Flow PPH:n` / `GPH:n`, `Title` and `ATC Id` (string256).
- **They don't read** `BRAKE PARKING POSITION` or touchdown rate. We have to verify those ourselves.

## Thresholds to remember for later milestones
- Flying = `!SIM ON GROUND && GS > 10 kt && AGL > 10 ft` (helicopters: `!onGround && AGL > 50`).
  **Careful:** our PMDG 737 reads about 8 ft AGL while parked, because AGL is measured from the reference point.
  10 ft is too close to that. Rely on `SIM ON GROUND` (see [msfs-observed-behaviour.md](msfs-observed-behaviour.md)).
- Debounce: takeoff confirmed 2 s after the change, landing confirmed 4 s after.
- Engine running: fuel flow `> 0.5 GPH` or `> 1.0 PPH`, debounced 1 s.
- For the junk-data filter, see [msfs-junk-data.md](msfs-junk-data.md).
