#include "quiver/binary/iteration.h"

#include "quiver/binary/dimension.h"
#include "quiver/binary/time_constants.h"
#include "quiver/binary/time_properties.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quiver {

std::vector<int64_t>
dimension_sizes_at_values(const BinaryMetadata& metadata, const std::vector<int64_t>& dimension_values) {
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
        auto freq = dim.time->frequency;
        auto parent_freq = parent.time->frequency;

        switch (freq) {
        case TimeFrequency::Hourly:
            switch (parent_freq) {
            case TimeFrequency::Daily:
                break;
            case TimeFrequency::Weekly:
                break;
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
            break;
        default:
            break;
        }
    }

    return sizes;
}

int64_t
dimension_start_at_values(const BinaryMetadata& meta, const std::vector<int64_t>& dimension_values, size_t index) {
    const auto& dim = meta.dimensions[index];
    if (!dim.is_time_dimension())
        return 1;

    // Only the period the file starts in begins at initial_value (day 15 of March 2025 for a
    // 2025-03-15 start), and a coordinate is in that period exactly when every time ancestor, up
    // the parent_dimension_index chain, is at its own initial_value. Checking the parent alone
    // restarted every later March at day 15 as well.
    for (auto ancestor = dim.time->parent_dimension_index; ancestor != -1;
         ancestor = meta.dimensions[ancestor].time->parent_dimension_index) {
        if (dimension_values[ancestor] != meta.dimensions[ancestor].time->initial_value)
            return 1;
    }
    return dim.time->initial_value;
}

std::vector<int64_t> first_dimensions(const BinaryMetadata& meta) {
    std::vector<int64_t> result;
    result.reserve(meta.dimensions.size());
    for (const auto& dim : meta.dimensions) {
        result.push_back(dim.is_time_dimension() ? dim.time->initial_value : 1);
    }
    return result;
}

std::optional<std::vector<int64_t>> next_dimensions(const BinaryMetadata& meta, const std::vector<int64_t>& current) {
    const auto current_sizes = dimension_sizes_at_values(meta, current);

    std::vector<int64_t> next = current;

    auto incremented = false;
    for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i) {
        if (next[i] < current_sizes[i]) {
            next[i] += 1;
            incremented = true;
            break;
        } else {
            next[i] = 1;
        }
    }

    if (!incremented)
        return std::nullopt;

    // The cascade resets every wrapped dimension to 1, but a file that starts mid-period
    // (2025-03-15 -> month 3, day 15) must resume at initial_value wherever the walk re-enters
    // that starting period: e.g. when a non-time outer dimension (a scenario) rolls over, but not a
    // year later. Ascending order matters: an ancestor restored earlier in this pass is the value
    // its descendants compare against.
    for (size_t i = 0; i < next.size(); ++i) {
        next[i] = std::max(next[i], dimension_start_at_values(meta, next, i));
    }

    return next;
}

}  // namespace quiver
