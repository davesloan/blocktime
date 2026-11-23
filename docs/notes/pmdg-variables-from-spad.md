# PMDG variables from Dave's SPAD.neXt profiles

Source: Dave's SPAD.neXt profiles `PMDG_737.xml` and `PMDG_777.xml` (2026-11-22). The profiles themselves aren't
committed (roadmap, milestone 6 notes). This note lists the PMDG and sim variables those profiles **read** and the
PMDG events they **send**, so the milestone 6 probe flight can target the right things.

**Everything here comes from a configuration file, not from the sim.** The meaning of each data value is inferred
from the light or display it drives. Confirm each one in the probe flight before relying on it.

## How SPAD names things

- `PMDGNG3:0000:XXXX` is a value from the PMDG 737 SDK data broadcast; `PMDG777:2000:XXXX` from the 777's. SPAD
  shows its own address, not the SDK header's field name. To read these from BlockTime, match each one to a field
  in the SDK header that ships with each aircraft (`PMDG_NG3_SDK.h`, `PMDG_777X_SDK.h`). **To do in the probe:**
  find where the headers are on Dave's install and record the mapping here.
- Since SPAD lights Dave's panels from these values, the PMDG data broadcast is already switched on in his
  installs, at least for the 737. The 777 profile reads only a handful of values; the probe shows whether its
  broadcast works too.
- `PMDG737:EVT_...` / `PMDG777:EVT_...` / `PMDGNG3:EVT_...` are PMDG control events (writes). BlockTime only reads,
  but the event names show **which switches Dave actually uses**, which is the probe's target list.
- `SIMCONNECT:` and `MSFS:` are plain SimVars; `LVAR:` are local variables.

## 737 (PMDGNG3): values read

From the Honeycomb Bravo annunciator lights, gear lights and autopilot buttons, and the Saitek Multi and Radio Panels.
The Bravo light names are the Bravo's printed labels.

| Address | Drives | Probably |
|---|---|---|
| `0000:018F` | Bravo PARKING BRAKE light | **Parking brake annunciator.** Compare with the `BRAKE PARKING POSITION` SimVar the recorder uses for block out |
| `0000:015B` / `015C` / `015D` | Bravo gear green (nose / left / right) | Gear down-and-locked lights |
| `0000:0158` / `0159` / `015A` | Bravo gear red (nose / left / right) | Gear in-transit lights |
| `0000:021C`–`0227` (12 values) | Bravo DOOR light (any on) | Door annunciators (entry, cargo, service, ...) |
| `0000:00F4` | Bravo APU light | An APU annunciator (running or maintenance?) |
| `0000:01BD` / `01BE` | Bravo STARTER ENGAGED (either) | Engine 1/2 start valve open |
| `0000:00F6` | Bravo ANTI ICE light | An anti-ice annunciator |
| `0000:0051`–`0056` (6 values) | Bravo LOW FUEL PRESSURE (any) | Fuel pump LOW PRESSURE lights |
| `0000:0079` | Bravo LOW OIL PRESSURE | Low oil pressure light |
| `0000:00A9`–`00AC` (4 values) | Bravo LOW HYD PRESSURE (any) | Hydraulic pump LOW PRESSURE lights |
| `0000:00EC` | Bravo ENGINE FIRE | Engine/fire warning light |
| `0000:00EE` | Bravo MASTER CAUTION | Master caution |
| `0000:0057` | Bravo LOW VOLTS | An electrical annunciator |
| `0000:0064` | Radio Panel power (2 = on, 1 = off) | Some electrical power state |
| `0000:0125` / `0126` / `0127` / `0128` | Multi Panel REV / IAS / HDG / NAV lights | MCP VNAV / LVL CHG / HDG SEL / LNAV lights |
| `0000:012A` / `012B` / `012C` / `012D` | Multi Panel APR / ALT / VS / AP lights | MCP APP / ALT HOLD / V/S / CMD A lights |
| `0000:0108` / `010A` | Course displays | MCP course L / R |
| `0000:010C`, `0000:0110` | IAS display, and its blank flag (1 = blank) | MCP speed window |
| `0000:0118` | VS display; ±16960 means blank | MCP V/S window |
| `LVAR:ngx_ALTwindow`, `LVAR:ngx_HDGwindow` | ALT / HDG displays | MCP altitude and heading windows |

Also read in the 737 profile: `SIMCONNECT:AVIONICS MASTER SWITCH` (panel power), `SIMCONNECT:CIRCUIT GENERAL PANEL ON`
with `MSFS:MASTER WARNING ACTIVE` / `ACKNOWLEDGED` (Bravo MASTER WARNING), `SIMCONNECT:SIM ON GROUND` (reversers only
on the ground), `SIMCONNECT:PLANE HEADING DEGREES MAGNETIC`, `MSFS:KOHLSMAN SETTING HG:1`, and the radio
frequencies, DME and transponder SimVars.

## 777 (PMDG777): values read

The 777 profile has no Bravo light bindings, so it reads far less.

