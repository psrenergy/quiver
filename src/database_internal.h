#ifndef QUIVER_DATABASE_INTERNAL_H
#define QUIVER_DATABASE_INTERNAL_H

#include "quiver/attribute_metadata.h"
#include "quiver/result.h"
#include "quiver/schema.h"
#include "quiver/value.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace quiver::internal {

// Type-specific Row value extractors for template implementations
inline std::optional<int64_t> get_row_value(const Row& row, size_t index, int64_t*) {
    return row.get_integer(index);
}

inline std::optional<double> get_row_value(const Row& row, size_t index, double*) {
    return row.get_float(index);
}

inline std::optional<std::string> get_row_value(const Row& row, size_t index, std::string*) {
    return row.get_string(index);
}

// Template for reading grouped values (vectors or sets) for all elements
template <typename T>
std::vector<std::vector<T>> read_grouped_values_all(const Result& result) {
    std::vector<std::vector<T>> groups;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_integer(0);
        auto val = get_row_value(result[i], 1, static_cast<T*>(nullptr));

        if (!id)
            continue;

        if (*id != current_id) {
            groups.emplace_back();
            current_id = *id;
        }

        if (val) {
            groups.back().push_back(*val);
        }
    }
    return groups;
}

// Template for reading column 0 values from query results.
// Drops NULLs — used by group readers (vector/set by id) and read_element_ids,
// where the columns are NOT NULL / PK by schema convention so no NULL ever appears.
template <typename T>
std::vector<T> read_column_values(const Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = get_row_value(result[i], 0, static_cast<T*>(nullptr));
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

// Template for reading column 0 values, preserving NULLs as std::nullopt.
// One entry per result row (positional) — used only by the scalar bulk readers,
// where ORDER BY rowid alignment with the element list must be preserved.
template <typename T>
std::vector<std::optional<T>> read_column_values_nullable(const Result& result) {
    std::vector<std::optional<T>> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        values.push_back(get_row_value(result[i], 0, static_cast<T*>(nullptr)));
    }
    return values;
}

// Template for reading a single optional value (column 0, row 0) from query results
template <typename T>
std::optional<T> read_single_value(const Result& result) {
    if (result.empty()) {
        return std::nullopt;
    }
    return get_row_value(result[0], 0, static_cast<T*>(nullptr));
}

// Find the dimension/ordering column in a time series table
inline std::string find_dimension_column(const TableDefinition& table_def) {
    for (const auto& [col_name, col] : table_def.columns) {
        if (col_name == "id")
            continue;
        if (col.type == DataType::DateTime || is_date_time_column(col_name)) {
            return col_name;
        }
    }
    throw std::runtime_error("Dimension column not found: time series table '" + table_def.name + "'");
}

// Find all dimension columns for a time series table: every PK column except
// "id", returned in declaration order (column_order is populated from
// PRAGMA table_info, which reports columns in declaration order). Throws if
// the table has no dimension columns — mirrors find_dimension_column's
// contract so callers don't repeat the empty check.
inline std::vector<std::string> find_dimension_columns(const TableDefinition& table_def) {
    std::vector<std::string> dim_cols;
    for (const auto& col_name : table_def.column_order) {
        if (col_name == "id") {
            continue;
        }
        const auto* col = table_def.get_column(col_name);
        if (col && col->primary_key) {
            dim_cols.push_back(col_name);
        }
    }
    if (dim_cols.empty()) {
        throw std::runtime_error("Dimension column not found: time series table '" + table_def.name + "'");
    }
    return dim_cols;
}

// True if the Value variant holds the data type expected by a schema column.
// NULL values match any column type. Integer values are also accepted for
// REAL columns (SQLite STRICT converts them to real on insert), so callers in
// any binding can write whole numbers into float columns.
inline bool value_matches_type(const Value& v, DataType expected) {
    return std::visit(
        [expected](const auto& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, std::nullptr_t>)
                return true;
            else if constexpr (std::is_same_v<T, int64_t>)
                return expected == DataType::Integer || expected == DataType::Real;
            else if constexpr (std::is_same_v<T, double>)
                return expected == DataType::Real;
            else
                return expected == DataType::Text || expected == DataType::DateTime;
        },
        v);
}

// Human-readable name of the type currently held in a Value (for error messages).
// Precondition: only called when value_matches_type returned false, so v never holds nullptr_t.
inline const char* value_type_name(const Value& v) {
    return std::visit(
        [](const auto& x) -> const char* {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, int64_t>)
                return "INTEGER";
            else if constexpr (std::is_same_v<T, double>)
                return "REAL";
            else if constexpr (std::is_same_v<T, std::string>)
                return "TEXT";
            else
                return "NULL";
        },
        v);
}

// Convert a ColumnDefinition to ScalarMetadata
inline ScalarMetadata scalar_metadata_from_column(const ColumnDefinition& col) {
    ScalarMetadata meta;
    meta.name = col.name;
    meta.data_type = col.type;
    // An INTEGER PRIMARY KEY is a rowid alias and is never NULL, even though SQLite's
    // PRAGMA table_info reports notnull=0 for it. Report it as not_null for the public API.
    // (The raw ColumnDefinition.not_null stays the PRAGMA value for csv_import / schema_validator.)
    meta.not_null = col.not_null || (col.primary_key && col.type == DataType::Integer);
    meta.primary_key = col.primary_key;
    meta.default_value = col.default_value;
    return meta;
}

}  // namespace quiver::internal

#endif  // QUIVER_DATABASE_INTERNAL_H
