#include "quiver/binary/time_properties.h"

#include <stdexcept>

namespace quiver {

std::string frequency_to_string(TimeFrequency frequency) {
    switch (frequency) {
    case TimeFrequency::Yearly:
        return "yearly";
    case TimeFrequency::Monthly:
        return "monthly";
    case TimeFrequency::Weekly:
        return "weekly";
    case TimeFrequency::Daily:
        return "daily";
    case TimeFrequency::Hourly:
        return "hourly";
    }
}

TimeFrequency frequency_from_string(const std::string& str) {
    if (str == "yearly") {
        return TimeFrequency::Yearly;
    }
    if (str == "monthly") {
        return TimeFrequency::Monthly;
    }
    if (str == "weekly") {
        return TimeFrequency::Weekly;
    }
    if (str == "daily") {
        return TimeFrequency::Daily;
    }
    if (str == "hourly") {
        return TimeFrequency::Hourly;
    }
    throw std::invalid_argument("Unknown frequency: " + str);
}

std::chrono::system_clock::time_point TimeProperties::add_offset_from_int(
    std::chrono::system_clock::time_point base_datetime,
    int64_t value
) const {
    // Flooring first is what keeps calendar steps exact: an inner dimension's base is already the start of
    // its parent's period (a 1st at midnight), so January 31 + one month can never become March 3.
    auto date = std::chrono::floor<std::chrono::days>(base_datetime);
    auto ymd = std::chrono::year_month_day{date};
    int64_t steps = value - 1;
    switch (frequency) {
    case TimeFrequency::Yearly:
        return std::chrono::sys_days{(ymd.year() + std::chrono::years{steps}) / std::chrono::January / 1};
    case TimeFrequency::Monthly:
        return std::chrono::sys_days{(ymd.year() / ymd.month() + std::chrono::months{steps}) / 1};
    case TimeFrequency::Weekly:
        return date + std::chrono::weeks{steps};
    case TimeFrequency::Daily:
        return date + std::chrono::days{steps};
    case TimeFrequency::Hourly:
        return std::chrono::floor<std::chrono::hours>(base_datetime) + std::chrono::hours{steps};
    }
}

}  // namespace quiver
