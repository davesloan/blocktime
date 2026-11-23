// BlockTime
// Copyright (C) 2026 Dave Sloan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "dates.h"

#include <algorithm>
#include <array>
#include <format>

namespace blocktime {

using namespace std::chrono;

namespace {

constexpr std::array<const char*, 12> kMonths{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                              "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

const char* monthName(Date date) { return kMonths[static_cast<unsigned>(date.month()) - 1]; }

std::string dayMonth(Date date) { return std::format("{} {}", static_cast<unsigned>(date.day()), monthName(date)); }

bool isDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::optional<Date> parseDate(std::string_view text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return std::nullopt;
    for (std::size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u}) {
        if (!isDigit(text[i])) return std::nullopt;
    }
    const auto number = [&](std::size_t from, std::size_t count) {
        int n = 0;
        for (std::size_t i = from; i < from + count; ++i) n = n * 10 + (text[i] - '0');
        return n;
    };
    const Date date{year{number(0, 4)}, month{static_cast<unsigned>(number(5, 2))},
                    day{static_cast<unsigned>(number(8, 2))}};
    if (!date.ok()) return std::nullopt;
    return date;
}

std::string formatDate(Date date) {
    return std::format("{:04}-{:02}-{:02}", static_cast<int>(date.year()), static_cast<unsigned>(date.month()),
                       static_cast<unsigned>(date.day()));
}

std::string formatInstant(Date date) { return formatDate(date) + "T00:00:00Z"; }

std::string formatDayMonthYear(Date date) { return std::format("{} {}", dayMonth(date), static_cast<int>(date.year())); }

Date utcToday() { return Date{floor<days>(system_clock::now())}; }

std::optional<int> localHour(std::string_view ianaZone, sys_seconds at) {
    try {
        const zoned_time local{locate_zone(ianaZone), at};
        const auto t = local.get_local_time();
        return static_cast<int>(hh_mm_ss{t - floor<days>(t)}.hours().count());
    } catch (const std::runtime_error&) {
        return std::nullopt;
    }
}

Date leasePeriodStart(Date leasedOn, int period) {
    const year_month ym = year_month{leasedOn.year(), leasedOn.month()} + months{period};
    const day monthEnd = year_month_day_last{ym.year(), month_day_last{ym.month()}}.day();
    return Date{ym.year(), ym.month(), std::min(leasedOn.day(), monthEnd)};
}

std::string formatLeasePeriod(Date start, Date nextStart) {
    const Date end{sys_days{nextStart} - days{1}};
    return dayMonth(start) + " – " + dayMonth(end);
}

}  // namespace blocktime
