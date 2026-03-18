#include "quiver/binary/binary.h"

#include "binary_utils.h"
#include "quiver/binary/dimension.h"
#include "quiver/binary/time_constants.h"

#include <chrono>
#include <cmath>
#include <cstring>
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
};

Binary::Binary(const std::string& file_path, const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io)
    : impl_(std::make_unique<Impl>(Impl{std::move(io), file_path, metadata})) {}

Binary::~Binary() = default;

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
    impl_->io->read(reinterpret_cast<char*>(data.data()), data.size() * sizeof(double));

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

    impl_->io->write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(double));
    impl_->io->flush();
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
    if (position < 0) {
        throw std::invalid_argument("File position must be non-negative, got " + std::to_string(position));
    }
    validate_file_is_open();
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

    // Get initial dimension values
    const auto& dimensions = metadata.dimensions;
    std::vector<int64_t> initial_dimensions;
    for (const auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            initial_dimensions.push_back(dim.time->initial_value);
        } else {
            initial_dimensions.push_back(1);
        }
    }

    // Calculate maximum number of lines
    int64_t max_lines = 1;
    for (const auto& dim : dimensions) {
        max_lines *= dim.size;
    }

    // Iterate CSV lines and write to binary
    std::vector<int64_t> current_dimensions = initial_dimensions;
    for (int64_t i = 0; i < max_lines; ++i) {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dimensions[j];
        }
        std::vector<double> data(metadata.labels.size(), std::numeric_limits<double>::quiet_NaN());
        write(data, dims);

        current_dimensions = next_dimensions(current_dimensions);
        if (current_dimensions == initial_dimensions) {
            break;
        }
    }
}

std::string Binary::build_data_report(const std::string& file_path1,
                                      const std::string& file_path2,
                                      Binary& binary1,
                                      Binary& binary2) {
    constexpr int MAX_REPORT_LINES = 100;

    const auto& metadata = binary1.get_metadata();
    const auto& dimensions = metadata.dimensions;

    // Get initial dimension values
    std::vector<int64_t> initial_dimensions;
    for (const auto& dim : dimensions) {
        if (dim.is_time_dimension()) {
            initial_dimensions.push_back(dim.time->initial_value);
        } else {
            initial_dimensions.push_back(1);
        }
    }

    // Calculate maximum number of lines
    int64_t max_lines = 1;
    for (const auto& dim : dimensions) {
        max_lines *= dim.size;
    }

    std::string report = "Data mismatch between '" + file_path1 + "' and '" + file_path2 + "':\n";
    int report_lines = 0;
    auto current_dims = initial_dimensions;
    for (int64_t i = 0; i < max_lines; ++i) {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dims[j];
        }

        auto data1 = binary1.read(dims, true);
        auto data2 = binary2.read(dims, true);

        for (size_t k = 0; k < data1.size(); ++k) {
            bool nan1 = std::isnan(data1[k]);
            bool nan2 = std::isnan(data2[k]);
            if (nan1 != nan2 || (!nan1 && data1[k] != data2[k])) {
                // Build dimension string
                std::string dim_str;
                for (size_t j = 0; j < dimensions.size(); ++j) {
                    if (!dim_str.empty())
                        dim_str += ", ";
                    dim_str += dimensions[j].name + "=" + std::to_string(current_dims[j]);
                }

                // Build data string
                std::string data_str = "'" + metadata.labels[k] + "': ";
                if (nan1) {
                    data_str += "NaN";
                } else {
                    data_str += std::to_string(data1[k]);
                }
                data_str += " vs ";
                if (nan2) {
                    data_str += "NaN";
                } else {
                    data_str += std::to_string(data2[k]);
                }

                // Append to report
                report += "  {" + dim_str + "}, " + data_str + "\n";
                report_lines++;
                if (report_lines >= MAX_REPORT_LINES) {
                    report += "  more ...\n";
                    return report;
                }
            }
        }

        current_dims = binary1.next_dimensions(current_dims);
        if (current_dims == initial_dimensions)
            break;
    }

    return report;
}

CompareResult
Binary::compare_files(const std::string& file_path1, const std::string& file_path2, bool detailed_report) {
    auto binary1 = open_file(file_path1, 'r');
    auto binary2 = open_file(file_path2, 'r');
    std::string report;

    const auto& metadata1 = binary1.get_metadata();
    const auto& metadata2 = binary2.get_metadata();

    if (metadata1 != metadata2) {
        if (detailed_report) {
            report = "Metadata mismatch between files '" + file_path1 + "' and '" + file_path2 + "'.\n";
        }
        return {CompareStatus::MetadataMismatch, report};
    }

    // Fast comparison via raw binary data
    auto& io1 = binary1.get_io();
    auto& io2 = binary2.get_io();
    io1.seekg(0);
    io2.seekg(0);

    CompareStatus status = CompareStatus::FileMatch;
    constexpr size_t BUFFER_SIZE = 8192;  // 8 KB chunks for raw binary comparison
    char buf1[BUFFER_SIZE];
    char buf2[BUFFER_SIZE];

    while (io1.good() && io2.good()) {
        io1.read(buf1, BUFFER_SIZE);
        io2.read(buf2, BUFFER_SIZE);

        auto bytes_read1 = io1.gcount();
        auto bytes_read2 = io2.gcount();

        if (bytes_read1 != bytes_read2 || std::memcmp(buf1, buf2, bytes_read1) != 0) {
            status = CompareStatus::DataMismatch;
            break;
        }

        if (bytes_read1 == 0)
            break;
    }

    if (detailed_report) {
        if (status == CompareStatus::DataMismatch) {
            // Clear stream state flags (eofbit/failbit) set by the memcmp loop
            io1.clear();
            io2.clear();
            report = build_data_report(file_path1, file_path2, binary1, binary2);
        } else if (status == CompareStatus::FileMatch) {
            report = "Files '" + file_path1 + "' and '" + file_path2 + "' match.";
        }
    }

    return {status, report};
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