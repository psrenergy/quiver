#include "quiver/binary/binary_metadata.h"

#include "binary_utils.h"
#include "quiver/binary/dimension.h"
#include "quiver/binary/time_constants.h"
#include "quiver/binary/time_properties.h"
#include "utils/datetime.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <toml++/toml.hpp>
#include <type_traits>

namespace {

constexpr std::string_view QUIVER_FILE_VERSION = "1";

}  // namespace

namespace quiver {

namespace {

// Reads a required TOML string, naming `key` when it is absent or not a string.
std::string read_toml_string(const toml::table& tbl, const std::string& key) {
    const toml::node* node = tbl.get(key);
    if (!node) {
        throw std::runtime_error("Cannot from_toml_content: missing key '" + key + "'");
    }
    auto value = node->value<std::string>();
    if (!value) {
        throw std::runtime_error("Cannot from_toml_content: key '" + key + "' must be a string");
    }
    return *value;
}

// Reads a TOML array of T. An absent key reads as empty: build_metadata's count checks and
// validate() then reject a missing required array. A non-array value or an entry of another type
// throws instead of being skipped, which used to shift every later dimension onto the wrong size.
template <typename T>
std::vector<T> read_toml_array(const toml::table& tbl, const std::string& key) {
    const toml::node* node = tbl.get(key);
    if (!node) {
        return {};
    }
    const toml::array* arr = node->as_array();
    if (!arr) {
        throw std::runtime_error("Cannot from_toml_content: key '" + key + "' must be an array");
    }
    std::vector<T> result;
    for (const toml::node& entry : *arr) {
        auto value = entry.value<T>();
        if (!value) {
            constexpr const char* kind = std::is_same_v<T, std::string> ? "strings" : "integers";
            throw std::runtime_error("Cannot from_toml_content: array '" + key + "' must contain " + kind);
        }
        result.push_back(*value);
    }
    return result;
}

// The one body behind from_element and from_toml_content, which differ only in how they read the
// eight fields. `operation` is the public factory the caller used, for the Pattern 1 messages.
BinaryMetadata build_metadata(const std::string& operation,
                              const std::vector<std::string>& dimensions,
                              const std::vector<int64_t>& dimension_sizes,
                              const std::vector<std::string>& time_dimensions,
                              const std::vector<std::string>& frequencies,
                              const std::string& initial_datetime_str,
                              const std::string& unit,
                              const std::vector<std::string>& labels,
                              const std::string& version) {
    // The dimension loop reads dimension_sizes and frequencies in parallel with dimensions and
    // time_dimensions, so their lengths must agree before anything is indexed.
    if (dimension_sizes.size() != dimensions.size()) {
        throw std::runtime_error("Cannot " + operation + ": dimension_sizes count (" +
                                 std::to_string(dimension_sizes.size()) + ") does not match dimensions count (" +
                                 std::to_string(dimensions.size()) + ")");
    }
    if (frequencies.size() != time_dimensions.size()) {
        throw std::runtime_error("Cannot " + operation + ": frequencies count (" + std::to_string(frequencies.size()) +
                                 ") does not match time_dimensions count (" + std::to_string(time_dimensions.size()) +
                                 ")");
    }

    // Validate time_dimensions are a subset of dimensions
    for (const auto& td : time_dimensions) {
        if (std::find(dimensions.begin(), dimensions.end(), td) == dimensions.end()) {
            throw std::runtime_error("Cannot " + operation + ": time dimension '" + td + "' is not in dimensions");
        }
    }

    // Validate time_dimensions are in the same order as dimensions
    size_t last_pos = 0;
    for (const auto& td : time_dimensions) {
        auto it = std::find(dimensions.begin() + last_pos, dimensions.end(), td);
        if (it == dimensions.end()) {
            throw std::runtime_error("Cannot " + operation +
                                     ": time dimensions must appear in the same order as dimensions");
        }
        last_pos = static_cast<size_t>(std::distance(dimensions.begin(), it)) + 1;
    }

    // Create and populate BinaryMetadata
    BinaryMetadata metadata;
    metadata.unit = unit;
    metadata.labels = labels;
    metadata.version = version;

    std::tm tm{};
    if (!quiver::datetime::parse_iso8601(initial_datetime_str, tm)) {
        throw std::runtime_error("Failed to parse initial_datetime: " + initial_datetime_str);
    }
    metadata.initial_datetime = quiver::datetime::tm_to_time_point(tm);

    // Add dimensions to metadata
    int64_t previous_time_dim_index = -1;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        auto time_it = std::find(time_dimensions.begin(), time_dimensions.end(), dimensions[i]);
        if (time_it != time_dimensions.end()) {
            // The frequency at the matched position, not a running count: a repeated dimension name
            // matches twice, and a count would read past `frequencies` before validate() rejects it.
            auto time_pos = static_cast<size_t>(std::distance(time_dimensions.begin(), time_it));
            TimeFrequency freq = frequency_from_string(frequencies[time_pos]);
            TimeProperties time_props{freq, 0, previous_time_dim_index};  // initial_value: derive_initial_values()
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::move(time_props)});
            previous_time_dim_index = i;
        } else {
            metadata.dimensions.push_back({dimensions[i], dimension_sizes[i], std::nullopt});
        }
    }

    // Validate first: an initial value is a position inside the parent's period, which exists only for the
    // parent/child layouts validate() accepts
    metadata.validate();
    metadata.derive_initial_values();
    return metadata;
}

}  // namespace

