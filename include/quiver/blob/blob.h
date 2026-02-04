#ifndef QUIVER_BLOB_H
#define QUIVER_BLOB_H

#include "blob_metadata.h"
#include "export.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

class QUIVER_API Blob {
    explicit Blob(const std::string& file_path, const BlobMetadata& metadata, const std::iostream& io);
    ~Blob();

    // Non-copyable
    Blob(const Blob&) = delete;
    Blob& operator=(const Blob&) = delete;

    // Movable
    Blob(Blob&& other) noexcept;
    Blob& operator=(Blob&& other) noexcept;

    // File handling
    static Blob&
    open_file(const std::string& file_path, const std::string& mode, const std::optional<BlobMetadata>& metadata);

    // Data handling
    double read(const std::unordered_map<std::string, int64_t>& dims);
    double write(const std::vector<double>& data, const std::unordered_map<std::string, int64_t>& dims);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    // File traversal
    int64_t calculate_file_position(const std::unordered_map<std::string, int64_t>& dims) const;
    void go_to_position(int64_t position);

    // Validations
    void validate_file_is_open() const;
    void validate_dimension_values(const std::unordered_map<std::string, int64_t>& dims);
    void validate_data_length(const std::vector<double>& data);
    // validate_time_dimension_values implemented inline inside validate_dimension_values

protected:
    // Getters
    const BlobMetadata& get_metadata() const;
    const std::string& get_file_path() const;
    const std::iostream& get_io() const;
};

}  // namespace quiver

#endif  // QUIVER_BLOB_H
