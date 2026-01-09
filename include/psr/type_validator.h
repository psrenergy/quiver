#ifndef PSR_TYPE_VALIDATOR_H
#define PSR_TYPE_VALIDATOR_H

#include "column_type.h"
#include "element.h"
#include "export.h"
#include "schema.h"
#include "value.h"

#include <string>

namespace psr {

class PSR_API TypeValidator {
public:
    explicit TypeValidator(const Schema& schema);

    // Validate a scalar value against a column in a table
    // Throws std::runtime_error on mismatch
    void validate_scalar(const std::string& table, const std::string& column, const Value& value) const;

    // Validate a vector group against the expected types in the vector table
    void validate_vector_group(const std::string& collection, const std::string& group_name, const VectorGroup& group) const;

    // Validate a set group against the expected types in the set table
    void validate_set_group(const std::string& collection, const std::string& group_name, const SetGroup& group) const;

    // Low-level: validate value against explicit type
    static void validate_value(const std::string& context, ColumnType expected_type, const Value& value);

private:
    const Schema& schema_;
};

}  // namespace psr

#endif  // PSR_TYPE_VALIDATOR_H
