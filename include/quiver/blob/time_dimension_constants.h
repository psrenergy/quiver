#ifndef QUIVER_TIME_DIMENSION_CONSTANTS_H
#define QUIVER_TIME_DIMENSION_CONSTANTS_H

namespace quiver::time {
    // Hours
    constexpr int MAX_HOURS_IN_DAY = 24;
    constexpr int MAX_HOURS_IN_WEEK = 168; // 7 * 24
    constexpr int MAX_HOURS_IN_MONTH = 744; // 31 * 24
    constexpr int MAX_HOURS_IN_YEAR = 8784; // 366 * 24

    constexpr int MIN_HOURS_IN_DAY = 24;
    constexpr int MIN_HOURS_IN_WEEK = 168; // 7 * 24
    constexpr int MIN_HOURS_IN_MONTH = 672; // 28 * 24
    constexpr int MIN_HOURS_IN_YEAR = 8760; // 365 * 24

    // Days
    constexpr int MAX_DAYS_IN_WEEK = 7;
    constexpr int MAX_DAYS_IN_MONTH = 31;
    constexpr int MAX_DAYS_IN_YEAR = 366;

    constexpr int MIN_DAYS_IN_WEEK = 7;
    constexpr int MIN_DAYS_IN_MONTH = 28;
    constexpr int MIN_DAYS_IN_YEAR = 365;

    // Weeks
    constexpr int MAX_WEEKS_IN_YEAR = 53;

    constexpr int MIN_WEEKS_IN_YEAR = 52;

    // Months
    constexpr int MAX_MONTHS_IN_YEAR = 12;

    constexpr int MIN_MONTHS_IN_YEAR = 12;
}  // namespace quiver::time

#endif  // QUIVER_TIME_DIMENSION_CONSTANTS_H
