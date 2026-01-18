#ifndef PSR_TYPE_VALIDATOR_H
#define PSR_TYPE_VALIDATOR_H

#include "data_type.h"
#include "export.h"
#include "schema.h"
#include "value.h"

#include <string>
#include <vector>

namespace psr {

class PSR_API TypeValidator {
public:
    explicit TypeValidator(const Schema& schema);

    // Validate a scalar value against a column in a table
    // Throws std::runtime_error on mismatch
    void validate_scalar(const std::string& table, const std::string& column, const Value& value) const;

    // Validate an array value against a column in a table
    void validate_array(const std::string& table, const std::string& column, const std::vector<Value>& values) const;

    // Low-level: validate value against explicit type
    static void validate_value(const std::string& context, DataType expected_type, const Value& value);

private:
    const Schema& schema_;
};

}  // namespace psr

#endif  // PSR_TYPE_VALIDATOR_H
