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

// Convert a std::tm (interpreted as UTC) to a system_clock::time_point.
//
// Built on the C++20 <chrono> calendar, which supports the full proleptic Gregorian range
// (including dates before the Unix epoch) and is locale/timezone-independent.
inline std::chrono::system_clock::time_point tm_to_time_point(const std::tm& tm) {
    std::chrono::year_month_day ymd{std::chrono::year{tm.tm_year + 1900},
                                    std::chrono::month{static_cast<unsigned>(tm.tm_mon + 1)},
                                    std::chrono::day{static_cast<unsigned>(tm.tm_mday)}};
    auto days = std::chrono::sys_days{ymd};
    return std::chrono::system_clock::time_point{days} + std::chrono::hours{tm.tm_hour} +
           std::chrono::minutes{tm.tm_min} + std::chrono::seconds{tm.tm_sec};
}

// Cross-platform ISO 8601 parser using std::get_time (not strptime).
// Tries "YYYY-MM-DDTHH:MM:SS" first, then "YYYY-MM-DD HH:MM:SS".
inline bool parse_iso8601(const std::string& datetime_str, std::tm& tm) {
    std::memset(&tm, 0, sizeof(tm));
    std::istringstream ss(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (!ss.fail() && tm.tm_mday >= 1) {
        return true;
    }
    std::memset(&tm, 0, sizeof(tm));
    ss.clear();
    ss.str(datetime_str);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return !ss.fail() && tm.tm_mday >= 1;
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