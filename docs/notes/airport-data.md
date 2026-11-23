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
  gps_code`, and uses `icao_code` too if the file has it. The real header (checked 2026-11-23, 19 columns): `id,
  ident, type, name, latitude_deg, longitude_deg, elevation_ft, continent, iso_country, iso_region, municipality,
  scheduled_service, icao_code, iata_code, gps_code, local_code, home_link, wikipedia_link, keywords`. So
  `icao_code` is there.
- `airports.json`: an object keyed by ICAO code; each value has `icao`, `iata`, `name`, `city`, `state`, `country`,
  `elevation`, `lat`, `lon`, `tz`.

## The filter

Kept: `large_airport` or `medium_airport`, a 4-character ICAO code, an open paved runway of at least 4,000 ft (a
surface containing ASP, CON, PEM, BIT, TAR or PAV, after upper-casing), latitude, longitude, elevation and a
2-letter country, and an IANA zone the C++ library (Windows' time zone data) knows.

## The first real import (2026-11-23)

Files downloaded that day: `airports.csv` 12.8 MB, `runways.csv` 4.0 MB, `airports.json` 9.0 MB (29,308 airports).
The release build's `blocktime-import` took about a second:

```
Read 86383 airports, kept 4265.
Left out: 81101 not large or medium, 232 without an ICAO code, 621 without an open paved runway of 4000 ft,
151 without a time zone, 13 missing a field.
```

- The new database (airport data only) is 512 KB. 2,626 of the kept airports have a runway of 7,000 ft or more (the
  BBJ2's minimum).
- A second run updated the same 4,265 rows in place and added a second `airport_import` row.
- Spot checks were right: KBFI (America/Los_Angeles, 10,007 ft), KSEA, KJFK, PANC, EGLL, RJTT, YSSY, FAOR, SBGR,
  VHHH, OMDB, NZAA. 367 distinct zones.
- 33 of the large airports were left out. Most aren't in mwgg's file, being new or recoded (YBSU Sunshine Coast,
  HSSK Khartoum, VOVI, FBPM, FVRG and others). The rest have no elevation in OurAirports (ZGOW, ZSTX, WPOC), a blank
  surface (LLER, ZGDY) or "Composite" (VAID). The filter is working as designed. If one of them is ever wanted, a
  zone fallback (for example from OurAirports' country and region) is the fix, not a looser filter.
- mwgg's zones aren't perfect: LLER (Ramon, Israel) is listed as Asia/Amman. It's harmless for neighbours on the same
  offset, but don't treat the dataset as authoritative.