| Address | Drives | Probably |
|---|---|---|
| `2000:0150` / `0153` / `0156` | Multi Panel AP / NAV / HDG lights | MCP A/P (L), LNAV, HDG HOLD lights |
| `2000:0138`, `2000:0124` | IAS display, and its blank flag (1 = blank) | MCP IAS/MACH window |
| `2000:012A` | VS display; ±16960 means blank | MCP V/S window |
| `2000:0195`, `2000:01AA` | Radio Panel XPDR left display | Probably the baro settings |
| `2000:01D8` | Radio Panel ADF display (÷1000) | ADF frequency |

Not usable: the profile's ALT, APR, REV (VNAV), IAS (FLCH) and VS button lights, its CRS display and the ALT/HDG
displays still read **737** values (`PMDGNG3:...`, `ngx_...`). They're leftovers and say nothing about the 777.

## Switches Dave uses (from the events sent)

These are the switches whose **state** the probe should try to read, per family.

| Switch | 737 event | 777 event |
|---|---|---|
| Beacon | `EVT_OH_LIGHTS_ANT_COL` | `EVT_OH_LIGHTS_BEACON` |
| Strobe / position | `EVT_OH_LIGHTS_POS_STROBE` (3 positions: 0, 1, 2) | `EVT_OH_LIGHTS_STROBE`; nav is `EVT_OH_LIGHTS_NAV` |
| Landing | `EVT_OH_LIGHTS_L_FIXED`, `R_FIXED` (retractables untouched) | `EVT_OH_LIGHTS_LANDING_L`, `_NOSE`, `_R` |
| Runway turnoff | `EVT_OH_LIGHTS_L_TURNOFF`, `R_TURNOFF` | same names |
| Taxi | `EVT_OH_LIGHTS_TAXI` | `EVT_OH_LIGHTS_TAXI` |
| Logo, wing | `EVT_OH_LIGHTS_LOGO`, `EVT_OH_LIGHTS_WING` | same names |
| Wheel well | `EVT_OH_LIGHTS_WHEEL_WELL` | not bound |
| Seatbelt sign | `EVT_OH_FASTEN_BELTS_LIGHT_SWITCH` (OFF / AUTO / ON) | **not bound** |
| APU | `EVT_OH_LIGHTS_APU_START` (OFF / ON / START) | **not bound** |
| Battery | `EVT_OH_ELEC_BATTERY_SWITCH`, `EVT_OH_ELEC_BATTERY_GUARD` | not bound |
| Anti-ice | `EVT_OH_ICE_ENGINE_ANTIICE_1/2`, `EVT_OH_ICE_WING_ANTIICE` | not bound |
| Window and probe heat | `EVT_OH_ICE_WINDOW_HEAT_1..4`, `EVT_OH_ICE_PROBE_HEAT_1/2` | not bound |
| Trim air | `EVT_OH_AIRCOND_TRIM_AIR_SWITCH_800` | not bound |
| Gear lever | `EVT_GEAR_LEVER` (and sim `GEAR_UP` / `GEAR_DOWN`) | not bound (sim gear only) |
| Reversers | `EVT_CONTROL_STAND_REV_THRUST1/2_LEVER` | same names |
| TO/GA | `EVT_CONTROL_STAND_TOGA1/2_SWITCH` | same names |
| Autopilot / autothrottle | `EVT_MCP_CMD_A_SWITCH`, `EVT_MCP_AT_ARM_SWITCH`, `EVT_MCP_DISENGAGE_BAR` | `EVT_MCP_AP_L/R_SWITCH`, `EVT_MCP_AT_ARM_SWITCH_L/R`, `EVT_MCP_DISENGAGE_BAR` |
| Transponder / TCAS mode | not bound | `EVT_TCAS_MODE` |

Flaps, spoilers, throttles and parking brake come from the Bravo axes or the sim, not from PMDG events.

## What the probe flight should log

**Both families**, as SDK data fields (names from the SDK headers) and, side by side, any standard SimVar that
claims the same thing, so we learn which SimVars can be trusted on PMDG:

1. **Switch positions** from the table above: beacon, strobe/nav, landing, taxi, turnoff, logo, wing, seatbelt,
   APU switch, battery, anti-ice, window/probe heat. Log every change with its time.
2. **Parking brake:** the SDK value (737 `0000:018F`) next to `BRAKE PARKING POSITION`, especially on the 777,
   whose brake already behaved oddly (`msfs-observed-behaviour.md`).
3. **Gear:** SDK green/red lights next to `GEAR HANDLE POSITION` and `GEAR TOTAL PCT EXTENDED`.
4. **Doors:** the 12 door values on the 737 (and the 777 equivalents) through a GSX boarding and deboarding, to
   see whether they line up with GSX's door events.
5. **APU:** the APU switch and the APU annunciator (737 `0000:00F4`) from start to shutdown.
6. **Autopilot:** CMD A / A/P L engaged, A/T armed, and the LNAV/VNAV lights, for the 1,000 ft "configured and
   stable" check of milestone 10.

**777 only:** the profile reads none of the overhead, so every 777 switch above has to be found in the SDK header
from scratch. Seatbelt and APU aren't even bound to an event; they're still worth logging.