BinaryMetadata::BinaryMetadata() = default;
BinaryMetadata::~BinaryMetadata() = default;

int64_t BinaryMetadata::number_of_time_dimensions() const {
    int64_t count = 0;
    for (const auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            ++count;
        }
    }
    return count;
}

// Every time dimension starts at the cell holding initial_datetime: the outermost at 1, each inner one at the
// position of initial_datetime inside its parent's period.
void BinaryMetadata::derive_initial_values() {
    for (size_t i = 0; i < dimensions.size(); ++i) {
        auto& time_properties = dimensions[i].time;
        if (!time_properties) {
            continue;
        }
        time_properties->initial_value =
            time_properties->parent_dimension_index == -1 ? 1 : position_in_parent(*this, i, initial_datetime);
    }
}

BinaryMetadata BinaryMetadata::from_element(const Element& element) {
    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    // Helper: Extract scalar fields
    auto get_string = [&](const std::string& name) -> std::string {
        auto it = scalars.find(name);
        if (it == scalars.end()) {
            throw std::runtime_error("Cannot from_element: missing scalar '" + name + "'");
        }
        if (!std::holds_alternative<std::string>(it->second)) {
            throw std::runtime_error("Cannot from_element: scalar '" + name + "' must be a string");
        }
        return std::get<std::string>(it->second);
    };

    // Helper: Extract array fields as strings
    auto get_string_array = [&](const std::string& name) -> std::vector<std::string> {
        auto it = arrays.find(name);
        if (it == arrays.end()) {
            throw std::runtime_error("Cannot from_element: missing array '" + name + "'");
        }
        std::vector<std::string> result;
        for (const auto& v : it->second) {
            if (!std::holds_alternative<std::string>(v)) {
                throw std::runtime_error("Cannot from_element: array '" + name + "' must contain strings");
            }
            result.push_back(std::get<std::string>(v));
        }
        return result;
    };

    // Helper: Extract array fields as int64_t
    auto get_int_array = [&](const std::string& name) -> std::vector<int64_t> {
        auto it = arrays.find(name);
        if (it == arrays.end()) {
            throw std::runtime_error("Cannot from_element: missing array '" + name + "'");
        }
        std::vector<int64_t> result;
        for (const auto& v : it->second) {
            if (!std::holds_alternative<int64_t>(v)) {
                throw std::runtime_error("Cannot from_element: array '" + name + "' must contain integers");
            }
            result.push_back(std::get<int64_t>(v));
        }
        return result;
    };

    // Helper: Optional array (returns empty if missing)
    auto get_string_array_opt = [&](const std::string& name) -> std::vector<std::string> {
        auto it = arrays.find(name);
        if (it == arrays.end()) {
            return {};
        }
        std::vector<std::string> result;
        for (const auto& v : it->second) {
            if (!std::holds_alternative<std::string>(v)) {
                throw std::runtime_error("Cannot from_element: array '" + name + "' must contain strings");
            }
            result.push_back(std::get<std::string>(v));
        }
        return result;
    };

    std::vector<std::string> dimensions = get_string_array("dimensions");
    std::vector<int64_t> dimension_sizes = get_int_array("dimension_sizes");
    std::vector<std::string> time_dimensions = get_string_array_opt("time_dimensions");
    std::vector<std::string> frequencies = get_string_array_opt("frequencies");
    std::string initial_datetime_str = get_string("initial_datetime");
    std::string unit = get_string("unit");
    std::vector<std::string> labels = get_string_array("labels");
    std::string version = get_string("version");

    return build_metadata("from_element",
                          dimensions,
                          dimension_sizes,
                          time_dimensions,
                          frequencies,
                          initial_datetime_str,
                          unit,
                          labels,
                          version);
}

