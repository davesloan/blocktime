# Milestone 4 (Airline Foundation) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans (native, per CLAUDE.md / D-033; no
> subagent-driven development). Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** BlockTime reads Dave's airline from config files into the real airline database, imports airport data,
charges monthly leases into a money ledger, and shows the real airplanes and the money in the app.

**Architecture:**
- Everything new is plain C++ in `blocktime-core`, unit-tested:
  - a small SQLite wrapper and the migrated schema;
  - money and date helpers;
  - the YAML config readers;
  - the airport import;
  - the leasing processes;
  - an `AirlineSession` that ties them together for the app.
- The app only builds Qt models from the session's results and refreshes them once a minute.
- A separate console tool, `blocktime-import.exe`, imports airport data that Dave downloads.

**Tech Stack:** C++20, MSVC `/W4 /permissive- /utf-8`, CMake presets + Ninja, vcpkg (sqlite3, nlohmann-json, gtest,
**yaml-cpp** new), Qt 6 QML for the screens.

**Spec:**
- `docs/database.md`: the schema and milestone 4's processes. This is the source of truth for tables and rules.
- `docs/roadmap.md`: milestone 4.
- `docs/design.md`: §3 the catalog, §5 leasing, §6 setup, §11 rounding (D-080), §15 reference data.
- Decisions D-076, D-078..D-093.

## Global Constraints

- No warnings at `/W4 /permissive- /utf-8`, release and debug. All tests pass. `blocktime-app_qmllint` reports nothing.
- Every new source and QML file starts with the GPL header used in `src/*.cpp`. QML files also `import BlockTime`
  and use `pragma ComponentBehavior: Bound`.
- `blocktime-core` and its tests include no Qt, `windows.h` or `SimConnect.h`.
- **The database rules:** `CLAUDE.md` ("Database", D-078, D-090), the schema exactly as in `docs/database.md`, and
  `PRAGMA foreign_keys = ON` on every connection.
- **Times and money:** times as `YYYY-MM-DDTHH:MM:SSZ` and dates as `YYYY-MM-DD` (D-081). Money in integer cents,
  every stored amount a multiple of 500 (D-080).
- **Files:** config and data live in `%LOCALAPPDATA%\BlockTime\`: `blocktime.db`, `catalog.yaml`, `airline.yaml`.
  The repo holds `config/catalog.yaml` and `config/airline.example.yaml`. No keys or personal values are committed.
- **Version:** the app becomes **0.5.0** (`CMakeLists.txt` and `vcpkg.json`).
- **Commits:** straight to `main` (D-038). Messages explain why and cite D-numbers, and end with
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`. Don't push.
- Build: `cmake --build --preset x64-release`. Tests: `ctest --preset x64-release`. Test names below are run with
  `build\x64-release\blocktime-tests.exe --gtest_filter=<Suite>.*`.

## Review Focus

1. **A typo in `airline.yaml` or `catalog.yaml`** (bad indentation, an unknown airframe name). Expected: the app
   still opens, shows the airline already in the database, leases nothing new, and the notice names the file and
   line. Tests: Task 4 `Config.YamlErrorNamesTheLine`, Task 6 `Session.BadConfigStillShowsTheDatabase`.
2. **The app left open across a lease due date.** Expected: the minute tick charges the month once, and the screens
   update. Test: Task 6 `Session.TickChargesAMonthThatFallsDueWhileOpen`.
3. **The app closed for several months.** Expected: each month is charged at its own due date, in date order across
   airplanes, and an airplane is returned at the first month cash can't cover. Test: Task 5
   `Leasing.MonthsMissedWhileClosedAreChargedInDateOrder`.
4. **The app killed during settlement, or settlement run twice.** Expected: no half-leased airplane and no month
   charged twice. Tests: Task 5 `Leasing.SettlingTwiceChangesNothing` and Task 1
   `AirlineDb.LeaseMonthCannotBeChargedTwice`.
5. **The OurAirports files change shape** (a renamed or missing column). Expected: the import stops with "missing
   column X" and writes nothing. Test: Task 3 `AirportImport.MissingColumnIsAnError`.

---

### Task 1: SQLite wrapper and the migrated airline database

**Files:**
- Create: `src/sqlite_db.h`, `src/sqlite_db.cpp`, `src/airline_db.h`, `src/airline_db.cpp`
- Create: `tests/airline_db_test.cpp`
- Modify: `CMakeLists.txt` (core sources, test sources)

**Interfaces:**
- Produces (`sqlite_db.h`, namespace `blocktime::db`):
  - `class Database`. Move-only. The destructor calls `sqlite3_close_v2`.
    - `static std::optional<Database> open(const std::filesystem::path& file, std::string* error)`: converts the
      path to UTF-8 and opens read/write, creating the file.
    - `static Database openInMemory()`.
    - `bool exec(std::string_view sql, std::string* error = nullptr)`.
    - `int userVersion()`.
    - `sqlite3* handle()`.
  - `class Statement`:
    - `Statement(Database&, std::string_view sql)`;
    - `bind(int index, std::int64_t)`, `bind(int, std::string_view)`, `bind(int, double)`, `bindNull(int)`, all
      1-based;
    - `bool next()`: true while there's a row;
    - `bool run()`: steps to done, true on success;
    - `std::int64_t integer(int col)`, `double real(int col)`, `std::string text(int col)`, `bool isNull(int col)`;
    - `void reset()`;
    - `const std::string& error()`: the prepare or step error, "" if none.
  - `class Transaction`:
    - `explicit Transaction(Database&)` runs `BEGIN IMMEDIATE`;
    - `bool commit()`;
    - the destructor rolls back if it wasn't committed.
