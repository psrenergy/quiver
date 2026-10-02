#ifndef QUIVER_BINARY_UTILS_H
#define QUIVER_BINARY_UTILS_H

#include "quiver/binary/binary_metadata.h"
#include "quiver/binary/time_constants.h"

#include <chrono>
#include <cstddef>
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

// 1-based position of `datetime` inside the parent period of the inner time dimension at `index`: the inverse of
// TimeProperties::add_offset_from_int. validate() admits Monthly only under Yearly, Daily under Yearly, Monthly or
// Weekly, and Hourly under any of the four. A week is seven days counted from the day of
// metadata.initial_datetime, not from January 1: that is the grid add_offset_from_int walks, so it never restarts
// at a year boundary, and every cell starts on or after that day.
inline int64_t position_in_parent(
    const BinaryMetadata& metadata,
    size_t index,
    chrono::system_clock::time_point datetime
) {
    const auto& time_properties = *metadata.dimensions[index].time;
    const auto parent_frequency = metadata.dimensions[time_properties.parent_dimension_index].time->frequency;
    const auto date = chrono::floor<chrono::days>(datetime);
    const auto ymd = chrono::year_month_day{date};

    if (time_properties.frequency == TimeFrequency::Monthly) {
        return static_cast<unsigned>(ymd.month());
    }

    int64_t days_into_parent = 0;  // under a Daily parent the hour is the whole position
    switch (parent_frequency) {
    case TimeFrequency::Yearly:
        days_into_parent = day_of_year(datetime) - 1;
        break;
    case TimeFrequency::Monthly:
        days_into_parent = static_cast<unsigned>(ymd.day()) - 1;
        break;
    case TimeFrequency::Weekly:
        days_into_parent =
            (date - chrono::floor<chrono::days>(metadata.initial_datetime)).count() % quiver::time::MAX_DAYS_IN_WEEK;
        break;
    default:
        break;
    }

    if (time_properties.frequency == TimeFrequency::Daily) {
        return days_into_parent + 1;
    }
    const int64_t hour_of_day = chrono::floor<chrono::hours>(datetime - date).count();
    return days_into_parent * quiver::time::MAX_HOURS_IN_DAY + hour_of_day + 1;
}

}  // namespace quiver

#endif  // QUIVER_BINARY_UTILS_H
