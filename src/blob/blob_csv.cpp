#include "quiver/blob/blob_csv.h"

#include "blob_utils.h"
#include "quiver/blob/blob.h"
#include "quiver/blob/dimension.h"
#include "utils/datetime.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <spdlog/fmt/fmt.h>
#include <sstream>
#include <stdexcept>

namespace quiver {

struct BlobCSV::Impl {
    bool aggregate_time_dimensions;
};

BlobCSV::BlobCSV(const std::string& file_path,
                 const BlobMetadata& metadata,
                 std::unique_ptr<std::iostream> io,
                 bool aggregate_time_dimensions)
    : Blob(file_path, metadata, std::move(io)), impl_(std::make_unique<Impl>(Impl{aggregate_time_dimensions})) {}

BlobCSV::~BlobCSV() = default;

BlobCSV::BlobCSV(BlobCSV&& other) noexcept = default;
BlobCSV& BlobCSV::operator=(BlobCSV&& other) noexcept = default;

void BlobCSV::csv_to_bin(const std::string& file_path) {
    // Read TOML metadata
    std::ifstream toml_file(file_path + std::string(TOML_EXTENSION));
    std::string toml_content((std::istreambuf_iterator<char>(toml_file)), std::istreambuf_iterator<char>());
    auto metadata = BlobMetadata::from_toml(toml_content);

    // Open the CSV file and detect whether time dimensions are aggregated
    auto csv_io = std::make_unique<std::fstream>(file_path + std::string(CSV_EXTENSION), std::ios::in);
    std::string header_line;
    std::getline(*csv_io, header_line);
    bool aggregate_time_dimensions = false;
    if (metadata.number_of_time_dimensions > 0) {
        std::string first_field = header_line.substr(0, header_line.find(','));
        aggregate_time_dimensions = (first_field == "datetime" || first_field == "date");
    }
    csv_io->seekg(0);  // Rewind so validate_header can re-read

    BlobCSV csv_reader(file_path, metadata, std::move(csv_io), aggregate_time_dimensions);
    csv_reader.validate_header();

    // Open the binary file in write mode
    Blob bin_writer = Blob::open_file(file_path, 'w', metadata);

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
        auto row = csv_reader.read_line();
        csv_reader.validate_dimensions(row.dimension_values, current_dimensions);

        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dimensions[j];
        }
        bin_writer.write(row.data, dims);

        current_dimensions = csv_reader.next_dimensions(current_dimensions);
        if (current_dimensions == initial_dimensions) {
            break;
        }
    }
}

void BlobCSV::bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions) {
    // Open the binary file in read mode
    Blob bin_reader = Blob::open_file(file_path, 'r');
    const auto& metadata = bin_reader.get_metadata();

    // Open the CSV file in write mode
    auto csv_io = std::make_unique<std::fstream>(file_path + std::string(CSV_EXTENSION), std::ios::out);
    BlobCSV csv_writer(file_path, metadata, std::move(csv_io), aggregate_time_dimensions);
    csv_writer.write_header();

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

    // Iterate files and write to CSV
    std::vector<int64_t> current_dimensions = initial_dimensions;
    for (int64_t i = 0; i < max_lines; ++i) {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dimensions[j];
        }

        std::vector<double> data = bin_reader.read(dims, true);
        csv_writer.get_io() << csv_writer.build_line(data, current_dimensions);

        current_dimensions = csv_writer.next_dimensions(current_dimensions);
        if (current_dimensions == initial_dimensions) {
            break;
        }
    }
}

