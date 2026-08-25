#ifndef QUIVER_DATETIME_H
#define QUIVER_DATETIME_H

#include "utils/string.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <string>

namespace quiver::datetime {

// Build the C++20 calendar date a std::tm denotes. Shared by tm_to_time_point and by
// parse_iso8601's validity check.
inline std::chrono::year_month_day tm_to_year_month_day(const std::tm& tm) {
    return std::chrono::year_month_day{std::chrono::year{tm.tm_year + 1900},
                                       std::chrono::month{static_cast<unsigned>(tm.tm_mon + 1)},
                                       std::chrono::day{static_cast<unsigned>(tm.tm_mday)}};
}

// Convert a std::tm (interpreted as UTC) to a system_clock::time_point.
//
// Built on the C++20 <chrono> calendar, which supports the full proleptic Gregorian range
// (including dates before the Unix epoch) and is locale/timezone-independent.
inline std::chrono::system_clock::time_point tm_to_time_point(const std::tm& tm) {
    auto days = std::chrono::sys_days{tm_to_year_month_day(tm)};
    return std::chrono::system_clock::time_point{days} + std::chrono::hours{tm.tm_hour} +
           std::chrono::minutes{tm.tm_min} + std::chrono::seconds{tm.tm_sec};
}

namespace detail {

// Read exactly `count` ASCII digits at `pos`, advancing it. Fixed width is the whole point:
// std::get_time treats a field width as a *maximum*, so "%Y-%m-%d" also matches "2024-1-5" and
// "24-01-15", and "%H:%M:%S" also matches "T1:30:00" and (on MSVC, which does not set failbit when
// the input ends mid-format) "T10:30". Every one of those is rejected by Python's fromisoformat
// and Dart's DateTime.parse, so get_time cannot express the grammar this parser must enforce.
inline bool read_digits(const std::string& s, std::size_t& pos, std::size_t count, int& out) {
    if (pos + count > s.size()) {
        return false;
    }
    int value = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const char c = s[pos + i];
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + (c - '0');
    }
    pos += count;
    out = value;
    return true;
}

inline bool read_char(const std::string& s, std::size_t& pos, char expected) {
    if (pos >= s.size() || s[pos] != expected) {
        return false;
    }
    ++pos;
    return true;
}

}  // namespace detail

// The single definition of "valid ISO 8601" in the core: "YYYY-MM-DD", optionally followed by
// "THH:MM:SS" or " HH:MM:SS". Every field is fixed-width and zero-padded, the year is 0001-9999,
// the calendar day must exist (2024-02-31 is rejected), a leap second is not accepted, and the
// whole string must be consumed - so "2005", "2005-01", "2024-1-5", "2024-01-15T10:30" and
// "2024-01-01xyz" are all rejected. That band is the intersection of what Python's
// `fromisoformat`, Julia's `DateTime` and Dart's `DateTime.parse` accept; keep it that way, since
// the write-side gate below is what promises a stored value is readable by every binding.
//
// Hand-rolled rather than std::get_time: get_time's widths are maxima (see detail::read_digits),
// it is locale- and platform-sensitive, and a literal space in its format means "skip zero or more
// spaces", which would also accept "2024-01-0110:30:00".
inline bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    tm = std::tm{};
    std::size_t pos = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    if (!detail::read_digits(datetime_str, pos, 4, year) || !detail::read_char(datetime_str, pos, '-') ||
        !detail::read_digits(datetime_str, pos, 2, month) || !detail::read_char(datetime_str, pos, '-') ||
        !detail::read_digits(datetime_str, pos, 2, day)) {
        return false;
    }
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    // year 0 is rejected because Python's datetime starts at year 1; ok() covers month/day.
    const auto ymd = tm_to_year_month_day(tm);
    if (year < 1 || !ymd.ok()) {
        return false;
    }
    // strftime reads tm_wday/tm_yday for %a/%A/%j/%U/%W/%w, and nothing else fills them, so
    // format_datetime would otherwise report every date as a Sunday on day 001.
    const auto sys_day = std::chrono::sys_days{ymd};
    tm.tm_wday = static_cast<int>(std::chrono::weekday{sys_day}.c_encoding());
    tm.tm_yday = static_cast<int>((sys_day - std::chrono::sys_days{ymd.year() / std::chrono::January / 1}).count());

    if (pos == datetime_str.size()) {
        return true;  // date only: "2024-01-01"
    }
    const char separator = datetime_str[pos++];
    if (separator != 'T' && separator != ' ') {
        return false;
    }
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (!detail::read_digits(datetime_str, pos, 2, hour) || !detail::read_char(datetime_str, pos, ':') ||
        !detail::read_digits(datetime_str, pos, 2, minute) || !detail::read_char(datetime_str, pos, ':') ||
        !detail::read_digits(datetime_str, pos, 2, second)) {
        return false;
    }
    // 60 would be a leap second: Python and Julia throw on it, Dart silently rolls the clock to the
    // next minute. No agreement, so it is not in the intersection.
    if (hour > 23 || minute > 59 || second > 59) {
        return false;
    }
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = second;
    return pos == datetime_str.size();
}

// Write-side gate for DATE_TIME columns: the parsed fields are discarded, only validity matters.
// Shared by TypeValidator::validate_value (scalar + array writes) and validate_time_series_row
// (time-series writes) - the two halves of the one scalar typing policy.
//
// Trims first because Database::execute trims every bound string, so the value that reaches SQLite
// is the trimmed one; validating the raw string would reject " 2024-01-15", which stores fine.
inline bool is_valid_iso8601(const std::string& datetime_str) {
    std::tm tm{};
    return parse_iso8601(quiver::string::trim(datetime_str), tm);
}

// Format a system_clock::time_point as ISO 8601 string in UTC.
//
// Decomposes via the C++20 <chrono> calendar, which supports the full proleptic Gregorian
// range (including dates before the Unix epoch) and is locale/timezone-independent.
inline std::string format_utc(const std::chrono::system_clock::time_point& tp) {
    auto days = std::chrono::floor<std::chrono::days>(tp);
    std::chrono::year_month_day ymd{days};
    std::chrono::hh_mm_ss<std::chrono::seconds> hms{std::chrono::duration_cast<std::chrono::seconds>(tp - days)};

    char buffer[32];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%04d-%02u-%02uT%02d:%02d:%02d",
                  static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()),
                  static_cast<unsigned>(ymd.day()),
                  static_cast<int>(hms.hours().count()),
                  static_cast<int>(hms.minutes().count()),
                  static_cast<int>(hms.seconds().count()));
    return std::string(buffer);
}

// Format a datetime value using strftime. Returns raw_value if parsing fails.
inline std::string format_datetime(const std::string& raw_value, const std::string& format) {
    std::tm tm{};
    if (!parse_iso8601(raw_value, tm)) {
        return raw_value;
    }
    char buffer[256];
    if (std::strftime(buffer, sizeof(buffer), format.c_str(), &tm) == 0) {
        return raw_value;
    }
    return std::string(buffer);
}

}  // namespace quiver::datetime

#endif  // QUIVER_DATETIME_H