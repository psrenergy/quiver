#ifndef QUIVER_TIME_PROPERTIES_H
#define QUIVER_TIME_PROPERTIES_H

#include "../export.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace quiver {

struct Dimension;

enum class TimeFrequency { Yearly, Monthly, Weekly, Daily, Hourly };

QUIVER_API std::string frequency_to_string(TimeFrequency frequency);
QUIVER_API TimeFrequency frequency_from_string(const std::string& str);

struct QUIVER_API TimeProperties {
    TimeFrequency frequency;
    int64_t initial_value;
    int64_t parent_dimension_index;

    // Setters
    void set_initial_value(int64_t initial_value);
    void set_parent_dimension_index(int64_t parent_dimension_index);

    int64_t datetime_to_int(std::chrono::system_clock::time_point datetime) const;
    std::chrono::system_clock::time_point add_offset_from_int(std::chrono::system_clock::time_point base_datetime,
                                                              int64_t value) const;
};

}  // namespace quiver

#endif  // QUIVER_TIME_PROPERTIES_H
