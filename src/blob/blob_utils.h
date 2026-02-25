#ifndef QUIVER_BLOB_UTILS_H
#define QUIVER_BLOB_UTILS_H

#include <chrono>
#include <cstdint>

namespace quiver {

// TODO: if day_of_year and day_of_week end up being the only functions in this file, consider moving them to time_constants.h and renaming it to time_utils.h or similar

namespace chrono = std::chrono;

inline int64_t day_of_year(chrono::system_clock::time_point datetime) {
    auto day = chrono::floor<chrono::days>(datetime);
    auto ymd = chrono::year_month_day{day};
    auto jan1 = chrono::sys_days{ymd.year() / chrono::January / chrono::day{1}};
    return (day - jan1).count() + 1;
}

inline int64_t day_of_week(std::chrono::system_clock::time_point datetime) {
    int day_of_year = quiver::day_of_year(datetime);
    return (day_of_year - 1) % quiver::time::MAX_DAYS_IN_WEEK + 1; // 1-7 instead of 0-6
}

}  // namespace quiver



#endif  // QUIVER_BLOB_UTILS_H