- Produces (`airline_db.h`, namespace `blocktime`):
  - `inline constexpr int kAirlineSchemaVersion = 2;`
  - `bool migrateAirlineDatabase(db::Database&, std::string* error)`:
    - applies every migration above `user_version`, each in its own transaction, setting `user_version` inside it;
    - sets `foreign_keys = ON`, `journal_mode = WAL` and `synchronous = FULL` first;
    - refuses a `user_version` above `kAirlineSchemaVersion` with "The database is from a newer BlockTime (schema
      N)".
  - `struct AirlineDbOpen { std::optional<db::Database> db; std::string error; bool testSchemaSetAside = false; };`
  - `AirlineDbOpen openAirlineDatabase(const std::filesystem::path& file)`. A file with `user_version` 1 is closed
    and renamed to `blocktime.test-schema.db` in the same folder (D-086), then a new file is built.
  - `std::filesystem::path defaultAirlineDatabaseFile()` returns `%LOCALAPPDATA%\BlockTime\blocktime.db`.

- [ ] **Step 1: Write the failing tests** in `tests/airline_db_test.cpp`:
  - `AirlineDb.EmptyDatabaseMigratesToVersion2`: in memory, `migrateAirlineDatabase` returns true and
    `userVersion() == 2`.
  - `AirlineDb.ForeignKeysAreOn`: `PRAGMA foreign_keys` returns 1 after migrating.
  - `AirlineDb.MigratingTwiceIsANoOp`: the second call returns true and the version stays 2.
  - `AirlineDb.NewerSchemaIsRefused`: set `user_version = 3`; migrating returns false and the error contains
    "newer BlockTime".
  - `AirlineDb.TestSchemaIsSetAside`:
    - in a temp folder, create `blocktime.db` with `user_version = 1` and a table `airframes`;
    - `openAirlineDatabase` gives a database at version 2 with `testSchemaSetAside == true`;
    - `blocktime.test-schema.db` exists and still has the `airframes` table.
  - `AirlineDb.ChecksRejectBadRows`: each insert fails:
    - a ledger amount of 250;
    - a `posted_at` of `2026-11-22 10:00:00`;
    - an aircraft type with family `747`;
    - an airplane whose `return_reason` is set while `returned_on` is NULL;
    - a second `opening_cash` line.
  - `AirlineDb.LeaseMonthCannotBeChargedTwice`: two `lease_charge` rows with the same airplane and `period_start`.
    The second insert fails.
  - `AirlineDb.QueryPlansUseIndexes`: `EXPLAIN QUERY PLAN` output contains `USING INDEX` or `USING COVERING
    INDEX`, and never `USE TEMP B-TREE`, for:
    - `SELECT airplane_id FROM airplane WHERE location_airport_id = ?`;
    - `SELECT ledger_line_id FROM ledger_line ORDER BY posted_at DESC`;
    - `SELECT SUM(amount_cents) FROM ledger_line WHERE posted_at <= ?`;
    - `SELECT 1 FROM lease_charge WHERE airplane_id = ? AND period_start = ?`.
- [ ] **Step 2: Run them to make sure they fail.** Run `cmake --build --preset x64-release`. Expected: a compile
  error, since `airline_db.h` doesn't exist yet.
- [ ] **Step 3: Implement** `sqlite_db` and `airline_db`. Migration 2 is the full schema from `docs/database.md`,
  written as one SQL string in `airline_db.cpp`:
  - tables `fleet`, `aircraft_type`, `airplane`, `airplane_cabin`, `airplane_hold`, `ledger_category`,
    `ledger_line`, `lease_charge`, `airport`, `airport_import`, all `STRICT`;
  - every index listed there, plus the partial unique index on `opening_cash`;
  - `ledger_category` rows `('opening_cash', 'Opening cash')` and `('lease', 'Lease')`.

  The shape checks are `GLOB '[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]T[0-2][0-9]:[0-5][0-9]:[0-5][0-9]Z'` for
  instants and `GLOB '[0-9][0-9][0-9][0-9]-[0-1][0-9]-[0-3][0-9]'` for dates.
- [ ] **Step 4: Run the tests.** Expected: `AirlineDb.*` pass.
- [ ] **Step 5: Commit** with a message like "Airline database: schema v2 with migrations, test schema set aside
  (D-078, D-086, D-090, D-093)".

### Task 2: Money and dates

