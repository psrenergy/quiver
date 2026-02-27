#ifndef QUIVER_BLOB_METADATA_H
#define QUIVER_BLOB_METADATA_H

#include "dimension.h"
#include "../export.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace quiver {

struct QUIVER_API BlobMetadata {
    std::vector<Dimension> dimensions;
    std::chrono::system_clock::time_point initial_datetime;
    std::string unit;
    std::vector<std::string> labels;
    std::string version;
    //
    int64_t number_of_time_dimensions;

    BlobMetadata();
    ~BlobMetadata();

    // TOML Serialization
    static BlobMetadata from_toml(const std::string& toml_content);
    std::string to_toml() const;

    // Validation
    void validate() const;
    void validate_time_dimension_metadata() const;
    void validate_time_dimension_sizes() const;

    // Setters
    void add_dimension(const std::string& name, int64_t size);
    void add_time_dimension(const std::string& name, int64_t size, const std::string& frequency);
};

}  // namespace quiver

#endif  // QUIVER_BLOB_METADATA_H
