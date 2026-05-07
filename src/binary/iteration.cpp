#include "quiver/binary/iteration.h"

#include "quiver/binary/dimension.h"
#include "quiver/binary/time_constants.h"
#include "quiver/binary/time_properties.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quiver::binary {

namespace {

// Lifted from src/binary/binary_file.cpp::dimension_sizes_at_values (the protected
// member). The only substitution: the original impl reads `impl_->metadata.dimensions`;
// this version takes `const BinaryMetadata& metadata` directly. Calendar-aware
// variable-size handling for time dimensions whose parent is monthly/yearly.
std::vector<int64_t> dimension_sizes_at_values(const BinaryMetadata& metadata,
                                               const std::vector<int64_t>& dimension_values) {
    using namespace quiver::time;
    const auto& dimensions = metadata.dimensions;

    std::vector<int64_t> sizes;
    sizes.reserve(dimensions.size());
    for (const auto& dim : dimensions) {
        sizes.push_back(dim.size);
    }

    auto datetime = metadata.initial_datetime;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (!dimensions[i].is_time_dimension())
            continue;
        datetime = dimensions[i].time->add_offset_from_int(datetime, dimension_values[i]);
    }
    const auto date = std::chrono::floor<std::chrono::days>(datetime);
    const auto ymd = std::chrono::year_month_day{date};

    for (size_t i = 0; i < dimensions.size(); ++i) {
        const auto& dim = dimensions[i];
        if (!dim.is_time_dimension() || dim.time->parent_dimension_index == -1)
            continue;

        const auto& parent = dimensions[dim.time->parent_dimension_index];
        TimeFrequency freq = dim.time->frequency;
        TimeFrequency parent_freq = parent.time->frequency;

        // Yearly and weekly frequencies must always be at index 1, so they are not considered in this loop
        switch (freq) {
        case TimeFrequency::Hourly:
            switch (parent_freq) {
            case TimeFrequency::Daily:
                break;  // Number of hours in a day is always the same
            case TimeFrequency::Weekly:
                break;  // Number of hours in a week is always the same
            case TimeFrequency::Monthly:
                sizes[i] =
                    static_cast<unsigned>((ymd.year() / ymd.month() / std::chrono::last).day()) * MAX_HOURS_IN_DAY;
                break;
            case TimeFrequency::Yearly:
                sizes[i] = (ymd.year().is_leap() ? 366 : 365) * MAX_HOURS_IN_DAY;
                break;
            default:
                break;
            }
            break;
        case TimeFrequency::Daily:
            switch (parent_freq) {
            case TimeFrequency::Weekly:
                break;  // Number of days in a week is always the same
            case TimeFrequency::Monthly:
                sizes[i] = static_cast<unsigned>((ymd.year() / ymd.month() / std::chrono::last).day());
                break;
            case TimeFrequency::Yearly:
                sizes[i] = ymd.year().is_leap() ? 366 : 365;
                break;
            default:
                break;
            }
            break;
        case TimeFrequency::Monthly:
            break;  // Number of months in a year is always the same
        default:
            break;
        }
    }

    return sizes;
}

}  // namespace

std::vector<int64_t> first_dimensions(const BinaryMetadata& meta) {
    std::vector<int64_t> result;
    result.reserve(meta.dimensions.size());
    for (const auto& dim : meta.dimensions) {
        result.push_back(dim.is_time_dimension() ? dim.time->initial_value : 1);
    }
    return result;
}

std::optional<std::vector<int64_t>> next_dimensions(const BinaryMetadata& meta, const std::vector<int64_t>& current) {
    const auto& dimensions = meta.dimensions;
    const auto current_sizes = dimension_sizes_at_values(meta, current);

    std::vector<int64_t> next = current;

    // WR-09 (D-22): track whether the increment loop broke. If it wrapped through every
    // dim without breaking, we've exhausted the position space -- equivalent to the old
    // post-loop "next == first_dimensions(meta)" check, but without rebuilding
    // first_dimensions per call (~7.3M wasted vector allocations on a 480x500x31 sweep).
    bool incremented = false;
    for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i) {
        if (next[i] < current_sizes[i]) {
            next[i] += 1;
            incremented = true;
            break;
        } else {
            next[i] = 1;
        }
    }

    // End-of-iteration: if the increment loop wrapped through every dimension without
    // breaking, we've exhausted the position space.
    if (!incremented)
        return std::nullopt;

    // Adjust time dimensions which were reset to 1 before their parent dimension is incremented.
    // Ex: [month, scenario, day] when initial date is 2025-01-02
    // [1, 1, 31] -> [1, 2, 1] is incorrect, should be [1, 2, 2]
    for (size_t i = 0; i < next.size(); ++i) {
        const auto& dim = dimensions[i];
        if (!dim.is_time_dimension())
            continue;
        int64_t initial_value = dim.time->initial_value;
        int64_t parent_idx = dim.time->parent_dimension_index;  // -1 = no parent
        if (next[i] < initial_value && parent_idx != -1 &&
            next[parent_idx] == dimensions[parent_idx].time->initial_value) {
            next[i] = initial_value;
        }
    }

    return next;
}

}  // namespace quiver::binary
