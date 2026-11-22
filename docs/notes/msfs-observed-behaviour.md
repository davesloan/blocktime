# MSFS 2020 behaviour observed in our own tests

These facts come from BlockTime Recorder logs. Each entry says when and with what it was seen. Add new
observations at the top.

## 2026-11-22: PMDG 737-800 and 777-300ER at KOAK, recorder 0.2.0

### Block detection
- **737 push with GSX:** the push release was confirmed by 10 m of movement 11 s after the release. On the 777 it
  took 15 s.
- **GSX refuelling** raised the 737's fuel from 8,934 lb (at the first brake set) to 15,762 lb before the push.
  This is why block-out fuel is read at the release, not at load or at the first set.
- **Shutdown is quick:** on the 737 the last engine read `false` 2 s after the gate brake set.
- **Brake set within 1 s of a release** (after shutdown at the gate) shows up as two separate samples. A
  release/set pair faster than the 1 s sample period could still be missed.

### 777-300ER engines
- `GENERAL ENG COMBUSTION:n` works on the PMDG 777. Engine 2 started first. On shutdown engine 2 stopped first,
  then engine 1.

### APU fuel burn isn't in fuel flow
- `fuel_flow_pph` is the sum of `ENG FUEL FLOW PPH:n`, so it covers the engines only. On the 777 with the engines
  off and the APU running, fuel flow decayed to about 0 while `FUEL TOTAL QUANTITY WEIGHT` kept dropping about
  4 lb/min (about 230 lb/h). Dave saw the same on the 737.
- **For fuel burn, use the change in tank totals, never fuel flow added up over time.** Block fuel (out − in)
  already includes APU burn, as it does in real operations.

### Force-terminating MSFS
- Killing MSFS (Dave force-terminated it during a flight) logs `sim_stopped`, then the process is gone about 5 s
  later with no SimConnect QUIT. The recorder logs `sim_process_gone`, as designed. A normal quit from the main
  menu sends QUIT (`sim_quit`, seen in the 0.1.1 test).
- Because `sim_stopped` comes first, from 0.3.0 (D-047) an open flight is interrupted with reason `sim_stopped` when
  MSFS is killed, before the session is lost.

## 2026-11-22: PMDG 737-800 and 777-300ER at KSAN, recorder 0.1.1

### Pause_EX1 flags
These values match the list in the SimConnect SDK docs.
- `1`: main menu and loading screens.
- `8`: Escape menu. It also shows for a few seconds right after Ready to Fly is clicked.
- `4`: active pause.
- The flags can change without an unpause in between, e.g. `8` → `1` when exiting to the main menu from the
  Escape menu.

### Main menu
- Long stays at the main menu (22 and 32 minutes) produced no disconnects and no heartbeat warnings. Only
  loading screens go silent (30–55 s).

### PMDG fuel restore is tied to Ready to Fly, not to time since load
- **737:** fuel went 23,031 → 9,001 lb about 5 s after Ready to Fly (49 s after load). This confirms PMDG restores
  the last FMC fuel (9,000 lb) the next time it loads.
- **777-300ER:** the default 179,275 lb became the saved 34,552 lb about 10 s after Ready to Fly, on the load where
  PMDG came up properly.
- **When PMDG fails to load** (no tablet, the aircraft can't be used), fuel stays at the default value. A fuel
  reading taken before PMDG finishes loading is meaningless.

### Engines (`GENERAL ENG COMBUSTION:n`)
- **737:** works on PMDG. Engine 2 started first (`[false,true]`, then `[true,true]`), and shutdown went back to
  `[false,false]`.
  - `fuel_flow_pph` is the sum for all engines, about 952 pph per engine at idle.
  - After combustion stops, flow decays to 0 over about 50 s. Use `eng_running` for "engine off", not fuel flow.
- **777:** both engines started (about 3,200 pph each at idle). Shutdown wasn't tested (exited with engines
  running).

### PMDG 777-300ER parking brake
- `BRAKE PARKING POSITION` was false at load and showed the single set Dave made. A release on the 777 hasn't
  been seen yet.

### Escape → Reload
- Reloading the flight from the Escape menu shows up as `sim_stopped` → `sim_running` about 2 s apart, with no
  trip to the main menu and no 0/0 position. Whether it fires `AircraftLoaded` or `FlightLoaded` is still to be
  checked.

### Ground AGL by aircraft
- 737: about 8.1 ft once flying (15.5 ft before Ready to Fly). 777-300ER: about 13.8 ft.

## 2026-11-22: PMDG 737-800 at KSAN, recorder 0.1.0

### Connecting and loading
- **The connection is accepted long before MSFS answers.** While MSFS is starting, `SimConnect_Open` succeeds, but
  the OPEN reply took about 4 minutes to arrive. The recorder must not treat that gap as a failure (D-028).
- **Loading screens go quiet.** During loading, and after returning to the menu, a `RequestSystemState("Sim")`
  heartbeat got no reply for 30 s or more while MSFS was alive.
- **The `Sim` state is not a reliable "in a flight" signal.** It flips running/stopped several times while a flight
  loads and when going back to the menu.
- **The main menu can report `Sim` = running.** The user aircraft there sits at 0/0. The junk filter catches this
  (`position_0_0`).
- **The Ready-to-Fly screen is running + paused.** Telemetry there is valid.

### Pause
- The `Pause` event fires for both the Escape menu and active pause, and can't tell them apart. We switched to
  `Pause_EX1` to get flags (D-030). The values were confirmed in the 0.1.1 test (see above) and are named in the
  log as of 0.1.2 (D-035).

### PMDG 737-800
- **Parking brake:** `BRAKE PARKING POSITION` reads correctly and follows each toggle within about 1 s.
- **Fuel reads correctly:** `FUEL TOTAL QUANTITY WEIGHT` matched an FMC load of 9,000 lb (8,999.99). The ratio to
  gallons was 6.70 lb/gal.
- **PMDG restores its saved fuel by itself, about 60 s after the flight loads.** Fuel went 23,031 → 8,390 lb with no
  input from the pilot. **Never take fuel at aircraft load.** Block-out fuel is read at the confirmed brake
  release (D-031).
- **Still to confirm:** does PMDG restore the last value set in the FMC (9,000 lb) the next time it loads?
  **Yes** (confirmed with 0.1.1, see above; the restore follows Ready to Fly, not a fixed 60 s).
- **The startup profile may set or leave the parking brake.** It isn't consistent (Dave). This is why block out
  needs the movement check of D-031.

### Values that look odd but are correct
- **`PLANE ALT ABOVE GROUND` is about 8 ft on the ground for the 737.** It is measured from the aircraft's reference
  point, not the wheels. Don't use small AGL thresholds (e.g. Little Navmap's 10 ft) to decide "airborne". Use
  `SIM ON GROUND`.
- **`INDICATED ALTITUDE` follows the altimeter setting.** It drifted from +20 to −89 ft while parked as the
  altimeter settled. Use `PLANE ALTITUDE` for anything logical.
- **Parked aircraft settle slowly.** `VERTICAL SPEED` decays through tiny values (e.g. −4e-15 fpm) after load. The
  CHANGED threshold (10 fpm) hides that.
