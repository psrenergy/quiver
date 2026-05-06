#ifndef QUIVER_BINARY_FILE_H
#define QUIVER_BINARY_FILE_H

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

class QUIVER_API BinaryFile {
public:
    explicit BinaryFile(const std::string& file_path,
                        const BinaryMetadata& metadata,
                        std::unique_ptr<std::iostream> io);
    ~BinaryFile();

    // Non-copyable
    BinaryFile(const BinaryFile&) = delete;
    BinaryFile& operator=(const BinaryFile&) = delete;

    // Movable
    BinaryFile(BinaryFile&& other) noexcept;
    BinaryFile& operator=(BinaryFile&& other) noexcept;

    // File handling
    static BinaryFile
    open_file(const std::string& file_path, char mode, const std::optional<BinaryMetadata>& metadata = {});

    // Data handling
    std::vector<double> read(const std::unordered_map<std::string, int64_t>& dims, bool allow_nulls = false);
    void write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);

    // Fast overloads (D-13/D-14): positional dimension values in metadata declaration order.
    // The vector must have one entry per dimension; the i-th entry is the value for dimensions[i].
    std::vector<double> read(const std::vector<int64_t>& dims, bool allow_nulls = false);

    // Trusted-caller fast path: NO dimension validation. Caller MUST guarantee `dims` is in
    // the valid range produced by quiver::binary::first_dimensions / next_dimensions, OR by
    // the validating read(...) overload's contract. Buffer `out` is resized only if its
    // current size differs from labels.size().
    void read_into(const std::vector<int64_t>& dims, std::vector<double>& out, bool allow_nulls = false);

    void write(const std::vector<double>& data, const std::vector<int64_t>& dims);

    // Getters
    const BinaryMetadata& get_metadata() const;
    const std::string& get_file_path() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // File traversal
    int64_t calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const;
    int64_t calculate_file_position_indexed(const std::vector<int64_t>& dims) const;
    void go_to_position(int64_t position, char mode);
    void fill_file_with_nulls();

    // Validations
    void validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims);
    void validate_dimension_values_indexed(const std::vector<int64_t>& dims);
    void validate_data_length(const std::vector<double>& data);

protected:
    std::iostream& get_io();
};

}  // namespace quiver

#endif  // QUIVER_BINARY_FILE_H
