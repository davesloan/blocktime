# Airport data

Where the airport data comes from, its terms, and what the import keeps (D-079, D-084, D-094). How to run it is in
the root README ("Airport data").

## Sources and terms

| File | Source | Terms |
|---|---|---|
| `airports.csv`, `runways.csv` | OurAirports, via its GitHub mirror `https://davidmegginson.github.io/ourairports-data/` (updated daily) | Public domain: the mirror's LICENSE is the Unlicense ("free and unencumbered software released into the public domain") |
| `airports.json` | mwgg/Airports, `https://raw.githubusercontent.com/mwgg/Airports/master/airports.json` (~29k airports keyed by ICAO) | MIT License, "Copyright (c) 2014 mwgg": use and redistribution allowed with the notice kept. Credited in the README's Acknowledgements |

Checked 2026-11-22. Notes:
- ourairports.com itself had an expired certificate, so its data dictionary couldn't be read. The mirror's files
  are the same data.
- mwgg's README says its time zones were "initially sourced from TimeZoneDB … and updated using TimeAPI". It says
  nothing about separate terms for those, and the repository as a whole is MIT.

## Columns

- `runways.csv` header (checked 2026-11-22): `id, airport_ref, airport_ident, length_ft, width_ft, surface, lighted,
  closed, le_ident, …, he_ident, …` (20 columns). The import uses `airport_ref`, `length_ft`, `surface`, `closed`.
- `airports.csv`: the import needs `id, type, name, latitude_deg, longitude_deg, elevation_ft, iso_country,
  gps_code`, and uses `icao_code` too if the file has it. The header couldn't be fetched in advance (the file is
  over 10 MB); recorded at the first real import.
- `airports.json`: an object keyed by ICAO code; each value has `icao`, `iata`, `name`, `city`, `state`, `country`,
  `elevation`, `lat`, `lon`, `tz`.

## The filter

Kept: `large_airport` or `medium_airport`, a 4-character ICAO code, an open paved runway of at least 4,000 ft (a
surface containing ASP, CON, PEM, BIT, TAR or PAV, after upper-casing), latitude, longitude, elevation and a
2-letter country, and an IANA zone the C++ library (Windows' time zone data) knows.

Results of the first real import: to be recorded (count kept, each reason's count, database size).
