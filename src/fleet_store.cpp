// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fleet_store.h"

#include <array>
#include <utility>

#include <sqlite3.h>

namespace blocktime {

namespace {

// The sample fleet (test schema, D-061): Dave's two real tails plus made-up ones covering design.md §3's types.
constexpr std::array<std::array<const char*, 5>, 8> kSeed{{
    {"N818SX", "737-800", "Two-class", "KSAN", "Parked"},
    {"N916SX", "777-300ER", "Standard", "KOAK", "Parked"},
    {"N601SX", "737-600", "All-economy", "KBFI", "Parked"},
    {"N802SX", "737-800", "All-economy", "KSEA", "Maintenance"},
    {"N880SX", "737-800 BBJ2", "VIP", "KLAS", "Parked"},
    {"N840SX", "737-800 BCF", "Freighter", "KONT", "Parked"},
    {"N720SX", "777-200ER", "Trent 895", "KSFO", "Parked"},
    {"N770SX", "777F", "Freighter", "PANC", "Parked"},
}};

constexpr const char* kSchema =
    "CREATE TABLE IF NOT EXISTS airframes ("
    "  registration TEXT PRIMARY KEY,"
    "  type TEXT NOT NULL,"
    "  configuration TEXT NOT NULL,"
    "  location TEXT NOT NULL,"
    "  status TEXT NOT NULL);"
    "PRAGMA user_version = 1;";

// A prepared statement, finalized when it goes out of scope.
class Statement {
public:
    Statement(sqlite3* db, const char* sql) { sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr); }
    ~Statement() { sqlite3_finalize(stmt_); }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool ok() const { return stmt_ != nullptr; }
    bool step() { return stmt_ != nullptr && sqlite3_step(stmt_) == SQLITE_ROW; }
    int integer(int col) const { return sqlite3_column_int(stmt_, col); }
    std::string text(int col) const {
        const auto* p = sqlite3_column_text(stmt_, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    }
    sqlite3_stmt* get() { return stmt_; }

private:
    sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace

std::optional<FleetStore> FleetStore::open(const std::string& file, std::string* error) {
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(file.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc == SQLITE_OK) {
        char* msg = nullptr;
        if (sqlite3_exec(db, kSchema, nullptr, nullptr, &msg) == SQLITE_OK) {
            if (error) error->clear();
            return FleetStore(db);
        }
        if (error) *error = msg ? msg : "could not create the schema";
        sqlite3_free(msg);
    } else if (error) {
        *error = db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    }
    sqlite3_close_v2(db);
    return std::nullopt;
}

FleetStore::FleetStore(FleetStore&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

FleetStore& FleetStore::operator=(FleetStore&& other) noexcept {
    std::swap(db_, other.db_);
    return *this;
}

FleetStore::~FleetStore() { sqlite3_close_v2(db_); }

void FleetStore::seedIfEmpty() {
    Statement count(db_, "SELECT COUNT(*) FROM airframes");
    if (!count.step() || count.integer(0) != 0) return;

    sqlite3_exec(db_, "BEGIN", nullptr, nullptr, nullptr);
    Statement insert(db_, "INSERT INTO airframes (registration, type, configuration, location, status) "
                          "VALUES (?, ?, ?, ?, ?)");
    for (const auto& row : kSeed) {
        for (int i = 0; i < 5; ++i) sqlite3_bind_text(insert.get(), i + 1, row[i], -1, SQLITE_STATIC);
        sqlite3_step(insert.get());
        sqlite3_reset(insert.get());
    }
    sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr);
}

std::vector<Airframe> FleetStore::airframes() const {
    std::vector<Airframe> out;
    Statement q(db_, "SELECT registration, type, configuration, location, status FROM airframes ORDER BY registration");
    while (q.step()) out.push_back(Airframe{q.text(0), q.text(1), q.text(2), q.text(3), q.text(4)});
    return out;
}

int FleetStore::userVersion() const {
    Statement q(db_, "PRAGMA user_version");
    return q.step() ? q.integer(0) : 0;
}

}  // namespace blocktime
