// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Calendar dates for the airline: the database's date and instant text shapes (D-081), lease months (D-083), and
// the short date texts the screens show.

#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace blocktime {

using Date = std::chrono::year_month_day;

// Exactly YYYY-MM-DD and a real date; anything else is nullopt.
std::optional<Date> parseDate(std::string_view text);
std::string formatDate(Date date);       // "2026-11-22"
std::string formatInstant(Date date);    // "2026-11-22T00:00:00Z": the start of the day in UTC
std::string formatDayMonthYear(Date date);  // "22 Nov 2026"
Date utcToday();

// The local hour (0-23) in an IANA time zone at `at`, or nullopt if the zone isn't known.
std::optional<int> localHour(std::string_view ianaZone, std::chrono::sys_seconds at);

// The start of lease month `period` (0 = the lease start): the same day of the month `period` months later, or that
// month's last day if it's shorter. Always counted from `leasedOn`, so 31 Jan → 28 Feb → 31 Mar.
Date leasePeriodStart(Date leasedOn, int period);

// "22 Nov – 21 Dec": from `start` to the day before `nextStart`.
std::string formatLeasePeriod(Date start, Date nextStart);

}  // namespace blocktime