**Files:**
- Create: `src/money.h`, `src/money.cpp`, `src/dates.h`, `src/dates.cpp`
- Create: `tests/money_test.cpp`, `tests/dates_test.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Produces (`money.h`, namespace `blocktime`):
  - `using Cents = std::int64_t;`
  - `Cents roundToFiveDollars(Cents)`: to the nearest 500, halves away from zero (D-080: "$2.50 → $5").
  - `std::string formatMoneyFull(Cents)`: whole dollars with thousands commas. For example `$1,400,000`,
    `-$900,000`, `$0`.
  - `std::string formatMoneyShort(Cents)` (D-080):
    - from $1M up: `M`, at most 2 decimals, trailing zeros trimmed;
    - from $1k up: `k`, at most 1 decimal, trimmed. A k value that rounds to 1000 becomes M;
    - below $1k: whole dollars;
    - negatives get a leading `-`.
- Produces (`dates.h`, namespace `blocktime`):
  - `using Date = std::chrono::year_month_day;`
  - `std::optional<Date> parseDate(std::string_view)`: exactly `YYYY-MM-DD` and a valid date;
  - `std::string formatDate(Date)`;
  - `std::string formatInstant(Date)` returns `YYYY-MM-DDT00:00:00Z`;
  - `Date utcToday()`;
  - `Date leasePeriodStart(Date leasedOn, int period)`: period 0 is `leasedOn`. Period n is n months later on the
    same day, or the month's last day if shorter. Always counted from `leasedOn`, so 31 Jan → 28 Feb → 31 Mar.
  - `std::string formatLeasePeriod(Date start, Date nextStart)` gives `22 Nov – 21 Dec`, with an en dash and English
    month abbreviations.

- [ ] **Step 1: Write the failing tests:**
  - `Money.RoundsToNearestFive`: 250→500, 249→0, 749→500, 750→1000, −250→−500, 12'750'000→12'750'000.
  - `Money.FullForm`: 140'000'000→`$1,400,000`, −90'000'000→`-$900,000`, 0→`$0`, 50'000→`$500`.
  - `Money.ShortForm`:
    - 140'000'000→`$1.4M`;
    - 1'248'000'000→`$12.48M`;
    - 38'250'000→`$382.5k`;
    - 95'000→`$950`;
    - 500'000'000→`$5M`;
    - −140'000'000→`-$1.4M`;
    - 99'995'000→`$1M`.
  - `Dates.ParseRejectsBadShapes`: `2026-11-22` parses; `2026-11-31`, `2026-1-22`, `2026-11-22T00:00:00Z` and `""`
    don't.
  - `Dates.LeasePeriodsClampToMonthEnd`: from 2027-01-31, periods 1, 2 and 13 are 2027-02-28, 2027-03-31 and
    2028-02-29.
  - `Dates.LeasePeriodText`: (2026-11-22, 2026-12-22) gives `22 Nov – 21 Dec`.
  - `Dates.InstantShape`: `formatInstant(2026-11-22)` is `2026-11-22T00:00:00Z`.
- [ ] **Step 2: Run them to make sure they fail.** Expected: a compile error.
- [ ] **Step 3: Implement** with integer arithmetic only (no `double` for money). Dates use `std::chrono` calendar
  types.
- [ ] **Step 4: Run the tests.** Expected: `Money.*` and `Dates.*` pass.
- [ ] **Step 5: Commit**: "Money rounding and short form, lease month dates (D-080, D-083)".

### Task 3: Airport import and the blocktime-import tool

**Files:**
- Create: `src/csv.h`, `src/csv.cpp`, `src/airport_import.h`, `src/airport_import.cpp`, `src/import_main.cpp`
- Create: `tests/csv_test.cpp`, `tests/airport_import_test.cpp`
- Create: `docs/notes/airport-data.md` (linked from `docs/README.md`)
- Modify: `CMakeLists.txt` (new console target `blocktime-import`, linking `blocktime-core`), `README.md`

**Interfaces:**
- Consumes: `db::Database`, `db::Transaction` (Task 1).
- Produces (namespace `blocktime`):
  - `class CsvReader { explicit CsvReader(std::istream&); bool next(std::vector<std::string>& fields); };`. RFC 4180:
    quoted fields, `""` inside quotes, commas and newlines inside quotes, LF or CRLF endings.
  - `bool isPavedSurface(std::string_view surface)`: after upper-casing, true if it contains `ASP`, `CON`, `PEM`,
    `BIT`, `TAR` or `PAV`.
  - `inline constexpr int kMinRunwayFt = 4000;`
  - `struct ImportedAirport { std::string icao, name, country, timeZone; double latitude, longitude; int elevationFt, longestRunwayFt; };`
  - `struct AirportImportStats { int read = 0, kept = 0, notLargeOrMedium = 0, noIcao = 0, noRunway = 0, noTimeZone = 0, missingField = 0; };`
  - `struct AirportImport { std::vector<ImportedAirport> airports; AirportImportStats stats; std::string error; };`
  - `AirportImport filterAirports(std::istream& airportsCsv, std::istream& runwaysCsv, std::istream& airportsJson)`
  - `bool writeAirports(db::Database&, const std::vector<ImportedAirport>&, std::string_view importedAt, std::string* error)`

The filter, D-084 as agreed:
- OurAirports `type` is `large_airport` or `medium_airport`.
- The ICAO code is the `icao_code` column if the header has it and it's not empty, else `gps_code`. It must be 4
  capitals or digits.
- The longest runway counts only runways with `closed` = 0, a paved `surface` and a `length_ft`. It must be at least
  `kMinRunwayFt`.
- The time zone is the `tz` of that ICAO key in mwgg's `airports.json`, and it must be known to
  `std::chrono::locate_zone`.
- `elevation_ft`, latitude and longitude must be present, else the airport counts as `missingField`.
- Columns are found by header name. A missing required column (`id`, `type`, `name`, `latitude_deg`,
  `longitude_deg`, `elevation_ft`, `iso_country`, `gps_code`; in runways `airport_ref`, `length_ft`, `surface`,
  `closed`) sets `error` to `missing column <name> in <file>`, with no airports.

`writeAirports` works in one transaction:
- an upsert by `icao_code` (`INSERT … ON CONFLICT (icao_code) DO UPDATE`);
- one `airport_import` row (`imported_at`, `airport_count`).

`blocktime-import <folder> [--db <file>]`:
- reads `airports.csv`, `runways.csv` and `airports.json` from `<folder>`;
- opens the airline database with `openAirlineDatabase` (default `defaultAirlineDatabaseFile()`);
- writes, and prints the stats.
- Exit code 0 on success, 1 on any error, with the message on stderr.

- [ ] **Step 1: Check the sources before using them (D-084 note).**
  - Read the `LICENSE` text in github.com/mwgg/Airports and confirm it covers `airports.json`.
  - Read OurAirports' data terms (try ourairports.com/data, otherwise the GitHub mirror's README).
  - Ask Dave before downloading: name the three files, their source, and their rough sizes (about 12 MB, 4 MB and
    8 MB). Then download them into a scratch folder (outside the repo):
    - `https://davidmegginson.github.io/ourairports-data/airports.csv`
    - `https://davidmegginson.github.io/ourairports-data/runways.csv`
    - `https://raw.githubusercontent.com/mwgg/Airports/master/airports.json`
  - Write `docs/notes/airport-data.md`: the URLs, the licences quoted briefly, the header columns as found, and
    whether `icao_code` exists.
  - **If either licence forbids redistribution or use, stop and ask Dave.**
