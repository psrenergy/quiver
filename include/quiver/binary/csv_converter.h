#ifndef QUIVER_CSV_CONVERTER_H
#define QUIVER_CSV_CONVERTER_H

#include "../export.h"
#include "binary_metadata.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace quiver {

class QUIVER_API CSVConverter {
public:
    static void csv_to_bin(const std::string& file_path);
    static void bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions = true);

private:
    // Only the two static entry points above construct a converter.
    CSVConverter(const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io, bool aggregate_time_dimensions);

    struct CSVRow {
        std::vector<std::string> dimension_values;
        std::vector<double> data;
    };

    // CSV Builders
    std::string build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions);
    std::string build_datetime_string_from_time_dimension_values(
        const std::vector<int64_t>& time_dimension_values
    ) const;
    std::vector<std::string> dimension_cells(const std::vector<int64_t>& current_dimensions) const;
    void write_header();

    // CSV Readers
    CSVRow read_line(size_t line_number);

    // Validations
    void validate_dimensions(
        const std::vector<std::string>& dimension_values,
        const std::vector<int64_t>& current_dimensions
    );
    void validate_header();

    // Time-dimension predicates
    bool aggregates_time_dimensions() const;
    bool has_hourly_dimension() const;

    BinaryMetadata metadata_;
    std::unique_ptr<std::iostream> io_;
    bool aggregate_time_dimensions_ = false;
    // The CSV column names, built once by the constructor: the dimension columns (or, when time
    // dimensions are aggregated, a leading date/datetime column instead of the time ones), then the labels.
    std::vector<std::string> header_;
};

}  // namespace quiver

#endif  // QUIVER_CSV_CONVERTER_H