BinaryMetadata BinaryMetadata::from_toml_file(const std::string& file_path) {
    const auto toml_path = file_path + std::string(quiver::TOML_EXTENSION);
    if (!std::filesystem::exists(toml_path)) {
        throw std::runtime_error("Metadata file not found: " + toml_path);
    }
    std::ifstream toml_file(toml_path);
    std::string toml_content((std::istreambuf_iterator<char>(toml_file)), std::istreambuf_iterator<char>());
    return from_toml_content(toml_content);
}

BinaryMetadata BinaryMetadata::from_toml_content(const std::string& content) {
    toml::table tbl = toml::parse(content);

    std::vector<std::string> dimensions = read_toml_array<std::string>(tbl, "dimensions");
    std::vector<int64_t> dimension_sizes = read_toml_array<int64_t>(tbl, "dimension_sizes");
    std::vector<std::string> time_dimensions = read_toml_array<std::string>(tbl, "time_dimensions");
    std::vector<std::string> frequencies = read_toml_array<std::string>(tbl, "frequencies");
    std::string initial_datetime_str = read_toml_string(tbl, "initial_datetime");
    std::string unit = read_toml_string(tbl, "unit");
    std::vector<std::string> labels = read_toml_array<std::string>(tbl, "labels");
    std::string version = read_toml_string(tbl, "version");

    return build_metadata("from_toml_content",
                          dimensions,
                          dimension_sizes,
                          time_dimensions,
                          frequencies,
                          initial_datetime_str,
                          unit,
                          labels,
                          version);
}

