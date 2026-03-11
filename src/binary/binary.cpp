#include "quiver/binary/binary.h"

#include "binary_utils.h"
#include "quiver/binary/dimension.h"
#include "quiver/binary/time_constants.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace quiver {

struct Binary::Impl {
    std::unique_ptr<std::iostream> io;
    std::string file_path;
    BinaryMetadata metadata;
    int64_t current_position = -1;  // tracked file position to skip redundant seeks
};

Binary::Binary(const std::string& file_path, const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io)
    : impl_(std::make_unique<Impl>(Impl{std::move(io), file_path, metadata})) {}

Binary::~Binary() {
    if (impl_ && impl_->io) {
        impl_->io->flush();
    }
}

Binary::Binary(Binary&& other) noexcept = default;
Binary& Binary::operator=(Binary&& other) noexcept = default;

Binary Binary::open_file(const std::string& file_path, char mode, const std::optional<BinaryMetadata>& metadata) {
    namespace fs = std::filesystem;
    switch (mode) {
    case 'r': {
        // Validate file exists
        if (!fs::exists(file_path + std::string(QVR_EXTENSION)) ||
            !fs::exists(file_path + std::string(TOML_EXTENSION))) {
            throw std::invalid_argument("File not found: " + file_path);
        }

        // Read TOML metadata
        std::ifstream toml_file(file_path + std::string(TOML_EXTENSION));
        std::string toml_content((std::istreambuf_iterator<char>(toml_file)), std::istreambuf_iterator<char>());
        auto metadata_from_toml = BinaryMetadata::from_toml(toml_content);

        // Open binary data file
        auto io =
            std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::in | std::ios::binary);
        return Binary(file_path, metadata_from_toml, std::move(io));
    }
    case 'w': {
        // Validate metadata provided
        if (!metadata.has_value()) {
            throw std::invalid_argument("Metadata must be provided when opening a file in write mode.");
        }

        // Write metadata to TOML file
        std::ofstream toml_file(file_path + std::string(TOML_EXTENSION));
        toml_file << metadata->to_toml();

        // Open binary data file
        auto io =
            std::make_unique<std::fstream>(file_path + std::string(QVR_EXTENSION), std::ios::out | std::ios::binary);
        Binary binary(file_path, metadata.value(), std::move(io));
        binary.fill_file_with_nulls();
        return binary;
    }
    default:
        throw std::invalid_argument("Invalid file mode: " + std::string(1, mode) +
                                    ". Use 'r' for read or 'w' for write.");
    }
}

std::vector<double> Binary::read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls) {
    validate_dimension_values(dims);

    go_to_position(calculate_file_position(dims), 'r');

    std::vector<double> data(impl_->metadata.labels.size());
    auto bytes = static_cast<int64_t>(data.size() * sizeof(double));
    impl_->io->read(reinterpret_cast<char*>(data.data()), bytes);
    impl_->current_position += bytes;

    if (!allow_nulls) {
        for (size_t i = 0; i < data.size(); ++i) {
            if (std::isnan(data[i])) {
                std::string dim_str;
                for (const auto& [name, value] : dims) {
                    if (!dim_str.empty())
                        dim_str += ", ";
                    dim_str += name + "=" + std::to_string(value);
                }
                throw std::runtime_error("Cannot read: data at {" + dim_str + "} contains null values");
            }
        }
    }

    return data;
}

void Binary::write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims) {
    validate_dimension_values(dims);
    validate_data_length(data);

    go_to_position(calculate_file_position(dims), 'w');

    auto bytes = static_cast<int64_t>(data.size() * sizeof(double));
    impl_->io->write(reinterpret_cast<const char*>(data.data()), bytes);
    impl_->current_position += bytes;
}

int64_t Binary::calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;
    int64_t position = 0;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        int64_t stride = 1;
        for (size_t j = i + 1; j < dimensions.size(); ++j) {
            stride *= dimensions[j].size;
        }
        position += (dims.at(dimensions[i].name) - 1) * stride;
    }
    position *= static_cast<int64_t>(metadata.labels.size());
    position *= static_cast<int64_t>(sizeof(double));
    return position;
}

void Binary::go_to_position(int64_t position, char mode) {
    if (impl_->current_position == position) {
        return;
    }
    switch (mode) {
    case 'r':
        impl_->io->seekg(position);
        break;
    case 'w':
        impl_->io->seekp(position);
        break;
    default:
        throw std::invalid_argument("Invalid seek mode: " + std::string(1, mode) +
                                    ". Use 'r' for read or 'w' for write.");
    }
    impl_->current_position = position;
}

