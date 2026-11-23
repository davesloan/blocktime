// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// A small RFC 4180 CSV reader for the OurAirports files: quoted fields, "" inside quotes, commas and newlines inside
// quotes, LF or CRLF line endings.

#pragma once

#include <istream>
#include <string>
#include <vector>

namespace blocktime {

class CsvReader {
public:
    explicit CsvReader(std::istream& in) : in_(in) {}
    // Reads the next row into `fields`. False at the end of the input.
    bool next(std::vector<std::string>& fields);

private:
    std::istream& in_;
};

}  // namespace blocktime
