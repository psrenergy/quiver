#include "psr/type_validator.h"

#include <stdexcept>

namespace psr {

TypeValidator::TypeValidator(const Schema& schema) : schema_(schema) {}

void TypeValidator::validate_scalar(const std::string& table, const std::string& column, const Value& value) const {
    auto expected = schema_.get_data_type(table, column);
    validate_value("column '" + column + "'", expected, value);
}

void TypeValidator::validate_array(const std::string& table,
                                   const std::string& column,
                                   const std::vector<Value>& values) const {
    auto expected = schema_.get_data_type(table, column);
    for (size_t i = 0; i < values.size(); ++i) {
        validate_value("array '" + column + "' index " + std::to_string(i), expected, values[i]);
    }
}

void TypeValidator::validate_value(const std::string& context, DataType expected_type, const Value& value) {
    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                // NULL allowed for any type
                return;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                if (expected_type != DataType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             data_type_to_string(expected_type) + ", got INTEGER");
                }
            } else if constexpr (std::is_same_v<T, double>) {
                // REAL can be stored in INTEGER or REAL columns
                if (expected_type != DataType::Real && expected_type != DataType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             data_type_to_string(expected_type) + ", got REAL");
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                // String can go to TEXT or INTEGER (FK label resolution happens elsewhere)
                if (expected_type != DataType::Text && expected_type != DataType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             data_type_to_string(expected_type) + ", got TEXT");
                }
            }
        },
        value);
}

}  // namespace psr
