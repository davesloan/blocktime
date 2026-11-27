// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Reading one field of a log line's data without throwing (review 1.6). nlohmann's value(key, fallback) and get<T>()
// throw when the key is there but null or of another type, and NaN is written to the log as null; these give the
// fallback instead. Use them wherever a log line or a web answer is read on the GUI thread or into history.

#pragma once

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace blocktime::fields {

inline const nlohmann::json* find(const nlohmann::json& d, std::string_view key) {
    if (!d.is_object()) return nullptr;
    const auto it = d.find(key);
    return it == d.end() ? nullptr : &*it;
}

inline double number(const nlohmann::json& d, std::string_view key, double fallback = 0.0) {
    const nlohmann::json* v = find(d, key);
    return v && v->is_number() ? v->get<double>() : fallback;
}

inline long long integer(const nlohmann::json& d, std::string_view key, long long fallback = 0) {
    const nlohmann::json* v = find(d, key);
    if (!v || !v->is_number()) return fallback;
    return v->is_number_integer() ? v->get<long long>() : static_cast<long long>(v->get<double>());
}

inline bool flag(const nlohmann::json& d, std::string_view key, bool fallback = false) {
    const nlohmann::json* v = find(d, key);
    return v && v->is_boolean() ? v->get<bool>() : fallback;
}

inline std::string text(const nlohmann::json& d, std::string_view key, std::string fallback = {}) {
    const nlohmann::json* v = find(d, key);
    return v && v->is_string() ? v->get<std::string>() : fallback;
}

}  // namespace blocktime::fields
