#ifndef QUIVER_DATABASE_INTERNAL_H
#define QUIVER_DATABASE_INTERNAL_H

#include "quiver/attribute_metadata.h"
#include "quiver/result.h"
#include "quiver/schema.h"

#include <optional>
#include <stdexcept>
#include <string>
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

// Template for reading grouped values (vectors or sets) for a single element by ID
template <typename T>
std::vector<T> read_grouped_values_by_id(const Result& result) {
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

// Convert a ColumnDefinition to ScalarMetadata
inline ScalarMetadata scalar_metadata_from_column(const ColumnDefinition& col) {
    return {col.name, col.type, col.not_null, col.primary_key, col.default_value};
}

}  // namespace quiver::internal

#endif  // QUIVER_DATABASE_INTERNAL_H
