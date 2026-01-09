#include "psr/type_validator.h"

#include <stdexcept>

namespace psr {

TypeValidator::TypeValidator(const Schema& schema) : schema_(schema) {}

void TypeValidator::validate_scalar(const std::string& table, const std::string& column, const Value& value) const {
    ColumnType expected = schema_.get_column_type(table, column);
    validate_value("column '" + column + "'", expected, value);
}

void TypeValidator::validate_vector_group(const std::string& collection,
                                          const std::string& group_name,
                                          const VectorGroup& group) const {
    std::string vector_table = Schema::vector_table_name(collection, group_name);
    const auto* table = schema_.get_table(vector_table);
    if (!table) {
        throw std::runtime_error("Vector table not found: " + vector_table);
    }

    for (size_t row_idx = 0; row_idx < group.size(); ++row_idx) {
        const auto& row = group[row_idx];
        for (const auto& [col_name, value] : row) {
            const auto* col = table->get_column(col_name);
            if (!col) {
                throw std::runtime_error("Column '" + col_name + "' not found in vector table '" + vector_table + "'");
            }
            validate_value("vector '" + group_name + "' row " + std::to_string(row_idx) + " column '" + col_name + "'",
                           col->type, value);
        }
    }
}

void TypeValidator::validate_set_group(const std::string& collection,
                                       const std::string& group_name,
                                       const SetGroup& group) const {
    std::string set_table = Schema::set_table_name(collection, group_name);
    const auto* table = schema_.get_table(set_table);
    if (!table) {
        throw std::runtime_error("Set table not found: " + set_table);
    }

    for (size_t row_idx = 0; row_idx < group.size(); ++row_idx) {
        const auto& row = group[row_idx];
        for (const auto& [col_name, value] : row) {
            const auto* col = table->get_column(col_name);
            if (!col) {
                throw std::runtime_error("Column '" + col_name + "' not found in set table '" + set_table + "'");
            }
            validate_value("set '" + group_name + "' row " + std::to_string(row_idx) + " column '" + col_name + "'",
                           col->type, value);
        }
    }
}

void TypeValidator::validate_value(const std::string& context, ColumnType expected_type, const Value& value) {
    std::visit(
        [&](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                // NULL allowed for any type
                return;
            } else if constexpr (std::is_same_v<T, std::vector<uint8_t>>) {
                // BLOB allowed for any type
                return;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                if (expected_type != ColumnType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             column_type_to_string(expected_type) + ", got INTEGER");
                }
            } else if constexpr (std::is_same_v<T, double>) {
                // REAL can be stored in INTEGER or REAL columns
                if (expected_type != ColumnType::Real && expected_type != ColumnType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             column_type_to_string(expected_type) + ", got REAL");
                }
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (expected_type != ColumnType::Text) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             column_type_to_string(expected_type) + ", got TEXT");
                }
            } else if constexpr (std::is_same_v<T, std::vector<int64_t>>) {
                if (expected_type != ColumnType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             column_type_to_string(expected_type) + ", got INTEGER[]");
                }
            } else if constexpr (std::is_same_v<T, std::vector<double>>) {
                if (expected_type != ColumnType::Real && expected_type != ColumnType::Integer) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             column_type_to_string(expected_type) + ", got REAL[]");
                }
            } else if constexpr (std::is_same_v<T, std::vector<std::string>>) {
                if (expected_type != ColumnType::Text) {
                    throw std::runtime_error("Type mismatch for " + context + ": expected " +
                                             column_type_to_string(expected_type) + ", got TEXT[]");
                }
            }
        },
        value);
}

}  // namespace psr