BlobCSV::CSVRow BlobCSV::read_line() {
    std::string line;
    std::getline(get_io(), line);

    // The first N fields are dimension labels (strings), the rest are data values (doubles).
    // N depends on whether time dimensions are aggregated into a single datetime column.
    const size_t n_dim_fields = expected_dimension_names().size();
    CSVRow row;
    size_t field_start = 0;
    while (field_start <= line.size()) {
        // Find the next comma, or use end-of-line if none remains.
        size_t comma_position = line.find(',', field_start);
        size_t field_end;
        if (comma_position == std::string::npos) {
            field_end = line.size();
        } else {
            field_end = comma_position;
        }
        std::string field(line, field_start, field_end - field_start);

        // Route the field: dimension labels come first, data values follow.
        if (row.dimension_values.size() < n_dim_fields) {
            row.dimension_values.push_back(std::move(field));
        } else {
            // Convert data value to double, treating "null" as NaN.
            if (field == "null") {
                row.data.push_back(std::numeric_limits<double>::quiet_NaN());
            } else {
                row.data.push_back(std::stod(field));
            }
        }

        // Advance past the comma. If there was no comma, jump past end-of-line to exit the loop.
        if (comma_position == std::string::npos) {
            field_start = line.size() + 1;
        } else {
            field_start = comma_position + 1;
        }
    }
    return row;
}

std::string BlobCSV::build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions) {
    const auto& dimensions = get_metadata().dimensions;
    std::vector<std::string> elements;

    bool aggregate_time =
        impl_->aggregate_time_dimensions &&
        std::any_of(dimensions.begin(), dimensions.end(), [](const auto& d) { return d.is_time_dimension(); });

    if (aggregate_time) {
        elements.push_back(build_datetime_string_from_time_dimension_values(current_dimensions));
    }

    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (aggregate_time && dimensions[i].is_time_dimension())
            continue;
        elements.push_back(std::to_string(current_dimensions[i]));
    }

    for (double v : data) {
        if (std::isnan(v)) {
            elements.push_back("null");
        } else {
            elements.push_back(fmt::format("{:.6g}", v));
        }
    }

    std::ostringstream oss;
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0)
            oss << ',';
        oss << elements[i];
    }
    oss << '\n';
    return oss.str();
}

std::string
BlobCSV::build_datetime_string_from_time_dimension_values(const std::vector<int64_t>& time_dimension_values) const {
    const auto& metadata = get_metadata();
    const auto& dimensions = metadata.dimensions;

    auto datetime = metadata.initial_datetime;
    bool has_hourly = false;
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (!dimensions[i].is_time_dimension())
            continue;
        datetime = dimensions[i].time->add_offset_from_int(datetime, time_dimension_values[i]);
        if (dimensions[i].time->frequency == TimeFrequency::Hourly)
            has_hourly = true;
    }

    if (has_hourly) {
        return quiver::datetime::format_utc(datetime);
    }
    // Date-only: truncate the full UTC string to YYYY-MM-DD
    return quiver::datetime::format_utc(datetime).substr(0, 10);
}

void BlobCSV::write_header() {
    const auto& metadata = get_metadata();
    const auto& dimensions = metadata.dimensions;

    std::vector<std::string> header;

    bool aggregate_time =
        impl_->aggregate_time_dimensions &&
        std::any_of(dimensions.begin(), dimensions.end(), [](const auto& d) { return d.is_time_dimension(); });

    if (aggregate_time) {
        bool has_hourly = false;
        for (const auto& d : dimensions) {
            if (d.is_time_dimension() && d.time->frequency == TimeFrequency::Hourly) {
                has_hourly = true;
                break;
            }
        }
        if (has_hourly) {
            header.push_back("datetime");
        } else {
            header.push_back("date");
        }
    }

    for (const auto& dim : dimensions) {
        if (aggregate_time && dim.is_time_dimension())
            continue;
        header.push_back(dim.name);
    }

    for (const auto& label : metadata.labels) {
        header.push_back(label);
    }

    auto& io = get_io();
    for (size_t i = 0; i < header.size(); ++i) {
        if (i > 0)
            io << ',';
        io << header[i];
    }
    io << '\n';
}

