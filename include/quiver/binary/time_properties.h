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
    // Coordinate of the metadata's initial_datetime in this dimension (1 for the outermost time
    // dimension). Stored because traversal (next_dimensions, ExpressionAggregate::compute_row) reads it
    // per cell; computed only by BinaryMetadata::derive_initial_values().
    int64_t initial_value;
    int64_t parent_dimension_index;

    // Start of period number `value`, counting the period of this frequency that holds base_datetime as 1:
    // base_datetime floored to January 1, the 1st of its month, its day (Weekly and Daily) or its hour, plus
    // value - 1 periods. Folded over a file's time dimensions outermost first, starting at initial_datetime,
    // it yields the start of the cell a coordinate names. initial_value plays no part.
    std::chrono::system_clock::time_point add_offset_from_int(std::chrono::system_clock::time_point base_datetime,
                                                              int64_t value) const;
};

}  // namespace quiver

#endif  // QUIVER_TIME_PROPERTIES_H
