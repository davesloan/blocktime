// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "csv.h"

namespace blocktime {

bool CsvReader::next(std::vector<std::string>& fields) {
    fields.clear();
    if (in_.peek() == std::char_traits<char>::eof()) return false;

    std::string field;
    bool quoted = false;
    for (int c = in_.get(); c != std::char_traits<char>::eof(); c = in_.get()) {
        if (quoted) {
            if (c != '"') {
                field += static_cast<char>(c);
            } else if (in_.peek() == '"') {
                field += '"';
                in_.get();
            } else {
                quoted = false;
            }
        } else if (c == '"') {
            quoted = true;
        } else if (c == ',') {
            fields.push_back(std::move(field));
            field.clear();
        } else if (c == '\r' || c == '\n') {
            if (c == '\r' && in_.peek() == '\n') in_.get();
            fields.push_back(std::move(field));
            return true;
        } else {
            field += static_cast<char>(c);
        }
    }
    fields.push_back(std::move(field));
    return true;
}

}  // namespace blocktime
