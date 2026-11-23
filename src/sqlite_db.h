// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A thin RAII layer over the sqlite3 C API (D-061): a connection, prepared statements and transactions. Errors are
// returned, not thrown, like the rest of the core library.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

struct sqlite3;
struct sqlite3_stmt;

namespace blocktime::db {

// How long a write waits for another connection (the app and blocktime-import) before failing with "database is
// locked".
inline constexpr int kBusyTimeoutMs = 5000;

class Database {
public:
    // Opens `file` read/write, creating it if missing. On failure returns nullopt and sets *error.
    static std::optional<Database> open(const std::filesystem::path& file, std::string* error);
    static Database openInMemory();

    Database(Database&& other) noexcept;
    Database& operator=(Database&& other) noexcept;
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    ~Database();

    // Runs one or more statements. False on the first failure, with SQLite's message in *error.
    bool exec(std::string_view sql, std::string* error = nullptr);
    int userVersion();
    sqlite3* handle() { return db_; }

private:
    explicit Database(sqlite3* db) : db_(db) {}
    sqlite3* db_ = nullptr;
};

class Statement {
public:
    Statement(Database& db, std::string_view sql);
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    ~Statement();

    // Parameters are 1-based.
    void bind(int index, std::int64_t value);
    void bind(int index, std::string_view value);
    void bind(int index, double value);
    void bindNull(int index);

    // True while there's a row to read. False when done or on an error (see error()).
    bool next();
    // Steps to the end. True on success.
    bool run();
    void reset();

    std::int64_t integer(int col);
    double real(int col);
    std::string text(int col);
    bool isNull(int col);

    // The prepare or step error, "" if none.
    const std::string& error() const { return error_; }

private:
    sqlite3* db_ = nullptr;
    sqlite3_stmt* stmt_ = nullptr;
    std::string error_;
};

// BEGIN IMMEDIATE on construction; rolls back on destruction unless commit() succeeded.
class Transaction {
public:
    explicit Transaction(Database& db);
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction();

    bool commit(std::string* error = nullptr);
    // False if BEGIN failed (another connection held the lock past the busy timeout, or a transaction was already
    // open). Callers must then write nothing: their statements would each commit on their own.
    bool active() const { return active_; }
    const std::string& error() const { return error_; }  // why BEGIN failed

private:
    Database& db_;
    bool active_ = false;
    std::string error_;
};

}  // namespace blocktime::db