std::vector<std::string> BlobCSV::expected_dimension_names() const {
    const auto& metadata = get_metadata();
    const auto& dimensions = metadata.dimensions;

    std::vector<std::string> names;
    bool aggregate_time =
        impl_->aggregate_time_dimensions &&
        std::any_of(dimensions.begin(), dimensions.end(), [](const auto& d) { return d.is_time_dimension(); });

    if (aggregate_time) {
        bool has_hourly = false;
        for (const auto& dim : dimensions) {
            if (dim.is_time_dimension() && dim.time->frequency == TimeFrequency::Hourly) {
                has_hourly = true;
                break;
            }
        }
        if (has_hourly) {
            names.push_back("datetime");
        } else {
            names.push_back("date");
        }
        for (const auto& dim : dimensions) {
            if (!dim.is_time_dimension()) {
                names.push_back(dim.name);
            }
        }
    } else {
        for (const auto& dim : dimensions) {
            names.push_back(dim.name);
        }
    }
    return names;
}

void BlobCSV::validate_header() {
    const auto& metadata = get_metadata();
    std::string header_line;
    std::getline(get_io(), header_line);

    // Build the expected header: dimension names (or aggregated date/datetime) followed by labels.
    std::vector<std::string> expected = expected_dimension_names();
    for (const auto& label : metadata.labels) {
        expected.push_back(label);
    }

    // Split the header line on commas and compare each field against the expected column name.
    size_t field_start = 0;
    size_t field_index = 0;
    while (field_start <= header_line.size()) {
        size_t comma_position = header_line.find(',', field_start);
        size_t field_end;
        if (comma_position == std::string::npos) {
            field_end = header_line.size();
        } else {
            field_end = comma_position;
        }
        std::string field(header_line, field_start, field_end - field_start);

        if (field_index >= expected.size() || field != expected[field_index]) {
            std::string expected_str;
            for (size_t i = 0; i < expected.size(); ++i) {
                if (i > 0)
                    expected_str += ", ";
                expected_str += expected[i];
            }
            throw std::runtime_error("Unexpected header in CSV file: '" + header_line +
                                     "'. Expected columns are: " + expected_str);
        }

        field_index++;
        // Advance past the comma. If there was no comma, jump past end-of-line to exit the loop.
        if (comma_position == std::string::npos) {
            field_start = header_line.size() + 1;
        } else {
            field_start = comma_position + 1;
        }
    }

    if (field_index != expected.size()) {
        throw std::runtime_error("CSV header has " + std::to_string(field_index) + " columns, expected " +
                                 std::to_string(expected.size()));
    }
}

void BlobCSV::validate_dimensions(const std::vector<std::string>& csv_dimension_values,
                                  const std::vector<int64_t>& current_bin_dimension_values) {
    const auto& metadata = get_metadata();
    const auto& dimensions = metadata.dimensions;

    std::vector<std::string> expected_names = expected_dimension_names();
    std::vector<std::string> expected_values;

    bool aggregate_time =
        impl_->aggregate_time_dimensions &&
        std::any_of(dimensions.begin(), dimensions.end(), [](const auto& d) { return d.is_time_dimension(); });

    // Build expected values
    if (aggregate_time) {
        // First expected value is the aggregated datetime string.
        expected_values.push_back(build_datetime_string_from_time_dimension_values(current_bin_dimension_values));
        // Remaining expected values are the non-time dimension integers.
        for (size_t i = 0; i < dimensions.size(); ++i) {
            if (!dimensions[i].is_time_dimension()) {
                expected_values.push_back(std::to_string(current_bin_dimension_values[i]));
            }
        }
    } else {
        for (size_t i = 0; i < dimensions.size(); ++i) {
            expected_values.push_back(std::to_string(current_bin_dimension_values[i]));
        }
    }

    // Compare CSV dimension values against expected values
    for (size_t i = 0; i < expected_names.size(); ++i) {
        if (csv_dimension_values[i] != expected_values[i]) {
            throw std::runtime_error("CSV dimension '" + expected_names[i] + "' has value '" + csv_dimension_values[i] +
                                     "', expected '" + expected_values[i] + "'");
        }
    }
}

}  // namespace quiver