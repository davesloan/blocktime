// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline database (docs/database.md): opening it, setting the Qt test's database aside (D-086), and the
// numbered migrations tracked with PRAGMA user_version (D-078).

#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "sqlite_db.h"

namespace blocktime {

inline constexpr int kAirlineSchemaVersion = 12;

// Sets the connection's pragmas (foreign_keys, WAL, synchronous FULL), then applies every migration above the
// database's user_version, each in its own transaction. Refuses a database from a newer BlockTime, and the Qt test's
// schema (user_version 1), which openAirlineDatabase sets aside first.
bool migrateAirlineDatabase(db::Database& db, std::string* error);
// Applies the migrations up to `version` only. For tests that need an older schema.
bool migrateAirlineDatabaseTo(db::Database& db, int version, std::string* error);

struct AirlineDbOpen {
    std::optional<db::Database> db;
    std::string error;
    bool testSchemaSetAside = false;
    bool versionRestored = false;  // an older BlockTime had set a real airline's user_version back to 1
    std::filesystem::path backup;  // the copy taken before migrating (blocktime.before-schema-N.db); "" if none
};

// Opens (or creates) the airline database and migrates it. A file holding the Qt test's schema (user_version 1, no
// airplane table) is renamed to blocktime.test-schema.db in the same folder and a new database is built in its place.
// A real airline whose user_version an older BlockTime set to 1 gets its version back (versionRestored).
// An existing airline with migrations pending is first copied to blocktime.before-schema-N.db in the same folder (N
// the version it's migrated to) with VACUUM INTO; if that can't be done it isn't migrated (review 1.5, D-226).
AirlineDbOpen openAirlineDatabase(const std::filesystem::path& file);

std::filesystem::path defaultAirlineDatabaseFile();  // %LOCALAPPDATA%\BlockTime\blocktime.db

namespace detail {  // for tests

struct Migration {
    int version;
    const char* sql;
    // Rebuilds a table by SQLite's 12-step recipe (sqlite.org/lang_altertable.html): create the new table, copy, drop
    // the old one, rename. It needs foreign keys off, or dropping a parent fails (RESTRICT) or deletes its children
    // (CASCADE) (review 1.5).
    bool rebuild = false;
};

// Applies one migration in its own transaction and sets user_version to its version. A rebuild runs with foreign
// keys off and must leave PRAGMA foreign_key_check empty before it commits. Foreign keys are on again afterwards,
// whether it worked or not.
bool applyMigration(db::Database& db, const Migration& m, std::string* error);

}  // namespace detail

}  // namespace blocktime
