#ifndef DECK_DATABASE_SCHEMA_VALIDATOR_H
#define DECK_DATABASE_SCHEMA_VALIDATOR_H

#include "export.h"
#include "schema.h"

#include <string>
#include <vector>

namespace margaux {

// Validates that a schema follows Margaux conventions:
// - Configuration table exists
// - Collections have id/label with proper constraints
// - Vector tables have proper structure and FK constraints
// - Set tables have proper UNIQUE constraints
// - No duplicate attributes across collection and its vector tables
class DECK_DATABASE_API SchemaValidator {
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
    void validate_vector_table(const std::string& name);
    void validate_set_table(const std::string& name);
    void validate_no_duplicate_attributes();
    void validate_foreign_keys();

    // Helper
    void validation_error(const std::string& message);
};

}  // namespace margaux

#endif  // DECK_DATABASE_SCHEMA_VALIDATOR_H
