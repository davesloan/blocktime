// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The airline's airframes in SQLite (D-061), for the Fleet screen. Plain C++ on the sqlite3 C API, so the
// data layer has no Qt in it. The schema is a test schema (user_version 1), not the real airline schema.

#pragma once

#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace blocktime {

struct Airframe {
    std::string registration;
    std::string type;
    std::string configuration;
    std::string location;
    std::string status;
};

class FleetStore {
public:
    // Opens or creates `file` (UTF-8; ":memory:" for tests) and creates the test schema. On failure returns nullopt
    // and sets *error to SQLite's message.
    static std::optional<FleetStore> open(const std::string& file, std::string* error);

    FleetStore(FleetStore&& other) noexcept;
    FleetStore& operator=(FleetStore&& other) noexcept;
    FleetStore(const FleetStore&) = delete;
    FleetStore& operator=(const FleetStore&) = delete;
    ~FleetStore();

    // Inserts the sample airframes, only into an empty table.
    void seedIfEmpty();
    // Every airframe, ordered by registration.
    std::vector<Airframe> airframes() const;
    // PRAGMA user_version: the schema version.
    int userVersion() const;

private:
    explicit FleetStore(sqlite3* db) : db_(db) {}
    sqlite3* db_ = nullptr;
};

}  // namespace blocktime
