#ifndef QUIVER_BLOB_UTILS_H
#define QUIVER_BLOB_UTILS_H

#include <chrono>

namespace quiver {

namespace chrono = std::chrono;

inline int day_of_year(chrono::system_clock::time_point datetime) {
    auto day = chrono::floor<chrono::days>(datetime);
    auto ymd = chrono::year_month_day{day};
    auto jan1 = chrono::sys_days{ymd.year() / chrono::January / chrono::day{1}};
    return (day - jan1).count() + 1;
}

}  // namespace quiver



#endif  // QUIVER_BLOB_UTILS_H