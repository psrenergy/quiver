#ifndef QUIVER_DATETIME_H
#define QUIVER_DATETIME_H

#include "utils/string.h"

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>

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

// The single definition of "valid ISO 8601" in the core: "YYYY-MM-DD", optionally followed by
// "THH:MM:SS" or " HH:MM:SS", matched against the shape mask below - 'd' is a digit, 'T' is 'T' or
// a space, everything else is a literal. The length check is what forbids a partial or trailing
// anything, so "2005", "2024-01-15T10:30", "+2024-01-15" and "2024-01-01xyz" never reach the mask.
// Hand-rolled rather than std::get_time, whose field widths are maxima; rationale in src/CLAUDE.md.
inline bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    static constexpr std::string_view kMask = "dddd-dd-ddTdd:dd:dd";
    if (datetime_str.size() != 10 && datetime_str.size() != kMask.size()) {
        return false;
    }
    for (std::size_t i = 0; i < datetime_str.size(); ++i) {
        const char c = datetime_str[i];
        const bool matches = kMask[i] == 'd'   ? (c >= '0' && c <= '9')
                             : kMask[i] == 'T' ? (c == 'T' || c == ' ')
                                               : (c == kMask[i]);
        if (!matches) {
            return false;
        }
    }
    const auto num = [&datetime_str](std::size_t at, std::size_t count) {
        int value = 0;
        for (std::size_t i = at; i < at + count; ++i) {
            value = value * 10 + (datetime_str[i] - '0');
        }
        return value;
    };
    tm = std::tm{};
    tm.tm_year = num(0, 4) - 1900;
    tm.tm_mon = num(5, 2) - 1;
    tm.tm_mday = num(8, 2);
    if (datetime_str.size() > 10) {
        tm.tm_hour = num(11, 2);
        tm.tm_min = num(14, 2);
        tm.tm_sec = num(17, 2);
    }
    // Year 0 is below Python's MINYEAR; ok() covers month/day. 60 seconds would be a leap second:
    // Python and Julia throw on it, Dart silently rolls the clock to the next minute. No agreement,
    // so it is not in the intersection.
    const auto ymd = tm_to_year_month_day(tm);
    if (num(0, 4) < 1 || !ymd.ok() || tm.tm_hour > 23 || tm.tm_min > 59 || tm.tm_sec > 59) {
        return false;
    }
    // strftime reads tm_wday/tm_yday for %a/%A/%j/%U/%W/%w, and nothing else fills them, so
    // format_datetime would otherwise report every date as a Sunday on day 001.
    const auto sys_day = std::chrono::sys_days{ymd};
    tm.tm_wday = static_cast<int>(std::chrono::weekday{sys_day}.c_encoding());
    tm.tm_yday = static_cast<int>((sys_day - std::chrono::sys_days{ymd.year() / std::chrono::January / 1}).count());
    return true;
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