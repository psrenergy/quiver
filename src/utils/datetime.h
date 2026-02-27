#ifndef QUIVER_DATETIME_H
#define QUIVER_DATETIME_H

#include <string>

namespace quiver::datetime {

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