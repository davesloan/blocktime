# MSFS sends junk data while loading

Little Navmap's comments say "MSFS returns garbled values at startup" and mention a junk user aircraft "appearing
before opening menu". Its trail code also says MSFS can move the aircraft to 0/0 for a moment. The recorder must
never act on that data. In later milestones a junk sample could fire a false block out.

## Filter (recorder/src/telemetry_filter.cpp, D-025)
A sample is rejected unless all of these hold:
1. The sim is in the **running** state (`"Sim"` system state/event = 1).
2. `TITLE` is null-terminated inside its 256-byte buffer, has no control characters, and isn't empty or blank.
3. `PLANE ALTITUDE` and `INDICATED ALTITUDE` are finite and between −2,000 and 200,000 ft.
4. The position isn't 0/0 (both within ±0.1°) while ground speed is under 5 kt.

When samples start failing, one `telemetry_invalid` line with the reason is logged. The next valid `telemetry`
line says how many samples were skipped (`skipped_invalid`).

## Status
- The thresholds come from Little Navmap and haven't been checked against our own logs yet. Check them against
  what Dave's logs show at the main menu and while loading a flight.
