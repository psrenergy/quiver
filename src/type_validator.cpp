#include "quiver/type_validator.h"

#include <stdexcept>

namespace quiver {

TypeValidator::TypeValidator(const Schema& schema) : schema_(schema) {}

void TypeValidator::validate_scalar(const std::string& caller,
                                    const std::string& table,
                                    const std::string& column,
                                    const Value& value) const {
    auto expected = schema_.get_data_type(table, column);
    validate_value(caller, "column '" + column + "'", expected, value);
}

void TypeValidator::validate_array(const std::string& caller,
                                   const std::string& table,
                                   const std::string& column,
                                   const std::vector<Value>& values) const {
    auto expected = schema_.get_data_type(table, column);
    for (size_t i = 0; i < values.size(); ++i) {
        validate_value(caller, "array '" + column + "' index " + std::to_string(i), expected, values[i]);
    }
}

void TypeValidator::validate_value(const std::string& caller,
                                   const std::string& context,
                                   DataType expected_type,
                                   const Value& value) {
    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                // NULL allowed for any type
                return;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                // Integers are accepted for INTEGER and REAL columns (SQLite STRICT converts
                // whole numbers to real on insert) -- the single coercion policy shared with
                // internal::value_matches_type (used by the time-series writers).
                if (expected_type != DataType::Integer && expected_type != DataType::Real) {
                    throw std::runtime_error("Cannot " + caller + ": type mismatch for " + context + ": expected " +
                                             data_type_to_string(expected_type) + ", got INTEGER");
                }
            } else if constexpr (std::is_same_v<T, double>) {
                // Floats only go to REAL columns -- writing a float into an INTEGER column is
                // rejected (it would silently truncate or fail SQLite STRICT at insert time).
                if (expected_type != DataType::Real) {
                    throw std::runtime_error("Cannot " + caller + ": type mismatch for " + context + ": expected " +
                                             data_type_to_string(expected_type) + ", got REAL");
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                // String can go to TEXT, INTEGER (FK label resolution), or DATE_TIME (stored as TEXT)
                if (expected_type != DataType::Text && expected_type != DataType::Integer &&
                    expected_type != DataType::DateTime) {
                    throw std::runtime_error("Cannot " + caller + ": type mismatch for " + context + ": expected " +
                                             data_type_to_string(expected_type) + ", got TEXT");
                }
            }
        },
        value);
}

}  // namespace quiver
