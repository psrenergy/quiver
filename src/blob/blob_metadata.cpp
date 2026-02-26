#include "quiver/blob/blob_metadata.h"

#include "quiver/blob/dimension.h"
#include "quiver/blob/time_properties.h"
#include "quiver/blob/time_constants.h"
#include "blob_utils.h"

#include <algorithm>
#include <format>
#include <sstream>
#include <toml++/toml.hpp>
#include <stdexcept>

namespace {

constexpr std::string_view QUIVER_FILE_VERSION = "1";

std::vector<int64_t> compute_time_dimension_initial_values(const std::vector<quiver::Dimension>& dimensions, const std::chrono::system_clock::time_point& initial_datetime) {
    std::vector<int64_t> initial_values;
    initial_values.push_back(1); // The largest time dimension always starts at 1

    auto date = std::chrono::floor<std::chrono::days>(initial_datetime);
    auto ymd = std::chrono::year_month_day{date};

    for (const auto& dim : dimensions) {
        if (dim.time->parent_dimension_index != -1) {
            quiver::TimeFrequency current_frequency = dim.time->frequency;
            quiver::TimeFrequency parent_frequency = dimensions[dim.time->parent_dimension_index].time->frequency;

            // Yearly and weekly frequencies must always be at index 1, so they error if encountered as inner time dimensions
            switch(current_frequency) {
                case quiver::TimeFrequency::Yearly:
                    throw std::logic_error("YEARLY frequency not implemented. This function should only be used for inner time dimensions.");
                case quiver::TimeFrequency::Monthly: {
                    int64_t month = static_cast<unsigned>(ymd.month());
                    initial_values.push_back(month);
                    break;
                }
                case quiver::TimeFrequency::Weekly:
                    throw std::logic_error("WEEKLY frequency not implemented. This function should only be used for inner time dimensions.");
                case quiver::TimeFrequency::Daily: {
                    int64_t day;
                    switch(parent_frequency) {
                        case quiver::TimeFrequency::Weekly: {
                            day = quiver::day_of_week(initial_datetime);
                            break;
                        }
                        case quiver::TimeFrequency::Monthly: {
                            day = static_cast<unsigned>(ymd.day());
                            break;
                        }
                        case quiver::TimeFrequency::Yearly: {
                            day = quiver::day_of_year(initial_datetime);
                            break;
                        }
                        default:
                            throw std::logic_error("Invalid parent frequency " + frequency_to_string(parent_frequency) + " for DAILY dimension.");
                    }
                    initial_values.push_back(day);
                    break;
                }
                case quiver::TimeFrequency::Hourly: {
                    auto time_of_day = std::chrono::hh_mm_ss{initial_datetime - date};
                    int64_t hour = time_of_day.hours().count() + 1; // 0-23 -> 1-24
                    switch(parent_frequency) {
                    case quiver::TimeFrequency::Daily:
                        break;
                    case quiver::TimeFrequency::Weekly:
                        hour += (quiver::day_of_week(initial_datetime) - 1) * quiver::time::MAX_HOURS_IN_DAY;
                        break;
                    case quiver::TimeFrequency::Monthly:
                        hour += (static_cast<unsigned>(ymd.day()) - 1) * quiver::time::MAX_HOURS_IN_DAY;
                        break;
                    case quiver::TimeFrequency::Yearly:
                        hour += (quiver::day_of_year(initial_datetime) - 1) * quiver::time::MAX_HOURS_IN_DAY;
                        break;
                    default:
                        throw std::logic_error("Invalid parent frequency " + frequency_to_string(parent_frequency) + " for HOURLY dimension.");
                    }
                    initial_values.push_back(hour);
                    break;
                }
                default:
                    throw std::logic_error("Unhandled frequency " + frequency_to_string(current_frequency) + " in compute_time_dimension_initial_values.");
            }
        }
    }
    return initial_values;
}

}