void Binary::validate_file_is_open() const {
    if (!impl_->io || !impl_->io->good()) {
        throw std::runtime_error("File is not open: " + impl_->file_path);
    }
}

void Binary::validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims) {
    const auto& metadata = impl_->metadata;
    const auto& dimensions = metadata.dimensions;

    // Check count
    if (dims.size() != dimensions.size()) {
        throw std::invalid_argument("Expected " + std::to_string(dimensions.size()) + " dimensions, got " +
                                    std::to_string(dims.size()));
    }

    // Check all dimension names exist and values are in bounds
    for (size_t i = 0; i < dimensions.size(); ++i) {
        const auto& dim = dimensions[i];
        auto it = dims.find(dim.name);
        if (it == dims.end()) {
            throw std::invalid_argument("Missing required dimension: '" + dim.name + "'");
        }
        int64_t value = it->second;
        if (value < 1 || value > dim.size) {
            throw std::invalid_argument("Dimension '" + dim.name + "' value " + std::to_string(value) +
                                        " is out of bounds [1, " + std::to_string(dim.size) + "]");
        }
    }

    if (metadata.number_of_time_dimensions > 1) {
        // Build the datetime by accumulating offsets from each time dimension
        auto datetime = metadata.initial_datetime;
        for (const auto& dim : dimensions) {
            if (dim.is_time_dimension()) {
                datetime = dim.time->add_offset_from_int(datetime, dims.at(dim.name));
            }
        }

        // Verify that inner time dimensions are consistent with the resulting date
        bool first = true;
        for (const auto& dim : dimensions) {
            if (!dim.is_time_dimension())
                continue;
            if (first) {
                first = false;
                continue;
            }  // skip outermost time dimension

            int64_t expected_value = dims.at(dim.name);
            int64_t resulting_value = dim.time->datetime_to_int(datetime);
            if (expected_value != resulting_value) {
                throw std::invalid_argument("Invalid values for time dimensions: dimension '" + dim.name +
                                            "' has value " + std::to_string(expected_value) +
                                            " but the resulting datetime implies " + std::to_string(resulting_value));
            }
        }
    }
}

void Binary::validate_data_length(const std::vector<double>& data) {
    if (data.size() != impl_->metadata.labels.size()) {
        throw std::invalid_argument("Data length " + std::to_string(data.size()) + " does not match expected length " +
                                    std::to_string(impl_->metadata.labels.size()));
    }
}

std::vector<int64_t> Binary::next_dimensions(const std::vector<int64_t>& current_dimensions) {
    const auto& dimensions = impl_->metadata.dimensions;
    const auto& current_sizes = dimension_sizes_at_values(current_dimensions);

    std::vector<int64_t> next = current_dimensions;

    for (int i = static_cast<int>(next.size()) - 1; i >= 0; --i) {
        if (next[i] < current_sizes[i]) {
            next[i] += 1;
            break;
        } else {
            next[i] = 1;
        }
    }

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

std::vector<int64_t> Binary::dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const {
    using namespace quiver::time;
    const auto& metadata = impl_->metadata;
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

void Binary::fill_file_with_nulls() {
    const auto& metadata = impl_->metadata;

    // Calculate total number of cells
    int64_t total_cells = 1;
    for (const auto& dim : metadata.dimensions) {
        total_cells *= dim.size;
    }

    int64_t total_doubles = total_cells * static_cast<int64_t>(metadata.labels.size());

    // Write NaN-filled buffer in chunks to avoid excessive memory usage
    constexpr int64_t CHUNK_DOUBLES = 1024 * 1024;  // ~8 MB per chunk
    std::vector<double> buffer(static_cast<size_t>(std::min(total_doubles, CHUNK_DOUBLES)),
                               std::numeric_limits<double>::quiet_NaN());

    impl_->io->seekp(0);
    impl_->current_position = 0;
    int64_t remaining = total_doubles;
    while (remaining > 0) {
        int64_t count = std::min(remaining, static_cast<int64_t>(buffer.size()));
        auto bytes = count * static_cast<int64_t>(sizeof(double));
        impl_->io->write(reinterpret_cast<const char*>(buffer.data()), bytes);
        impl_->current_position += bytes;
        remaining -= count;
    }
    impl_->io->flush();
}

const BinaryMetadata& Binary::get_metadata() const {
    return impl_->metadata;
}

const std::string& Binary::get_file_path() const {
    return impl_->file_path;
}

std::iostream& Binary::get_io() {
    return *impl_->io;
}

}  // namespace quiver