#ifndef QUIVER_BLOB_H
#define QUIVER_BLOB_H

#include "blob_metadata.h"
#include "../export.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

class QUIVER_API Blob {
public:
    explicit Blob(const std::string& file_path, const BlobMetadata& metadata, std::unique_ptr<std::iostream> io);
    ~Blob();

    // Non-copyable
    Blob(const Blob&) = delete;
    Blob& operator=(const Blob&) = delete;

    // Movable
    Blob(Blob&& other) noexcept;
    Blob& operator=(Blob&& other) noexcept;

    // File handling
    static Blob open_file(const std::string& file_path, char mode, const std::optional<BlobMetadata>& metadata = {});

    // Data handling
    std::vector<double> read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls = false);
    void write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);

    // Getters
    const BlobMetadata& get_metadata() const;
    const std::string& get_file_path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // File traversal
    int64_t calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const;
    void go_to_position(int64_t position, char mode);
    void fill_file_with_nulls();

    // Validations
    void validate_file_is_open() const;
    void validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims);
    void validate_data_length(const std::vector<double>& data);

protected:
    std::iostream& get_io();

    // Dimension iteration
    std::vector<int64_t> next_dimensions(const std::vector<int64_t>& current_dimensions);
    std::vector<int64_t> dimension_sizes_at_values(const std::vector<int64_t>& dimension_values) const;
};

}  // namespace quiver

#endif  // QUIVER_BLOB_H
