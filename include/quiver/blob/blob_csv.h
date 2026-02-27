#ifndef QUIVER_BLOB_CSV_H
#define QUIVER_BLOB_CSV_H

#include "blob.h"
#include "../export.h"

#include <cstdint>
#include <ctime>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace quiver {

class QUIVER_API BlobCSV : public Blob {
public:
    explicit BlobCSV(const std::string& file_path,
                     const BlobMetadata& metadata,
                     std::unique_ptr<std::iostream> io,
                     bool aggregate_time_dimensions);
    ~BlobCSV();

    // Non-copyable
    BlobCSV(const BlobCSV&) = delete;
    BlobCSV& operator=(const BlobCSV&) = delete;

    // Movable
    BlobCSV(BlobCSV&& other) noexcept;
    BlobCSV& operator=(BlobCSV&& other) noexcept;

    static void csv_to_bin(const std::string& file_path);
    static void bin_to_csv(const std::string& file_path, bool aggregate_time_dimensions = true);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    struct CSVRow {
        std::vector<std::string> dimension_values;
        std::vector<double> data;
    };

    // CSV Builders
    std::string build_line(const std::vector<double>& data, const std::vector<int64_t>& current_dimensions);
    std::string
    build_datetime_string_from_time_dimension_values(const std::vector<int64_t>& time_dimension_values) const;
    void write_header();

    // Iterators
    std::vector<int64_t> next_dimensions(const std::vector<int64_t>& current_dimensions);
    std::vector<int64_t> dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const;

    // CSV Readers
    CSVRow read_line();

    // Validations
    std::vector<std::string> expected_dimension_names() const;
    void validate_dimensions(const std::vector<std::string>& dimension_values,
                             const std::vector<int64_t>& current_dimensions);
    void validate_header();
};

}  // namespace quiver

#endif  // QUIVER_BLOB_CSV_H
