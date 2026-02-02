#ifndef QUIVER_TIME_SERIES_METADATA_VALIDATOR_H
#define QUIVER_TIME_SERIES_METADATA_VALIDATOR_H

#include "export.h"
#include "time_series_metadata.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace quiver {

// Validates that a TimeSeriesMetadata object has internal consistency
class QUIVER_API TimeSeriesMetadataValidator {
public:
    explicit TimeSeriesMetadataValidator(const TimeSeriesMetadata& metadata);

    // Throws std::runtime_error on validation failure
    void validate();

private:
    const TimeSeriesMetadata& metadata_;

    // Metadata validations
    void validate_time_dimension_metadata();
    void validate_time_dimension_sizes();

    // Helper
    void validation_error(const std::string& message);
};

}  // namespace quiver

#endif  // QUIVER_TIME_SERIES_METADATA_VALIDATOR_H
