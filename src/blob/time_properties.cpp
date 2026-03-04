#include "quiver/blob/time_properties.h"

#include "blob_utils.h"
#include "quiver/blob/time_constants.h"

#include <stdexcept>

namespace {}  // anonymous namespace

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
    if (str == "yearly")
        return TimeFrequency::Yearly;
    if (str == "monthly")
        return TimeFrequency::Monthly;
    if (str == "weekly")
        return TimeFrequency::Weekly;
    if (str == "daily")
        return TimeFrequency::Daily;
    if (str == "hourly")
        return TimeFrequency::Hourly;
    throw std::invalid_argument("Unknown frequency: " + str);
}

void TimeProperties::set_initial_value(int64_t initial_value) {
    this->initial_value = initial_value;
}

void TimeProperties::set_parent_dimension_index(int64_t parent_dimension_index) {
    this->parent_dimension_index = parent_dimension_index;
}

int64_t TimeProperties::datetime_to_int(std::chrono::system_clock::time_point datetime) const {
    auto date = std::chrono::floor<std::chrono::days>(datetime);
    auto ymd = std::chrono::year_month_day{date};
    switch (this->frequency) {
    case TimeFrequency::Yearly:
        throw std::invalid_argument("YEARLY frequency extraction not implemented. This function should only be used "
                                    "for inner time dimensions.");
    case TimeFrequency::Monthly:
        return static_cast<unsigned>(ymd.month());  // 1-12
    case TimeFrequency::Weekly:
        throw std::invalid_argument("WEEKLY frequency extraction not implemented. This function should only be used "
                                    "for inner time dimensions.");
    case TimeFrequency::Daily:
        return static_cast<unsigned>(ymd.day());  // 1-31
    case TimeFrequency::Hourly:
        auto time_of_day = std::chrono::hh_mm_ss{datetime - date};
        return time_of_day.hours().count() + 1;  // 0-23 -> 1-24
    }
}

std::chrono::system_clock::time_point
TimeProperties::add_offset_from_int(std::chrono::system_clock::time_point base_datetime, int64_t value) const {
    auto date = std::chrono::floor<std::chrono::days>(base_datetime);
    auto ymd = std::chrono::year_month_day{date};
    int64_t relative_value = value - this->initial_value;
    switch (this->frequency) {
    case TimeFrequency::Yearly:
        return std::chrono::sys_days{ymd + std::chrono::years{relative_value}};
    case TimeFrequency::Monthly:
        return std::chrono::sys_days{ymd + std::chrono::months{relative_value}};
    case TimeFrequency::Weekly:
        return base_datetime + std::chrono::weeks{relative_value};
    case TimeFrequency::Daily:
        return base_datetime + std::chrono::days{relative_value};
    case TimeFrequency::Hourly:
        return base_datetime + std::chrono::hours{relative_value};
    }
}

}  // namespace quiver