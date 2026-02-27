#ifndef QUIVER_BLOB_UTILS_H
#define QUIVER_BLOB_UTILS_H

#include "quiver/blob/time_constants.h"

#include <chrono>
#include <cstdint>
#include <string_view>

namespace quiver {

constexpr std::string_view QVR_EXTENSION = ".qvr";
constexpr std::string_view TOML_EXTENSION = ".toml";
constexpr std::string_view CSV_EXTENSION = ".csv";

namespace chrono = std::chrono;

inline int64_t day_of_year(chrono::system_clock::time_point datetime) {
    auto day = chrono::floor<chrono::days>(datetime);
    auto ymd = chrono::year_month_day{day};
    auto jan1 = chrono::sys_days{ymd.year() / chrono::January / chrono::day{1}};
    return (day - jan1).count() + 1;
}

inline int64_t day_of_week(chrono::system_clock::time_point datetime) {
    int day_of_year = quiver::day_of_year(datetime);
    return (day_of_year - 1) % quiver::time::MAX_DAYS_IN_WEEK + 1;  // 1-7 instead of 0-6
}

}  // namespace quiver

#endif  // QUIVER_BLOB_UTILS_H