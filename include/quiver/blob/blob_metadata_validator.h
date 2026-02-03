#ifndef QUIVER_BLOB_METADATA_VALIDATOR_H
#define QUIVER_BLOB_METADATA_VALIDATOR_H

#include "export.h"
#include "blob_metadata.h"

#include <string>

namespace quiver {

// Validates that a BlobMetadata object has internal consistency
class QUIVER_API BlobMetadataValidator {
public:
    explicit BlobMetadataValidator(const BlobMetadata& metadata);

    // Throws std::runtime_error on validation failure
    void validate();

private:
    const BlobMetadata& metadata_;

    // Metadata validations
    void validate_time_dimension_metadata();
    void validate_time_dimension_sizes();

    // Helper
    void validation_error(const std::string& message);
};

}  // namespace quiver

#endif  // QUIVER_BLOB_METADATA_VALIDATOR_H