- [ ] **Step 2: Write the failing tests**, with small inline CSV/JSON strings:
  - `Csv.QuotedCommasNewlinesAndDoubledQuotes`: `a,"b,c","d""e","f\ng"\r\nh` gives rows `[a, b,c, d"e, f\ng]` and
    `[h]`.
  - `AirportImport.KeepsLargeAndMediumWithLongPavedRunwayAndZone`: KSAN (large, 9,401 ft ASP, tz
    `America/Los_Angeles`) is kept with `longestRunwayFt == 9401`.
  - `AirportImport.DropsShortUnpavedClosedAndSmall`, one airport per reason:
    - a 3,999 ft runway (`noRunway`);
    - a 6,000 ft `GRASS` runway (`noRunway`);
    - a closed 8,000 ft runway (`noRunway`);
    - a `small_airport` (`notLargeOrMedium`);
    - no ICAO (`noIcao`);
    - no tz entry (`noTimeZone`).
  - `AirportImport.PrefersIcaoCodeColumnOverGpsCode`.
  - `AirportImport.MissingColumnIsAnError`: an airports header without `latitude_deg` gives an `error` containing
    `missing column latitude_deg`, and no airports.
  - `AirportImport.WriteUpsertsAndLogsTheImport`: write KSAN, then write it again with a new name. There's one
    airport row with the new name and two `airport_import` rows.
  - `AirportImport.PavedSurfaces`: `ASP`, `asphalt`, `CONC`, `PEM`, `BIT` are paved; `GRS`, `TURF`, `GRAVEL`,
    `WATER` aren't.
- [ ] **Step 3: Run them to make sure they fail.** Expected: a compile error.
- [ ] **Step 4: Implement** `csv`, `airport_import` and `import_main.cpp`. Parse `airports.json` with
  nlohmann-json: an object keyed by ICAO, each value with a `tz` string.
- [ ] **Step 5: Run the tests.** Expected: `Csv.*` and `AirportImport.*` pass.
- [ ] **Step 6: Run against the real files:**
  - `build\x64-release\blocktime-import.exe <scratch folder> --db <scratch>\test.db`.
  - Expected: exit 0, and KBFI, KSAN, KPAE and KSEA are in the database.
  - Record the kept count, each drop count and the database file size in `docs/notes/airport-data.md`. This was the
    "to be measured" item in `database.md`; update it there too.
- [ ] **Step 7: Update `README.md`** with a section "Airport data". It gives the three download commands as
  PowerShell `Invoke-WebRequest` lines into `%LOCALAPPDATA%\BlockTime\airport-data\`, then the
  `blocktime-import.exe` command. Refreshing is the same steps again. Log **D-094 (Proposed)**: airports come in
  through a separate console tool from files Dave downloads. The app needs no network code for it, and the output
  is visible on the console.
- [ ] **Step 8: Commit**: "Airport import tool: OurAirports + airport time zones, filtered (D-079, D-084, D-094)".

### Task 4: Catalog and airline config files

**Files:**
- Create: `src/catalog.h`, `src/catalog.cpp`, `src/airline_config.h`, `src/airline_config.cpp`
- Already written before the plan ran (2026-11-22, so Dave could fill in his copy): `config/catalog.yaml`,
  `config/airline.example.yaml`, and Dave's own `%LOCALAPPDATA%\BlockTime\airline.yaml` and `catalog.yaml`. This
  task checks them against the readers and fixes them if a rule below rejects them. **Never overwrite Dave's
  `%LOCALAPPDATA%` copies**; if one needs a change, tell him what to edit.
- Create: `tests/catalog_test.cpp`, `tests/airline_config_test.cpp`
- Modify: `vcpkg.json` (add `yaml-cpp`), `CMakeLists.txt` (`find_package(yaml-cpp CONFIG REQUIRED)`; link
  `yaml-cpp::yaml-cpp` into core; check the target name in vcpkg's usage output)

**Interfaces:**
- Consumes: `Cents` (Task 2), `Date` and `parseDate` (Task 2).
- Produces (`catalog.h`):
  - `struct CatalogType { std::string code, family, role; int turnaroundMin, cruiseSpeedKt; Cents monthlyLeaseCents; };`
  - `struct Cabin { std::string cabinClass; int seats; };`
  - `struct Hold { std::string hold; int maxLb, ratio; };`
  - `struct CatalogAirframe { std::string name, typeCode; std::vector<Cabin> cabins; std::vector<Hold> holds; };`
  - `struct Catalog { std::vector<CatalogType> types; std::vector<CatalogAirframe> airframes; const CatalogType* type(std::string_view code) const; const CatalogAirframe* airframe(std::string_view name) const; };`
  - `std::optional<Catalog> parseCatalog(std::string_view yaml, std::string* error);`
  - `std::optional<Catalog> loadCatalog(const std::filesystem::path&, std::string* error);`
- Produces (`airline_config.h`):
  - `struct AirlineIdentity { std::string name, icaoCode, callsign; };`
  - `struct ConfigAirplane { std::string registration, tailNumber /* "" = none */, airframe, fleet, simbriefAirframeId, locationIcao; Date leasedOn; bool awaitingDelivery = false; };`
  - `struct AirlineConfig { AirlineIdentity identity; Date foundedOn; Cents startingCashCents; std::vector<std::string> fleets; std::vector<ConfigAirplane> airplanes; };`
  - `std::optional<AirlineConfig> parseAirlineConfig(std::string_view yaml, std::string* error);`
  - `std::optional<AirlineConfig> loadAirlineConfig(const std::filesystem::path&, std::string* error);`
  - `std::filesystem::path defaultCatalogFile();` and `std::filesystem::path defaultAirlineConfigFile();`, next to
    `settings.json`.

File shapes (keys exactly these; money in whole dollars in YAML):

```yaml
# catalog.yaml
types:
  - code: 737-800 BBJ2
    family: "737"
    role: executive            # airliner | executive | freighter
    turnaround_min: 35
    cruise_speed_kt: 450
    lease_per_month: 900000
