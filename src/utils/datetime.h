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

// Portable gmtime: converts time_t to std::tm in UTC.
inline std::tm gmtime_portable(std::time_t t) {
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    return tm;
}

// Format a system_clock::time_point as "YYYY-MM-DDTHH:MM:SS" in UTC.
inline std::string format_utc_datetime(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto tm = gmtime_portable(t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

// Format a system_clock::time_point as "YYYY-MM-DD" in UTC.
inline std::string format_utc_date(std::chrono::system_clock::time_point tp) {
    auto t = std::chrono::system_clock::to_time_t(tp);
    auto tm = gmtime_portable(t);
    char buf[16];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d", &tm);
    return buf;
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