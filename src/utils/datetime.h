#ifndef QUIVER_DATETIME_H
#define QUIVER_DATETIME_H

#include <chrono>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>

namespace quiver::datetime {

// Portable UTC mktime: converts a std::tm (interpreted as UTC) to time_t.
// Uses _mkgmtime on Windows (MSVC and MinGW), timegm on POSIX.
inline std::time_t mkgmtime_portable(std::tm* tm) {
#ifdef _WIN32
    return _mkgmtime(tm);
#else
    return timegm(tm);
#endif
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
// Uses gmtime to avoid locale/timezone-dependent std::format behavior.
inline std::string format_utc(const std::chrono::system_clock::time_point& tp) {
    auto time_t_val = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &time_t_val);
#else
    gmtime_r(&time_t_val, &tm);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &tm);
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