airframes:
  - name: 737-800 BBJ2 VIP
    type: 737-800 BBJ2
    cabins: { economy: 48 }
    holds:
      - { hold: forward, max_lb: 7500, ratio: 4 }
      - { hold: aft, max_lb: 9500, ratio: 5 }
```

```yaml
# airline.yaml
airline: { name: BlockTime Air, icao: BTA, callsign: BLOCKTIME }
founded_on: 2026-11-22
starting_cash: 5000000
fleets: [Executive]
airplanes:
  - registration: N738BT
    tail_number: "738"           # optional
    airframe: 737-800 BBJ2 VIP
    fleet: Executive
    simbrief_airframe_id: "replace-me"
    leased_on: 2026-11-22
    location: KBFI
    awaiting_delivery: true      # optional, default false
```

Validation; each failure is an error naming the file and, where yaml-cpp gives one, the line, as
`catalog.yaml line 12: ...`:
- **Catalog:**
  - unique type codes and airframe names;
  - `family` is `737` or `777`, and `role` is one of the three;
  - positive integers, and `lease_per_month` a multiple of 5;
  - an airframe's `type` exists;
  - freighter airframes have no cabins; airliner and executive airframes have at least one;
  - cabin classes are `economy`, `business` or `first`, and hold names `forward`, `aft`, `bulk` or `main_deck`, with
    no duplicates;
  - at least one hold.
- **Airline config:**
  - `icao` is 3 capitals and `name` and `callsign` aren't empty;
  - `starting_cash` is positive and a multiple of 5;
  - registrations match `[A-Z0-9-]+` and are unique, and so are `simbrief_airframe_id`s;
  - `fleet` is in `fleets`;
  - `leased_on >= founded_on`;
  - `location` is 4 capitals or digits.
- Whether `airframe` exists in the catalog is checked at leasing time (Task 5), since it spans both files.

`config/catalog.yaml` holds every type and airframe from design.md §3:
- lease rates from §5, turnarounds from §7, cruise speeds 450 kt (737s) and 490 kt (777s);
- seats from §3's estimates;
- holds as estimates. Each estimated number is commented `# estimate (design.md §17): replace from the PMDG load
  page`.

The BBJ2 airframe has 48 economy seats (the VIP fare is per seat whatever the class, §12). `airline.example.yaml` is
the block above.

- [ ] **Step 1: Write the failing tests:**
  - `Catalog.ParsesTheRepoCatalog`: loads `config/catalog.yaml` (path from a `BLOCKTIME_SOURCE_DIR` compile
    definition on the test target). The BBJ2 type is `executive`, at 90'000'000 cents.
  - `Catalog.RejectsCabinsOnAFreighter`, `Catalog.RejectsUnknownType` and `Catalog.RejectsLeaseNotMultipleOfFive`:
    each error names the rule.
  - `Config.ParsesTheExample`: loads `config/airline.example.yaml`. N738BT is at KBFI, awaiting delivery, leased on
    2026-11-22, and starting cash is 500'000'000.
  - `Config.RejectsLeaseBeforeFounding`, `Config.RejectsDuplicateRegistration` and `Config.RejectsUnknownFleet`.
  - `Config.YamlErrorNamesTheLine`: badly indented YAML gives an error starting `airline.yaml line `.
- [ ] **Step 2: Run them to make sure they fail.** Expected: a compile error.
- [ ] **Step 3: Implement** the readers, and check the existing config files pass. Log **D-095 (Proposed)**: yaml-cpp for the
  config files. It's mature and MIT-licensed, its API is simple, and it gives line numbers for errors. rapidyaml
  was the alternative (faster, but speed doesn't matter here).
- [ ] **Step 4: Run the tests.** Expected: `Catalog.*` and `Config.*` pass.
- [ ] **Step 5: Commit**: "Catalog and airline config files (D-076, D-082, D-091, D-095)".

### Task 5: Leasing

**Files:**
- Create: `src/leasing.h`, `src/leasing.cpp`, `src/airline_queries.h`, `src/airline_queries.cpp`
- Create: `tests/leasing_test.cpp`

**Interfaces:**
- Consumes: Tasks 1, 2 and 4.
- Produces (`leasing.h`):
  - `bool syncCatalogTypes(db::Database&, const Catalog&, std::string* error)`: upserts every type by `code` (D-093).
  - `bool syncFleets(db::Database&, const AirlineConfig&, std::string* error)`: inserts missing fleets, never
    deletes.
  - `bool postOpeningCash(db::Database&, const AirlineConfig&, std::string* error)`: posts
    `roundToFiveDollars(startingCash)` at `formatInstant(foundedOn)`, only if there's no `opening_cash` line yet.
  - `struct LeaseOutcome { int leased = 0, monthsCharged = 0, returned = 0; std::vector<std::string> notices; std::string error; };`
  - `LeaseOutcome settleLeases(db::Database&, const Catalog&, const AirlineConfig&, Date today);`
  - `std::vector<std::string> configDriftWarnings(db::Database&, const AirlineConfig&);`
- Produces (`airline_queries.h`):
  - `struct AirplaneView { std::string registration, tailNumber, typeCode, airframeName, fleet, locationIcao; int fuelOnBoardLb; bool awaitingDelivery; std::optional<Date> returnedOn; std::string returnReason; };`
  - `std::vector<AirplaneView> listAirplanes(db::Database&)`, by registration.
  - `struct LedgerView { std::string postedAt; std::string text; Cents amountCents; };`
  - `std::vector<LedgerView> listLedger(db::Database&)`, newest `posted_at` first, then the highest id first.
  - `Cents cashCents(db::Database&)`, `Cents cashCentsAsOf(db::Database&, std::string_view instant)` and
    `int airportCount(db::Database&)`.

`settleLeases` processes **lease events** in date order (D-083). Ties are broken by registration, and each event is
its own transaction:
- **Events:**
  - every in-fleet airplane (`returned_on IS NULL`) has a next period index, the number of its `lease_charge` rows,
    and an event at that period's start, if it's on or before `today`;
  - every config airplane not yet in the database whose `leased_on <= today` has an event at `leased_on`, period 0.
- **For each event**, `cash = cashCentsAsOf(formatInstant(date))` and `charge = monthly rate`:
  - **A new airplane:**
    - if its airframe isn't in the catalog, its location isn't in `airport`, or `cash < charge`, add a notice and
      skip it;
    - otherwise insert the airplane (copying the type's lease rate and the airframe's name, cabins and holds; fuel 0;
      `awaiting_delivery` from config), its first `lease` line (−charge) and its `lease_charge` row;
    - then queue its next period.
  - **An existing airplane:**
    - if `cash < charge`, set `returned_on = date` and `return_reason = 'unpaid'`, and drop its later events;
    - otherwise post the line and the `lease_charge`, and queue the next period.

Notice copy (exact):
- `Can't afford N738BT's first month: $900,000 needed, $400,000 in cash on 22 Nov 2026.`
- `N738BT: no airframe "737-800 BBJ" in catalog.yaml.`
- `N738BT: KBFI isn't in the airport data. Run blocktime-import (README).`
- `N738BT returned on 22 Jan 2027: the lease month couldn't be paid.`

