#ifndef QUIVER_TYPE_VALIDATOR_H
#define QUIVER_TYPE_VALIDATOR_H

#include "quiver/data_type.h"
#include "quiver/value.h"
#include "schema.h"

#include <string>
#include <vector>

namespace quiver {

// Scalar/array type validation for create_element / update_element (the scalar half of the one
// typing policy, root AGENTS.md). `caller` names the public operation for Pattern 1 messages.
void validate_scalar(const std::string& caller,
                     const Schema& schema,
                     const std::string& table,
                     const std::string& column,
                     const Value& value);
void validate_array(const std::string& caller,
                    const Schema& schema,
                    const std::string& table,
                    const std::string& column,
                    const std::vector<Value>& values);
void validate_value(const std::string& caller, const std::string& context, DataType expected_type, const Value& value);

}  // namespace quiver

#endif  // QUIVER_TYPE_VALIDATOR_H
