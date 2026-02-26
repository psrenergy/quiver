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

}

void BlobMetadata::validate_time_dimension_metadata() const {

}

void BlobMetadata::validate_time_dimension_sizes() const {

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