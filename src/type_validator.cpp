#include "type_validator.h"

#include "database_internal.h"
#include "utils/datetime.h"

#include <stdexcept>

namespace quiver {

namespace {

// Same wording as Impl::require_column, so one condition has one message.
DataType column_type(
    const Schema& schema,
    const std::string& caller,
    const std::string& table,
    const std::string& column
) {
    const auto* table_def = schema.get_table(table);
    const auto type = table_def ? table_def->get_data_type(column) : std::nullopt;
    if (!type) {
        throw std::runtime_error("Cannot " + caller + ": column '" + column + "' not found in table '" + table + "'");
    }
    return *type;
}

}  // namespace

void validate_scalar(
    const std::string& caller,
    const Schema& schema,
    const std::string& table,
    const std::string& column,
    const Value& value
) {
    validate_value(caller, "column '" + column + "'", column_type(schema, caller, table, column), value);
}

void validate_array(
    const std::string& caller,
    const Schema& schema,
    const std::string& table,
    const std::string& column,
    const std::vector<Value>& values
) {
    const auto expected = column_type(schema, caller, table, column);
    for (size_t i = 0; i < values.size(); ++i) {
        validate_value(caller, "array '" + column + "' index " + std::to_string(i), expected, values[i]);
    }
}

void validate_value(const std::string& caller, const std::string& context, DataType expected_type, const Value& value) {
    // The shape rule is the one typing policy (internal::value_matches_type): int64 -> INTEGER or
    // REAL, double -> REAL, string -> TEXT or DATE_TIME, NULL -> any. FK label strings never get
    // here: Impl::resolve_fk_label turns them into ids (or rejects them) first.
    if (!internal::value_matches_type(value, expected_type)) {
        throw std::runtime_error(
            "Cannot " + caller + ": type mismatch for " + context + ": expected " + data_type_to_string(expected_type) +
            ", got " + internal::value_type_name(value)
        );
    }
    // Content check, separate on purpose (see src/AGENTS.md): a DATE_TIME column is TEXT that every
    // binding parses back into a date, so an unparseable value is rejected here rather than
    // detonating in whichever binding reads it. Same predicate in validate_time_series_row.
    if (expected_type == DataType::DateTime) {
        if (const auto* s = std::get_if<std::string>(&value); s && !datetime::is_valid_iso8601(*s)) {
            throw std::runtime_error(
                "Cannot " + caller + ": invalid DATE_TIME value for " + context + ": '" + *s +
                "' (expected YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS)"
            );
        }
    }
}

}  // namespace quiver
