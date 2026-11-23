// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Percent-encoding for query values in the URLs blocktime-core builds (SimBrief, SayIntentions).

#pragma once

#include <string>
#include <string_view>

namespace blocktime {

// Every byte except A-Z a-z 0-9 - _ . ~ becomes %XX.
inline std::string percentEncode(std::string_view value) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (const char ch : value) {
        const auto c = static_cast<unsigned char>(ch);
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
            c == '.' || c == '~') {
            out += ch;
        } else {
            out += '%';
            out += kHex[c >> 4];
            out += kHex[c & 15];
        }
    }
    return out;
}

}  // namespace blocktime
