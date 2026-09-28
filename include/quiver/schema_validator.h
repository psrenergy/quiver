#ifndef QUIVER_SCHEMA_VALIDATOR_H
#define QUIVER_SCHEMA_VALIDATOR_H

#include "export.h"
#include "schema.h"

#include <string>
#include <vector>

namespace quiver {

// Validates that a schema follows QUIVER conventions:
// - Configuration table exists
// - Collections have id/label with proper constraints
// - Every vector, set and time series table is named after an existing collection, and its `id`
//   references that collection with ON DELETE CASCADE ON UPDATE CASCADE
// - Vector tables have a vector_index column, and their id column is not the sole primary key
// - Set tables have proper UNIQUE constraints
// - Every foreign key uses ON UPDATE CASCADE and ON DELETE SET NULL or CASCADE
// - No duplicate attributes across a collection and its vector, set and time series tables
class QUIVER_API SchemaValidator {
public:
    explicit SchemaValidator(const Schema& schema);

    // Throws std::runtime_error on validation failure
    void validate();

private:
    const Schema& schema_;
    std::vector<std::string> collections_;

    // Individual validations
    void validate_configuration_exists();
    void validate_collection_names();
    void validate_collection(const std::string& name);
    void validate_group_parent(const std::string& name, const std::string& kind);
    void validate_vector_table(const std::string& name);
    void validate_set_table(const std::string& name);
    void validate_time_series_files_table(const std::string& name);
    void validate_no_duplicate_attributes();
    void validate_foreign_keys();

    // Helper
    void validation_error(const std::string& message);
};

}  // namespace quiver

#endif  // QUIVER_SCHEMA_VALIDATOR_H