namespace quiver {

BlobMetadata BlobMetadata::from_toml(const std::string& toml_content) {
    // Parse toml content
    toml::table tbl = toml::parse(toml_content);

    std::vector<std::string> dimensions;
    if (auto* arr = tbl["dimensions"].as_array()) {
        for (auto& elem : *arr) {
            if (auto val = elem.value<std::string>()) {
                dimensions.push_back(*val);
            }
        }
    }

    std::vector<int64_t> dimension_sizes;
    if (auto* arr = tbl["dimension_sizes"].as_array()) {
        for (auto& elem : *arr) {
            if (auto val = elem.value<int64_t>()) {
                dimension_sizes.push_back(*val);
            }
        }
    }

    std::vector<std::string> time_dimensions;
    if (auto* arr = tbl["time_dimensions"].as_array()) {
        for (auto& elem : *arr) {
            if (auto val = elem.value<std::string>()) {
                time_dimensions.push_back(*val);
            }
        }
    }

    std::vector<std::string> frequencies;
    if (auto* arr = tbl["frequencies"].as_array()) {
        for (auto& elem : *arr) {
            if (auto val = elem.value<std::string>()) {
                frequencies.push_back(*val);
            }
        }
    }

    std::string initial_datetime_str = tbl["initial_datetime"].value<std::string>().value();
    std::string unit = tbl["unit"].value<std::string>().value();

    std::vector<std::string> labels;
    if (auto* arr = tbl["labels"].as_array()) {
        for (auto& elem : *arr) {
            if (auto val = elem.value<std::string>()) {
                labels.push_back(*val);
            }
        }
    }

    std::string version = tbl["version"].value<std::string>().value();
    
    // Create and populate BlobMetadata
    BlobMetadata metadata;
    metadata.unit = unit;
    metadata.labels = labels;
    metadata.version = version;

    std::chrono::system_clock::time_point initial_datetime;
    std::istringstream iss{initial_datetime_str};
    iss >> std::chrono::parse("%Y-%m-%dT%H:%M:%S", initial_datetime);
    metadata.initial_datetime = initial_datetime;

    // Add dimensions to metadata
    int64_t time_dim_index = 0;
    int64_t previous_time_dim_index = -1;
    metadata.dimensions.clear();
    for (size_t i = 0; i < dimensions.size(); ++i) {
        bool is_time = std::find(time_dimensions.begin(), time_dimensions.end(), dimensions[i]) != time_dimensions.end();
        if (is_time) {
            TimeFrequency freq = frequency_from_string(frequencies[time_dim_index]);
            TimeProperties time_props{freq, 0, previous_time_dim_index};
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::move(time_props)});
            time_dim_index++;
            previous_time_dim_index = i;
        } else {
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::nullopt});
        }
    }
    metadata.number_of_time_dimensions = time_dim_index;

    // Compute and set initial values for time dimensions
    std::vector<int64_t> initial_values = compute_time_dimension_initial_values(metadata.dimensions, initial_datetime);
    int64_t time_dim_index = 0;
    for (auto& dim : metadata.dimensions) {
        if (dim.is_time_dimension()) {
            dim.time->set_initial_value(initial_values[time_dim_index]);
            time_dim_index++;
        }
    }

    return metadata;
}

std::string BlobMetadata::to_toml() const {
    validate();

    auto dim_names = toml::array{};
    auto dim_sizes = toml::array{};
    auto time_dim_names = toml::array{};
    auto freq_strings = toml::array{};

    for (const auto& dim : dimensions) {
        dim_names.push_back(dim.name);
        dim_sizes.push_back(dim.size);
        if (dim.is_time_dimension()) {
            time_dim_names.push_back(dim.name);
            freq_strings.push_back(frequency_to_string(dim.time->frequency));
        }
    }

    auto label_arr = toml::array{};
    for (const auto& label : labels) {
        label_arr.push_back(label);
    }

    // std::format with chrono requires C++20 + compiler support for <chrono> formatting.
    // If this fails to compile, fall back to: to_time_t + gmtime + put_time.
    std::string datetime_str = std::format("{:%Y-%m-%dT%H:%M:%S}", std::chrono::floor<std::chrono::seconds>(initial_datetime));

    toml::table tbl{
        {"version", version},
        {"dimensions", std::move(dim_names)},
        {"dimension_sizes", std::move(dim_sizes)},
        {"time_dimensions", std::move(time_dim_names)},
        {"frequencies", std::move(freq_strings)},
        {"initial_datetime", datetime_str},
        {"unit", unit},
        {"labels", std::move(label_arr)},
    };

    std::ostringstream oss;
    oss << tbl;
    return oss.str();
}

void BlobMetadata::validate() const {
    // Version check
    if (version != QUIVER_FILE_VERSION) {
        throw std::runtime_error("Incompatible file version: expected " + std::string(QUIVER_FILE_VERSION) + ", got " + version);
    }

    // Dimension count
    if (dimensions.empty()) {
        throw std::runtime_error("Number of dimensions must be positive, got 0");
    }

    // Label count
    if (labels.empty()) {
        throw std::runtime_error("Number of labels must be positive, got 0");
    }

    // Time dimension count must not exceed total dimensions
    if (number_of_time_dimensions < 0 || number_of_time_dimensions > static_cast<int64_t>(dimensions.size())) {
        throw std::runtime_error(
            "Number of time dimensions must be non-negative and <= number of dimensions. "
            "Got number_of_time_dimensions=" + std::to_string(number_of_time_dimensions) +
            ", number_of_dimensions=" + std::to_string(dimensions.size()));
    }

    // Dimension sizes must be positive
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].size <= 0) {
            throw std::runtime_error(
                "Dimension size at index " + std::to_string(i) +
                " must be positive, got " + std::to_string(dimensions[i].size));
        }
    }

    // Dimension name uniqueness
    for (size_t i = 0; i < dimensions.size(); ++i) {
        for (size_t j = i + 1; j < dimensions.size(); ++j) {
            if (dimensions[i].name == dimensions[j].name) {
                throw std::runtime_error("Dimension names must be unique, duplicate: '" + dimensions[i].name + "'");
            }
        }
    }

    // Label uniqueness
    for (size_t i = 0; i < labels.size(); ++i) {
        for (size_t j = i + 1; j < labels.size(); ++j) {
            if (labels[i] == labels[j]) {
                throw std::runtime_error("Label names must be unique, duplicate: '" + labels[i] + "'");
            }
        }
    }

    validate_time_dimension_metadata();
}