std::string BinaryMetadata::to_toml() const {
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

    std::string datetime_str = quiver::datetime::format_utc(initial_datetime);

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

void BinaryMetadata::validate() const {
    // Version check
    if (version != QUIVER_FILE_VERSION) {
        throw std::runtime_error("Incompatible file version: expected " + std::string(QUIVER_FILE_VERSION) + ", got " +
                                 version);
    }

    // Dimension count
    if (dimensions.empty()) {
        throw std::runtime_error("Number of dimensions must be positive, got 0");
    }

    // Label count
    if (labels.empty()) {
        throw std::runtime_error("Number of labels must be positive, got 0");
    }

    // Dimension sizes must be positive
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].size <= 0) {
            throw std::runtime_error("Dimension size at index " + std::to_string(i) + " must be positive, got " +
                                     std::to_string(dimensions[i].size));
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

void BinaryMetadata::validate_time_dimension_metadata() const {
    if (number_of_time_dimensions() == 0)
        return;

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
                throw std::runtime_error("Time dimension frequencies must be unique. Duplicate: " +
                                         frequency_to_string(time_dims[i]->time->frequency));
            }
        }
    }

    // Frequencies must be sorted from lowest to highest (Yearly < Monthly < Weekly < Daily < Hourly)
    for (size_t i = 1; i < time_dims.size(); ++i) {
        if (time_dims[i]->time->frequency <= time_dims[i - 1]->time->frequency) {
            throw std::runtime_error("Time dimension frequencies must be ordered from lowest to highest frequency.");
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

void BinaryMetadata::validate_time_dimension_sizes() const {
    using namespace quiver::time;

    // Skip the outermost time dimension (parent_dimension_index == -1) — its size is unconstrained
    for (const auto& dim : dimensions) {
        if (!dim.is_time_dimension() || dim.time->parent_dimension_index == -1)
            continue;
        const Dimension& parent = dimensions[dim.time->parent_dimension_index];
        TimeFrequency freq = dim.time->frequency;
        TimeFrequency parent_freq = parent.time->frequency;
        int64_t size = dim.size;

        int64_t min_size = 0, max_size = 0;
        bool found = true;
        switch (freq) {
        case TimeFrequency::Hourly:
            switch (parent_freq) {
            case TimeFrequency::Daily:
                min_size = MIN_HOURS_IN_DAY;
                max_size = MAX_HOURS_IN_DAY;
                break;
            case TimeFrequency::Weekly:
                min_size = MIN_HOURS_IN_WEEK;
                max_size = MAX_HOURS_IN_WEEK;
                break;
            case TimeFrequency::Monthly:
                min_size = MIN_HOURS_IN_MONTH;
                max_size = MAX_HOURS_IN_MONTH;
                break;
            case TimeFrequency::Yearly:
                min_size = MIN_HOURS_IN_YEAR;
                max_size = MAX_HOURS_IN_YEAR;
                break;
            default:
                found = false;
                break;
            }
            break;
        case TimeFrequency::Daily:
            switch (parent_freq) {
            case TimeFrequency::Weekly:
                min_size = MIN_DAYS_IN_WEEK;
                max_size = MAX_DAYS_IN_WEEK;
                break;
            case TimeFrequency::Monthly:
                min_size = MIN_DAYS_IN_MONTH;
                max_size = MAX_DAYS_IN_MONTH;
                break;
            case TimeFrequency::Yearly:
                min_size = MIN_DAYS_IN_YEAR;
                max_size = MAX_DAYS_IN_YEAR;
                break;
            default:
                found = false;
                break;
            }
            break;
        case TimeFrequency::Weekly:
            switch (parent_freq) {
            case TimeFrequency::Yearly:
                min_size = MIN_WEEKS_IN_YEAR;
                max_size = MAX_WEEKS_IN_YEAR;
                break;
            default:
                found = false;
                break;
            }
            break;
        case TimeFrequency::Monthly:
            switch (parent_freq) {
            case TimeFrequency::Yearly:
                min_size = MIN_MONTHS_IN_YEAR;
                max_size = MAX_MONTHS_IN_YEAR;
                break;
            default:
                found = false;
                break;
            }
            break;
        default:
            found = false;
            break;
        }

        if (!found) {
            throw std::runtime_error("Invalid parent/child frequency combination: " + frequency_to_string(freq) +
                                     " inside " + frequency_to_string(parent_freq));
        }

        if (size < min_size || size > max_size) {
            throw std::runtime_error("Time dimension '" + dim.name + "' with frequency '" + frequency_to_string(freq) +
                                     "' has size " + std::to_string(size) + " which is out of bounds [" +
                                     std::to_string(min_size) + ", " + std::to_string(max_size) +
                                     "] based on the next lower frequency: '" + frequency_to_string(parent_freq) + "'");
        }
    }
}

void BinaryMetadata::add_dimension(const std::string& name, int64_t size) {
    dimensions.push_back({name, size, std::nullopt});
}

void BinaryMetadata::add_time_dimension(const std::string& name, int64_t size, const std::string& frequency) {
    TimeFrequency freq_enum = frequency_from_string(frequency);
    // Chain to the previous time dimension, matching from_toml_content/from_element
    int64_t parent_index = -1;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (dimensions[i].is_time_dimension()) {
            parent_index = static_cast<int64_t>(i);
        }
    }
    TimeProperties time_props{freq_enum, 0, parent_index};
    dimensions.push_back({name, size, std::move(time_props)});
}

}  // namespace quiver