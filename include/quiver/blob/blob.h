#ifndef QUIVER_BINARY_H
#define QUIVER_BINARY_H

#include "../export.h"
#include "binary_metadata.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

class QUIVER_API Binary {
public:
    explicit Binary(const std::string& file_path, const BinaryMetadata& metadata, std::unique_ptr<std::iostream> io);
    ~Binary();

    // Non-copyable
    Binary(const Binary&) = delete;
    Binary& operator=(const Binary&) = delete;

    // Movable
    Binary(Binary&& other) noexcept;
    Binary& operator=(Binary&& other) noexcept;

    // File handling
    static Binary open_file(const std::string& file_path, char mode, const std::optional<BinaryMetadata>& metadata = {});

    // Data handling
    std::vector<double> read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls = false);
    void write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);

    // Getters
    const BinaryMetadata& get_metadata() const;
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

#endif  // QUIVER_BINARY_H