void BlobMetadata::validate_time_dimension_metadata() const {
    if (number_of_time_dimensions == 0) return;

    // Collect time dimensions in order
    std::vector<const Dimension*> time_dims;
    for (const auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            time_dims.push_back(&dim);
        }
    }

    // Frequencies must be unique
    for (size_t i = 0; i < time_dims.size(); ++i) {
        for (size_t j = i + 1; j < time_dims.size(); ++j) {
            if (time_dims[i]->time->frequency == time_dims[j]->time->frequency) {
                throw std::runtime_error(
                    "Time dimension frequencies must be unique. Duplicate: " +
                    frequency_to_string(time_dims[i]->time->frequency));
            }
        }
    }

    // Frequencies must be sorted from lowest to highest (Yearly < Monthly < Weekly < Daily < Hourly)
    for (size_t i = 1; i < time_dims.size(); ++i) {
        if (time_dims[i]->time->frequency <= time_dims[i - 1]->time->frequency) {
            throw std::runtime_error(
                "Time dimension frequencies must be ordered from lowest to highest frequency.");
        }
    }

    // If WEEKLY is present it must be the lowest (first) frequency
    for (size_t i = 1; i < time_dims.size(); ++i) {
        if (time_dims[i]->time->frequency == TimeFrequency::Weekly) {
            throw std::runtime_error("If WEEKLY frequency is present, it must be the lowest frequency.");
        }
    }

    validate_time_dimension_sizes();
}

void BlobMetadata::validate_time_dimension_sizes() const {
    using namespace quiver::time;

    // Skip the outermost time dimension (parent_dimension_index == -1) — its size is unconstrained
    for (const auto& dim : dimensions) {
        if (!dim.is_time_dimension() || dim.time->parent_dimension_index == -1) continue;
        const Dimension& parent = dimensions[dim.time->parent_dimension_index];
        TimeFrequency freq = dim.time->frequency;
        TimeFrequency parent_freq = parent.time->frequency;
        int64_t size = dim.size;

        int64_t min_size = 0, max_size = 0;
        bool found = true;
        switch (freq) {
        case TimeFrequency::Hourly:
            switch (parent_freq) {
                case TimeFrequency::Daily:   min_size = MIN_HOURS_IN_DAY;   max_size = MAX_HOURS_IN_DAY;   break;
                case TimeFrequency::Weekly:  min_size = MIN_HOURS_IN_WEEK;  max_size = MAX_HOURS_IN_WEEK;  break;
                case TimeFrequency::Monthly: min_size = MIN_HOURS_IN_MONTH; max_size = MAX_HOURS_IN_MONTH; break;
                case TimeFrequency::Yearly:  min_size = MIN_HOURS_IN_YEAR;  max_size = MAX_HOURS_IN_YEAR;  break;
                default: found = false; break;
            }
            break;
        case TimeFrequency::Daily:
            switch (parent_freq) {
                case TimeFrequency::Weekly:  min_size = MIN_DAYS_IN_WEEK;  max_size = MAX_DAYS_IN_WEEK;  break;
                case TimeFrequency::Monthly: min_size = MIN_DAYS_IN_MONTH; max_size = MAX_DAYS_IN_MONTH; break;
                case TimeFrequency::Yearly:  min_size = MIN_DAYS_IN_YEAR;  max_size = MAX_DAYS_IN_YEAR;  break;
                default: found = false; break;
            }
            break;
        case TimeFrequency::Weekly:
            switch (parent_freq) {
                case TimeFrequency::Yearly: min_size = MIN_WEEKS_IN_YEAR; max_size = MAX_WEEKS_IN_YEAR; break;
                default: found = false; break;
            }
            break;
        case TimeFrequency::Monthly:
            switch (parent_freq) {
                case TimeFrequency::Yearly: min_size = MIN_MONTHS_IN_YEAR; max_size = MAX_MONTHS_IN_YEAR; break;
                default: found = false; break;
            }
            break;
        default:
            found = false;
            break;
        }

        if (!found) {
            throw std::runtime_error(
                "Invalid parent/child frequency combination: " +
                frequency_to_string(freq) + " inside " + frequency_to_string(parent_freq));
        }

        if (size < min_size || size > max_size) {
            throw std::runtime_error(
                "Time dimension '" + dim.name + "' with frequency '" + frequency_to_string(freq) +
                "' has size " + std::to_string(size) +
                " which is out of bounds [" + std::to_string(min_size) + ", " + std::to_string(max_size) +
                "] based on the next lower frequency: '" + frequency_to_string(parent_freq) + "'");
        }
    }
}

void BlobMetadata::add_dimension(const std::string& name, int64_t size) {
    dimensions.push_back({name, size, std::nullopt});
}

void BlobMetadata::add_time_dimension(const std::string& name, int64_t size, const std::string& frequency) {
    TimeFrequency freq_enum = frequency_from_string(frequency);
    TimeProperties time_props{freq_enum, 0, -1};
    dimensions.push_back({name, size, std::move(time_props)});
}

}  // namespace quiver