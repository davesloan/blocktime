# Test data

## `flight-klas-ksan/`

The debrief's real-flight fixture (milestone 10): Dave's BBJ2 leg KLAS → KSAN of 2026-11-24 (log flight id
`2026-11-24T23:53:16.837Z`), from 30 minutes before block out to block in. Made once with
`tools/make_debrief_fixture.py` from the recorder 0.10.0 log (run with `--probe`), in 0.11.0's format.

Added or changed by the script, not recorded as such:

- **`cockpit` events** derived from the probe's PMDG 737 lines (seat belts, landing lights, gear lights, VREF,
  landing flaps, CMD A/B), on change.
- **The touchdown's** `heading_true_deg` (the telemetry's at that moment) and `wind_dir_deg` 330 / `wind_kt` 9 (KSAN
  ATIS Quebec, which is magnetic; the recorder logs the sim's true wind). No `sim_pitch_deg` or `bank_deg`: they
  weren't logged.
- **A `bounces` event** with 0, 15 s after the touchdown: the telemetry shows no bounce.
- **Dropped:** probe lines, cabin and PA messages (`INTERCOM` channels), audio links, and telemetry fields the
  debrief doesn't read.

The other files are described in the tests that read them.