Ledger text:
- `Opening cash`;
- `Lease N738BT, 22 Nov – 21 Dec`, built in `listLedger` from `lease_charge` and the next period's start.

`configDriftWarnings` covers each leased registration whose `airframe`, `fleet`, `simbrief_airframe_id`, `leased_on`
or `tail_number` differs from config: `N738BT: airline.yaml changed simbrief_airframe_id, but the database keeps
the leased values (D-082).` It also covers each leased registration missing from config: `N738BT is no longer in
airline.yaml; returning a lease comes with the settings screen (D-092).`

- [ ] **Step 1: Write the failing tests.** The fixture is an in-memory migrated database, KBFI and KSAN in
  `airport`, the repo catalog, and a config built in code (N738BT, BBJ2, $5M, founded and leased 2026-11-22).
  - `Leasing.FirstMonthChargedAtLease`: `settleLeases(today = 2026-11-22)` leases 1 airplane and charges 1 month.
    Cash is 410'000'000, and the ledger has `Lease N738BT, 22 Nov – 21 Dec` at −90'000'000 and `Opening cash`.
  - `Leasing.AirplaneCopiesItsAirframe`: N738BT has 48 economy seats and two holds, `awaiting_delivery` 1, fuel 0, at
    KBFI.
  - `Leasing.SettlingTwiceChangesNothing`: a second call on the same day leases 0, charges 0, and the line count is
    unchanged.
  - `Leasing.MonthsMissedWhileClosedAreChargedInDateOrder`:
    - two airplanes leased 2026-11-22 and 2026-12-05, a starting cash of $2M, and BBJ2 rates of $900k;
    - settle on 2026-11-22, then not again until 2027-01-10;
    - expected order of charges: 22 Nov A, 5 Dec B (cash now $200k), then on 22 Dec A can't be paid, so A is
      returned on 2026-12-22;
    - B's 5 Jan month also can't be paid, so B is returned on 2027-01-05;
    - exactly two return notices.
  - `Leasing.CantAffordFirstMonthLeasesNothing`: starting cash $500k. There's no airplane row and a notice with
    `$900,000 needed, $500,000 in cash`.
  - `Leasing.FutureLeaseWaitsForItsDay`: `leased_on` 2026-12-01 and today 2026-11-22. Nothing is leased; with today
    2026-12-01 it is.
  - `Leasing.UnknownAirportOrAirframeIsANotice`.
  - `Leasing.TypesRefreshFromCatalog`: sync, change the BBJ2's `turnaround_min` in the catalog, sync again. The row
    shows the new value, and N738BT's `monthly_lease_cents` is unchanged.
  - `Leasing.OpeningCashPostedOnce`.
  - `Leasing.DriftWarnings`: change N738BT's `simbrief_airframe_id` and add nothing else. One warning, with the
    exact copy.
- [ ] **Step 2: Run them to make sure they fail.** Expected: a compile error.
- [ ] **Step 3: Implement** `leasing` and `airline_queries`. Use a `std::priority_queue` (or a sorted vector
  re-sorted after each insert) for the events.
- [ ] **Step 4: Run the tests.** Expected: `Leasing.*` pass.
- [ ] **Step 5: Log D-096 (Proposed): lease settlement details.**
  - `founded_on` in `airline.yaml` dates the opening cash, and leases can't start before it.
  - A new airplane's first month is a lease event like any other, so everything is processed in one date-ordered
    pass.
  - Affordability uses cash as of the event's date.
  - Config airplanes whose `leased_on` is in the future are leased when the day comes, even while the app is open.
  - Rare retroactive cases (adding a back-dated airplane after later lines exist) aren't rechecked.
