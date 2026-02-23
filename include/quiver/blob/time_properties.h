#ifndef QUIVER_TIME_PROPERTIES_H
#define QUIVER_TIME_PROPERTIES_H

#include "export.h"

#include <cstdint>
#include <ctime>
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
    std::unique_ptr<Dimension> parent_dimension;

    // Setters
    void set_initial_value(int64_t initial_value);
    void set_parent_dimension(std::unique_ptr<Dimension> parent);

    int64_t datetime_to_int(std::time_t datetime) const;
    std::time_t int_to_datetime(int64_t value) const;
};

}  // namespace quiver

#endif  // QUIVER_TIME_PROPERTIES_H
