# Test data

## `flight-klas-ksan/`

The debrief's real-flight fixture (milestone 10): Dave's BBJ2 leg KLAS → KSAN of 2026-11-24 (log flight id
`2026-11-24T23:53:16.837Z`), from 30 minutes before block out to block in. Made once with
`tools/make_debrief_fixture.py` from the recorder 0.10.0 log (run with `--probe`), in 0.11.0's format.

Added or changed by the script, not recorded as such:

- **`cockpit` events** derived from the probe's PMDG 737 lines (seat belts, landing lights, gear lights, VREF,
  landing flaps, CMD A/B, and the APU: its selector, running from 300 °C EGT), on change.
- **The touchdown's** `heading_true_deg` (the telemetry's at that moment) and `wind_dir_deg` 330 / `wind_kt` 9 (KSAN
  ATIS Quebec, which is magnetic; the recorder logs the sim's true wind). No `sim_pitch_deg` or `bank_deg`: they
  weren't logged.
- **A `bounces` event** with 0, 15 s after the touchdown: the telemetry shows no bounce.
- **Dropped:** probe lines, cabin and PA messages (`INTERCOM` channels), audio links, and telemetry fields the
  debrief doesn't read.

## `flight-ksan-ksjc/`

The replay's fixture (milestone 11, D-175): Dave's BBJ2 leg KSAN → KSJC of 2026-11-25 (log flight id
`2026-11-25T18:43:04.053Z`), from 30 minutes before block out to block in, in 0.11.1's full format: every field of
every line kept, including position, fuel, the title, `cockpit` and `atc_exchange`. It has a short Escape-menu pause
in flight. Made with `tools/make_replay_fixture.py`, which drops:

- probe and `pmdg_data` lines;
- cabin and PA messages (`INTERCOM` channels, which name the captain) and the ATC entries' audio links (`*_url`);
- four telemetry lines in five, except within 2 minutes of block out, the touchdown and block in (1,448 of 4,876
  lines kept, about 900 KB).

The other files are described in the tests that read them.
