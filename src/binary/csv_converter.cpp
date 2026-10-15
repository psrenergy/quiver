#include "quiver/binary/csv_converter.h"

#include "binary_utils.h"
#include "quiver/binary/binary_file.h"
#include "quiver/binary/dimension.h"
#include "quiver/binary/iteration.h"
#include "utils/datetime.h"
#include "utils/number.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace quiver {

namespace {

// Splits on every ',' and keeps empty fields, so "a,b," is three fields and "" is one. Do not swap
// this for std::getline(stream, field, ','): it drops the trailing empty field, which would let a
// row with a trailing comma pass read_line's width check one field short.
std::vector<std::string> split_fields(const std::string& line) {
    std::vector<std::string> fields;
    size_t field_start = 0;
    for (;;) {
        const size_t comma = line.find(',', field_start);
        if (comma == std::string::npos) {
            fields.push_back(line.substr(field_start));
            return fields;
        }
        fields.push_back(line.substr(field_start, comma - field_start));
        field_start = comma + 1;
    }
}

std::string join_fields(const std::vector<std::string>& fields, std::string_view separator) {
    std::string joined;
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            joined += separator;
        }
        joined += fields[i];
    }
    return joined;
}

}  // namespace

CSVConverter::CSVConverter(
    const BinaryMetadata& metadata,
    std::unique_ptr<std::iostream> io,
    bool aggregate_time_dimensions
)
    : metadata_(metadata), io_(std::move(io)), aggregate_time_dimensions_(aggregate_time_dimensions) {
    // The one definition of the CSV columns. write_header emits it, and validate_header, read_line
    // and validate_dimensions check against it, so a file bin_to_csv writes is exactly what
    // csv_to_bin accepts.
    const bool aggregate_time = aggregates_time_dimensions();
    if (aggregate_time) {
        header_.push_back(has_hourly_dimension() ? "datetime" : "date");
    }
    for (const auto& dim : metadata_.dimensions) {
        if (aggregate_time && dim.is_time_dimension()) {
            continue;
        }
        header_.push_back(dim.name);
    }
    header_.insert(header_.end(), metadata_.labels.begin(), metadata_.labels.end());
}

bool CSVConverter::aggregates_time_dimensions() const {
    const auto& dimensions = metadata_.dimensions;
    return aggregate_time_dimensions_ &&
           std::any_of(dimensions.begin(), dimensions.end(), [](const auto& d) { return d.is_time_dimension(); });
}

bool CSVConverter::has_hourly_dimension() const {
    const auto& dimensions = metadata_.dimensions;
    return std::any_of(dimensions.begin(), dimensions.end(), [](const auto& d) {
        return d.is_time_dimension() && d.time->frequency == TimeFrequency::Hourly;
    });
}

void CSVConverter::csv_to_bin(const std::string& file_path) {
    auto metadata = BinaryMetadata::from_toml_file(file_path);

    // Open the CSV file and detect whether time dimensions are aggregated
    auto csv_io = std::make_unique<std::fstream>(file_path + std::string(CSV_EXTENSION), std::ios::in);
    std::string header_line;
    std::getline(*csv_io, header_line);
    bool aggregate_time_dimensions = false;
    if (metadata.number_of_time_dimensions() > 0) {
        std::string first_field = header_line.substr(0, header_line.find(','));
        aggregate_time_dimensions = (first_field == "datetime" || first_field == "date");
    }
    csv_io->seekg(0);  // Rewind so validate_header can re-read

    CSVConverter csv_reader(metadata, std::move(csv_io), aggregate_time_dimensions);
    csv_reader.validate_header();

    // Open the binary file in write mode
    BinaryFile bin_writer = BinaryFile::open_file(file_path, 'w', metadata);

    // Iterate CSV lines and write to binary. Line 1 is the header, so data starts at line 2.
    const auto& dimensions = metadata.dimensions;
    std::vector<int64_t> current_dimensions = first_dimensions(metadata);
    for (size_t line_number = 2;; ++line_number) {
        auto row = csv_reader.read_line(line_number);
        csv_reader.validate_dimensions(row.dimension_values, current_dimensions);

        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dimensions[j];
        }
        bin_writer.write(row.data, dims);

        auto nxt = next_dimensions(metadata, current_dimensions);
        if (!nxt) {
            break;
        }
        current_dimensions = std::move(*nxt);
    }
}

void CSVConverter::bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions) {
    // Open the binary file in read mode
    BinaryFile bin_reader = BinaryFile::open_file(file_path, 'r');
    const auto& metadata = bin_reader.get_metadata();

    // Open the CSV file in write mode
    auto csv_io = std::make_unique<std::fstream>(file_path + std::string(CSV_EXTENSION), std::ios::out);
    CSVConverter csv_writer(metadata, std::move(csv_io), aggregate_time_dimensions);
    csv_writer.write_header();

    // Iterate files and write to CSV
    const auto& dimensions = metadata.dimensions;
    std::vector<int64_t> current_dimensions = first_dimensions(metadata);
    for (;;) {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t j = 0; j < dimensions.size(); ++j) {
            dims[dimensions[j].name] = current_dimensions[j];
        }

        std::vector<double> data = bin_reader.read(dims, true);
        *csv_writer.io_ << csv_writer.build_line(data, current_dimensions);

        auto nxt = next_dimensions(metadata, current_dimensions);
        if (!nxt) {
            break;
        }
        current_dimensions = std::move(*nxt);
    }
}

