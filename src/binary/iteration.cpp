#include "quiver/binary/iteration.h"

#include "quiver/binary/dimension.h"
#include "quiver/binary/time_constants.h"
#include "quiver/binary/time_properties.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quiver {

namespace {

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

    // Restore initial_value when a child time dim wrapped to 1 but its parent
    // is still at its own initial_value: the cascade reset above forces 1, but
    // a non-1 initial_value means the iteration starts mid-period (e.g. day=5
    // within a month), and that offset must persist across parent rollovers.
    for (size_t i = 0; i < next.size(); ++i) {
        const auto& dim = dimensions[i];
        if (!dim.is_time_dimension())
            continue;
        auto initial_value = dim.time->initial_value;
        auto parent_idx = dim.time->parent_dimension_index;  // -1 = no parent
        if (next[i] < initial_value && parent_idx != -1 &&
            next[parent_idx] == dimensions[parent_idx].time->initial_value) {
            next[i] = initial_value;
        }
    }

    return next;
}

}  // namespace quiver
