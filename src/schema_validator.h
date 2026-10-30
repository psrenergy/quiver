#ifndef QUIVER_SCHEMA_VALIDATOR_H
#define QUIVER_SCHEMA_VALIDATOR_H

#include "schema.h"

namespace quiver {

// Validates that a schema follows QUIVER conventions, throwing std::runtime_error on the first failure:
// - Configuration table exists
// - Collections have id/label with proper constraints
// - Every vector, set and time series table is named after an existing collection, and its `id`
//   references that collection with ON DELETE CASCADE ON UPDATE CASCADE
// - Vector tables have a vector_index column, and their id column is not the sole primary key
// - Set tables have proper UNIQUE constraints
// - Every foreign key uses ON UPDATE CASCADE and ON DELETE SET NULL or CASCADE
// - No duplicate attributes across a collection and its vector, set and time series tables
void validate_schema(const Schema& schema);

}  // namespace quiver

#endif  // QUIVER_SCHEMA_VALIDATOR_H
