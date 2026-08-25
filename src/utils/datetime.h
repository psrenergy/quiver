#ifndef QUIVER_DATETIME_H
#define QUIVER_DATETIME_H

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
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

// The single definition of "valid ISO 8601" in the core: "YYYY-MM-DD", optionally followed by
// "THH:MM:SS" or " HH:MM:SS". Cross-platform via std::get_time (not strptime).
//
// The whole string must be consumed, so "2005", "2005-01" and "2024-01-01xyz" are all rejected.
// Date and time are two separate get_time passes with the separator matched by hand: a literal
// space in a get_time format means "skip zero or more spaces", so a single "%Y-%m-%d %H:%M:%S"
// pass would also accept "2024-01-0110:30:00".
inline bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream ss(datetime_str);
    // libstdc++'s get_time skips leading whitespace, MSVC's does not. Unset skipws so they agree.
    ss.unsetf(std::ios_base::skipws);

    ss >> std::get_time(&tm, "%Y-%m-%d");
    // get_time range-checks %m (1-12) and %d (1-31) and sets failbit when the input ends
    // mid-format; tm_mday guards a laxer implementation, and the calendar rejects the month/day
    // pairs get_time cannot see on its own (2024-02-31).
    if (ss.fail() || tm.tm_mday < 1 || !tm_to_year_month_day(tm).ok()) {
        return false;
    }
    if (ss.peek() == std::char_traits<char>::eof()) {
        return true;  // date only: "2024-01-01"
    }
    const auto separator = ss.get();
    if (separator != 'T' && separator != ' ') {
        return false;
    }
    ss >> std::get_time(&tm, "%H:%M:%S");  // no memset: the date fields must survive
    return !ss.fail() && ss.peek() == std::char_traits<char>::eof();
}

// Write-side gate for DATE_TIME columns: the parsed fields are discarded, only validity matters.
// Shared by TypeValidator::validate_value (scalar + array writes) and validate_time_series_row
// (time-series writes) - the two halves of the one scalar typing policy.
inline bool is_valid_iso8601(const std::string& datetime_str) {
    std::tm tm{};
    return parse_iso8601(datetime_str, tm);
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