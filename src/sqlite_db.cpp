// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sqlite_db.h"

#include <utility>

#include <sqlite3.h>

namespace blocktime::db {

std::optional<Database> Database::open(const std::filesystem::path& file, std::string* error) {
    const std::u8string utf8 = file.u8string();  // sqlite3 takes UTF-8
    const std::string name(utf8.begin(), utf8.end());
    sqlite3* db = nullptr;
    const int rc = sqlite3_open_v2(name.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc == SQLITE_OK) {
        if (error) error->clear();
        return Database(db);
    }
    if (error) *error = db ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    sqlite3_close_v2(db);
    return std::nullopt;
}

Database Database::openInMemory() {
    sqlite3* db = nullptr;
    sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    return Database(db);
}

Database::Database(Database&& other) noexcept : db_(std::exchange(other.db_, nullptr)) {}

Database& Database::operator=(Database&& other) noexcept {
    std::swap(db_, other.db_);
    return *this;
}

Database::~Database() { sqlite3_close_v2(db_); }

bool Database::exec(std::string_view sql, std::string* error) {
    char* msg = nullptr;
    const std::string text(sql);
    if (sqlite3_exec(db_, text.c_str(), nullptr, nullptr, &msg) == SQLITE_OK) {
        if (error) error->clear();
        return true;
    }
    if (error) *error = msg ? msg : sqlite3_errmsg(db_);
    sqlite3_free(msg);
    return false;
}

int Database::userVersion() {
    Statement q(*this, "PRAGMA user_version");
    return q.next() ? static_cast<int>(q.integer(0)) : 0;
}

Statement::Statement(Database& db, std::string_view sql) : db_(db.handle()) {
    if (sqlite3_prepare_v2(db_, sql.data(), static_cast<int>(sql.size()), &stmt_, nullptr) != SQLITE_OK) {
        error_ = sqlite3_errmsg(db_);
        stmt_ = nullptr;
    }
}

Statement::~Statement() { sqlite3_finalize(stmt_); }

void Statement::bind(int index, std::int64_t value) { sqlite3_bind_int64(stmt_, index, value); }

void Statement::bind(int index, std::string_view value) {
    sqlite3_bind_text(stmt_, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
}

void Statement::bind(int index, double value) { sqlite3_bind_double(stmt_, index, value); }

void Statement::bindNull(int index) { sqlite3_bind_null(stmt_, index); }

bool Statement::next() {
    if (!stmt_) return false;
    const int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc != SQLITE_DONE) error_ = sqlite3_errmsg(db_);
    return false;
}

bool Statement::run() {
    if (!stmt_) return false;
    int rc = SQLITE_ROW;
    while (rc == SQLITE_ROW) rc = sqlite3_step(stmt_);
    if (rc == SQLITE_DONE) return true;
    error_ = sqlite3_errmsg(db_);
    return false;
}

void Statement::reset() {
    sqlite3_reset(stmt_);
    sqlite3_clear_bindings(stmt_);
}

std::int64_t Statement::integer(int col) { return sqlite3_column_int64(stmt_, col); }

double Statement::real(int col) { return sqlite3_column_double(stmt_, col); }

std::string Statement::text(int col) {
    const auto* p = sqlite3_column_text(stmt_, col);
    return p ? reinterpret_cast<const char*>(p) : "";
}

bool Statement::isNull(int col) { return sqlite3_column_type(stmt_, col) == SQLITE_NULL; }

Transaction::Transaction(Database& db) : db_(db) { active_ = db_.exec("BEGIN IMMEDIATE"); }

Transaction::~Transaction() {
    if (active_) db_.exec("ROLLBACK");
}

bool Transaction::commit(std::string* error) {
    if (!active_) {
        if (error) *error = "no transaction is open";
        return false;
    }
    if (!db_.exec("COMMIT", error)) return false;
    active_ = false;
    return true;
}

}  // namespace blocktime::db