- [ ] **Step 6: Commit**: "Leasing: first month, monthly charges in date order, unpaid returns (D-082, D-083, D-093, D-096)".

### Task 6: AirlineSession, the app-facing entry point

**Files:**
- Create: `src/airline_session.h`, `src/airline_session.cpp`, `tests/airline_session_test.cpp`

**Interfaces:**
- Consumes: Tasks 1, 4 and 5.
- Produces:
  - `struct AirlinePaths { std::filesystem::path database, catalog, airlineConfig; };`
  - `AirlinePaths defaultAirlinePaths();`
  - `class AirlineSession`:
    - `static AirlineSession start(const AirlinePaths&, Date today, std::ostream& warnings);`
    - `void tick(Date today);` runs `settleLeases` again with the config read at start;
    - `const std::vector<std::string>& notices() const;` holds the notices from start plus the latest tick;
    - `std::vector<AirplaneView> airplanes();`, `std::vector<LedgerView> ledger();`, `Cents cash();`;
    - `std::string airlineName() const;` is "" without a valid config.

`start` does this in order:
1. Open the database (on failure, a notice with SQLite's message; everything else is skipped).
2. Add a notice `The Qt test's database was set aside as blocktime.test-schema.db.` if it was.
3. Load the catalog, then the airline config. A missing file gives `No catalog.yaml in <folder>. Copy config\catalog.yaml there (README).` or the same for `airline.yaml` and `config\airline.example.yaml`. A parse error gives the reader's message.
4. If `airportCount == 0`, add `No airport data yet. Run blocktime-import (README).`
5. With both configs valid: `syncCatalogTypes`, `syncFleets`, `postOpeningCash`, `settleLeases`, and write
   `configDriftWarnings` to `warnings`.

If anything fails, the session still answers `airplanes()`, `ledger()` and `cash()` from the database.

- [ ] **Step 1: Write the failing tests**, in a temp folder with real files:
  - `Session.FreshStartLeasesTheExample`: copy the repo's example and catalog, and give a database with KBFI imported.
    Expected: no notices, one airplane, cash 410'000'000, and `airlineName() == "BlockTime Air"`.
  - `Session.MissingConfigIsANotice`.
  - `Session.BadConfigStillShowsTheDatabase`: start once with good files, then break `airline.yaml` and start again.
    Expected: the airplane and ledger are still listed, and there's a notice starting `airline.yaml line`.
  - `Session.TickChargesAMonthThatFallsDueWhileOpen`: start on 2026-12-21, then `tick(2026-12-22)` adds one lease line.
  - `Session.NoAirportsIsANotice`.
- [ ] **Step 2: Run them to make sure they fail.** Expected: a compile error.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run the tests.** Expected: `Session.*` pass.
- [ ] **Step 5: Commit**: "AirlineSession: one call to open, sync and settle the airline".

### Task 7: The Qt models and app wiring

**Files:**
- Modify: `src/fleet_table_model.h`, `src/fleet_table_model.cpp`, `src/app_main.cpp`, `tests/qt_models_test.cpp`,
  `CMakeLists.txt`
- Create: `src/ledger_model.h`, `src/ledger_model.cpp`
- Delete: `src/fleet_store.h`, `src/fleet_store.cpp`, `tests/fleet_store_test.cpp`

**Interfaces:**
- Consumes: `AirlineSession`, `AirplaneView`, `LedgerView` and `formatMoney*` (Tasks 2, 5 and 6).
- Produces:
  - `FleetTableModel`:
    - `FleetTableModel(std::vector<blocktime::AirplaneView>, QStringList notices, QObject* parent = nullptr)`;
    - `void reset(std::vector<blocktime::AirplaneView>, QStringList notices)`;
    - `enum Column { Registration, Type, Airframe, Fleet, Location, Fuel, Status, ColumnCount }`;
    - `Q_PROPERTY(QStringList notices READ notices NOTIFY noticesChanged)`, in place of `error`;
    - sorting unchanged: Fuel sorts by number, the text columns case-insensitively.
    - Display text:
      - fuel is `12,128 lb`;
      - status is `Awaiting delivery`, `Parked`, or `Returned 22 Jan 2027`.
  - `LedgerModel` (`QAbstractListModel`, `QML_ELEMENT`, uncreatable):
    - roles `date` (`22 Nov 2026`), `text`, `amount` (`formatMoneyFull`) and `income` (bool);
    - `Q_PROPERTY(QString cashShort ...)` and `Q_PROPERTY(QString cashFull ...)`, NOTIFY `changed`;
    - `void reset(std::vector<blocktime::LedgerView>, blocktime::Cents cash)`.
  - New initial properties for QML: `ledger` (LedgerModel*) and `airlineName` (QString).
- In `app_main.cpp`, `openFleet` is replaced:
  - `AirlineSession::start(defaultAirlinePaths(), utcToday(), std::cerr)` (`std::cerr` goes to `errors.log`, D-067);
  - both models are built from it;
  - a `QTimer` every 60,000 ms calls `tick(utcToday())` and resets both models.
  - `--demo` opens the same session.

The Fleet table no longer counts flights and block hours from the log. The registration no longer matches the sim
(D-087), and stored hours come in milestone 6. Log **D-097 (Proposed)**.

- [ ] **Step 1: Rewrite the Fleet tests and add Ledger tests** in `qt_models_test.cpp`:
  - `Fleet.DefaultsToRegistrationAscending`, `Fleet.TextSortIsCaseInsensitive`, `Fleet.IgnoresABadSortColumn` (kept,
    on `AirplaneView` rows);
  - `Fleet.FuelSortsByNumberAndShowsPounds`: 950, 12128 and 3000 sort ascending as 950, 3000, 12128; the cell text is
    `12,128 lb`;
  - `Fleet.StatusText`: awaiting gives `Awaiting delivery`; returned 2027-01-22 gives `Returned 22 Jan 2027`;
  - `Fleet.ResetReplacesRowsAndNotices`;
  - `Ledger.RowsAndCash`: two rows give date `22 Nov 2026`, amount `-$900,000`, `income` false; cash 410'000'000 gives
    `cashShort` `$4.1M` and `cashFull` `$4,100,000`;
  - `Ledger.ResetEmitsChanged`.
  - Remove the flight-count tests (`FleetCountsBlockInFlightsPerTail`, `FleetRecountsWhenFlightsReload`,
    `FleetCarriesTheStoreError`).
- [ ] **Step 2: Run them to make sure they fail.** Expected: a compile error.
- [ ] **Step 3: Implement** the models and wiring, delete `fleet_store`, and update `CMakeLists.txt`.
- [ ] **Step 4: Build and run all tests.** Expected: every test passes, with no warnings.
- [ ] **Step 5: Commit**: "Fleet and Ledger models from the airline database; minute tick (D-077, D-097)".

### Task 8: The screens

**Files:**
- Create: `qml/MoneyPage.qml`
- Modify: `qml/Main.qml`, `qml/Sidebar.qml`, `qml/FleetPage.qml`, `qml/Theme.qml` (only if a new colour is needed),
  `CMakeLists.txt` (`BLOCKTIME_QML_FILES`)

- **Sidebar:**
  - entries Live, Flights, Fleet, **Money**, in that order;
  - the airline's name under the BlockTime brand when `airlineName` isn't empty.
- **Fleet page:**
  - the new columns;
  - above the table, one line per notice, in the existing card style;
  - the empty state reads `No airplanes yet.` with no notices, otherwise just the notices.
- **Money page:**
  - a `StatTile` with the cash (`cashShort`), its tooltip or subtitle `cashFull`;
  - below it, the ledger list: date, text and amount, with amounts right-aligned, income in the theme's positive
    colour and costs in the default text colour;
  - newest first, as the model gives it;
  - no animations.

- [ ] **Step 1: Build the pages** and wire them into `Main.qml`.
- [ ] **Step 2: Check with hot reload.** Run `build\x64-debug\BlockTime.exe --demo` against a scratch
  `%LOCALAPPDATA%` copy, or Dave's real folder once he agrees. Expected:
  - Fleet shows N738BT, `737-800 BBJ2`, `737-800 BBJ2 VIP`, `Executive`, `KBFI`, `0 lb`, `Awaiting delivery`;
  - Money shows `$4.1M` with two lines.
  - Then resize to the minimum window size and check nothing clips.
- [ ] **Step 3: qmllint.** Run `cmake --build --preset x64-debug --target blocktime-app_qmllint`. Expected: no output.
- [ ] **Step 4: Commit**: "Money page, real Fleet columns and notices, airline name in the sidebar".

### Task 9: Docs, version, review, and Dave's checks

**Files:**
- Modify: `CMakeLists.txt` and `vcpkg.json` (0.5.0), `README.md`, `docs/database.md` (Status: built),
  `docs/design.md` (only if something changed), `docs/decisions.md`, `docs/progress.md`, `CLAUDE.md` (repo map:
  `config/`; the core library list)

- [ ] **Step 1: README.**
  - First-run setup: copy `config\catalog.yaml` and `config\airline.example.yaml` (renamed `airline.yaml`) into
    `%LOCALAPPDATA%\BlockTime\`, edit the airline and the airplane, then run the airport import.
  - What the test database set-aside means.
  - The new test suites.
  - Check every command from a clean checkout.
- [ ] **Step 2: Version 0.5.0.** Then a clean release and debug build, with no warnings. Run
  `ctest --preset x64-release`; every test passes. Run qmllint.
- [ ] **Step 3: Code review.** Follow `superpowers:requesting-code-review` on the milestone's commits, then
  `superpowers:receiving-code-review`. Fix Critical and Important findings; list deferred Minor ones in
  `progress.md`.
- [ ] **Step 4: `superpowers:verification-before-completion`.** Rebuild, retest and rerun qmllint after the fixes.
- [ ] **Step 5: Progress entry.** It lists what was built, the test count, the D-094..D-097 Proposed decisions, and
  Dave's checks. Milestone 4 changes nothing in the recorder, so no sim is needed:
  1. Download the airport files and run `blocktime-import.exe`. Expected: a kept count close to the one in
     `docs/notes/airport-data.md`.
  2. Put his `catalog.yaml` and `airline.yaml` in `%LOCALAPPDATA%\BlockTime\` with his real airline name, ICAO,
     callsign, the BBJ2's registration and SimBrief airframe ID, and today as `founded_on` and `leased_on`.
  3. Start `build\x64-release\BlockTime.exe`. Expected:
     - the airline name in the sidebar;
     - Fleet: the BBJ2 at KBFI, `Awaiting delivery`, `0 lb`;
     - Money: `$4.1M` from `Opening cash +$5,000,000` and `Lease <reg>, <d Mon> – <d Mon> -$900,000`;
     - the old `blocktime.db` renamed `blocktime.test-schema.db`, and Fleet shows that notice once.
  4. Close and reopen. Expected: the same cash, and no second lease line.
  5. Break `airline.yaml`'s indentation and reopen. Expected: the same screens plus a notice naming the line. Then
     fix it.
- [ ] **Step 6: Commit**: "Milestone 4 docs, version 0.5.0".