CSVConverter::CSVRow CSVConverter::read_line(size_t line_number) {
    std::string line;
    if (!std::getline(*io_, line)) {
        throw std::runtime_error("Cannot csv_to_bin: file ends before line " + std::to_string(line_number));
    }

    // Check the width before touching any cell: validate_dimensions indexes every dimension cell (a
    // short row would read past its end), and the data branch below names a cell's label by position
    // (a long row would index past the last label).
    std::vector<std::string> fields = split_fields(line);
    if (fields.size() != header_.size()) {
        throw std::runtime_error(
            "Cannot csv_to_bin: line " + std::to_string(line_number) + " has " + std::to_string(fields.size()) +
            " fields, expected " + std::to_string(header_.size())
        );
    }

    // The leading fields are dimension cells (strings), the rest are data values (doubles), one per label.
    const size_t n_dim_fields = header_.size() - metadata_.labels.size();
    CSVRow row;
    for (std::string& field : fields) {
        // Route the field: dimension cells come first, data values follow.
        if (row.dimension_values.size() < n_dim_fields) {
            row.dimension_values.push_back(std::move(field));
        } else {
            // Convert data value to double, treating "null" as NaN. The whole cell must parse, in the
            // "C" locale's number format whatever locale the host set -- bin_to_csv writes '.'.
            if (field == "null") {
                row.data.push_back(std::numeric_limits<double>::quiet_NaN());
            } else if (auto value = utils::parse_float(field)) {
                row.data.push_back(*value);
            } else {
                throw std::runtime_error(
                    "Cannot csv_to_bin: invalid float value '" + field + "' for label '" +
                    metadata_.labels[row.data.size()] + "'"
                );
            }
        }
    }
    return row;
}

std::string CSVConverter::build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions) {
    std::vector<std::string> cells = dimension_cells(current_dimensions);
    for (double v : data) {
        if (std::isnan(v)) {
            cells.push_back("null");
        } else {
            // Shortest text that reads back to the same double, '.' in every locale.
            std::string cell;
            utils::append_number(v, cell);
            cells.push_back(std::move(cell));
        }
    }
    return join_fields(cells, ",") + '\n';
}

std::string CSVConverter::build_datetime_string_from_time_dimension_values(
    const std::vector<int64_t>& time_dimension_values
) const {
    const auto datetime = calendar_cell_start(metadata_, time_dimension_values);
    if (has_hourly_dimension()) {
        return quiver::datetime::format_utc(datetime);
    }
    // Date-only: truncate the full UTC string to YYYY-MM-DD
    return quiver::datetime::format_utc(datetime).substr(0, 10);
}

// The dimension cells of one coordinate, in header_ order. build_line writes them and
// validate_dimensions checks against them.
std::vector<std::string> CSVConverter::dimension_cells(const std::vector<int64_t>& current_dimensions) const {
    const auto& dimensions = metadata_.dimensions;
    const bool aggregate_time = aggregates_time_dimensions();

    std::vector<std::string> cells;
    if (aggregate_time) {
        cells.push_back(build_datetime_string_from_time_dimension_values(current_dimensions));
    }
    for (size_t i = 0; i < dimensions.size(); ++i) {
        if (aggregate_time && dimensions[i].is_time_dimension()) {
            continue;
        }
        cells.push_back(std::to_string(current_dimensions[i]));
    }
    return cells;
}

void CSVConverter::write_header() {
    *io_ << join_fields(header_, ",") << '\n';
}

void CSVConverter::validate_header() {
    std::string header_line;
    std::getline(*io_, header_line);

    if (split_fields(header_line) != header_) {
        throw std::runtime_error(
            "Unexpected header in CSV file: '" + header_line + "'. Expected columns are: " + join_fields(header_, ", ")
        );
    }
}

void CSVConverter::validate_dimensions(
    const std::vector<std::string>& csv_dimension_values,
    const std::vector<int64_t>& current_bin_dimension_values
) {
    // read_line's width check guarantees csv_dimension_values has one cell per dimension column.
    const std::vector<std::string> expected_values = dimension_cells(current_bin_dimension_values);
    for (size_t i = 0; i < expected_values.size(); ++i) {
        if (csv_dimension_values[i] != expected_values[i]) {
            throw std::runtime_error(
                "CSV dimension '" + header_[i] + "' has value '" + csv_dimension_values[i] + "', expected '" +
                expected_values[i] + "'"
            );
        }
    }
}

}  // namespace quiver
