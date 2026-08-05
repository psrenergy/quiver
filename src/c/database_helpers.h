#ifndef QUIVER_C_DATABASE_HELPERS_H
#define QUIVER_C_DATABASE_HELPERS_H

#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/database.h"
#include "utils/string.h"

#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Helper template for reading numeric scalars
template <typename T>
quiver_error_t read_scalars_impl(const std::vector<T>& values, T** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new T[values.size()];
    std::copy(values.begin(), values.end(), *out_values);
    return QUIVER_OK;
}

// Helper for reading nullable numeric scalars into a value array + parallel presence mask.
// mask[i] == 0 means SQL NULL; the data slot then holds a placeholder (0 / 0.0) to be ignored.
template <typename T>
quiver_error_t read_scalars_masked_impl(const std::vector<std::optional<T>>& values,
                                        T** out_values,
                                        uint8_t** out_mask,
                                        size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        *out_mask = nullptr;
        return QUIVER_OK;
    }
    *out_values = new T[values.size()];
    *out_mask = new uint8_t[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i]) {
            (*out_values)[i] = *values[i];
            (*out_mask)[i] = 1;
        } else {
            (*out_values)[i] = T{};
            (*out_mask)[i] = 0;
        }
    }
    return QUIVER_OK;
}

// Helper template for reading numeric vectors
template <typename T>
quiver_error_t
read_vectors_impl(const std::vector<std::vector<T>>& vectors, T*** out_vectors, size_t** out_sizes, size_t* out_count) {
    *out_count = vectors.size();
    if (vectors.empty()) {
        *out_vectors = nullptr;
        *out_sizes = nullptr;
        return QUIVER_OK;
    }
    *out_vectors = new T*[vectors.size()];
    *out_sizes = new size_t[vectors.size()];
    for (size_t i = 0; i < vectors.size(); ++i) {
        (*out_sizes)[i] = vectors[i].size();
        if (vectors[i].empty()) {
            (*out_vectors)[i] = nullptr;
        } else {
            (*out_vectors)[i] = new T[vectors[i].size()];
            std::copy(vectors[i].begin(), vectors[i].end(), (*out_vectors)[i]);
        }
    }
    return QUIVER_OK;
}

// Helper template for freeing numeric vectors
template <typename T>
void free_vectors_impl(T** vectors, size_t* sizes, size_t count) {
    if (vectors) {
        for (size_t i = 0; i < count; ++i) {
            delete[] vectors[i];
        }
        delete[] vectors;
    }
    delete[] sizes;
}

// Helper to copy nested string vectors (vector/set reads) to C arrays
inline void copy_string_vectors_to_c(const std::vector<std::vector<std::string>>& vectors,
                                     char**** out_vectors,
                                     size_t** out_sizes,
                                     size_t* out_count) {
    *out_count = vectors.size();
    if (vectors.empty()) {
        *out_vectors = nullptr;
        *out_sizes = nullptr;
        return;
    }
    *out_vectors = new char**[vectors.size()];
    *out_sizes = new size_t[vectors.size()];
    for (size_t i = 0; i < vectors.size(); ++i) {
        (*out_sizes)[i] = vectors[i].size();
        if (vectors[i].empty()) {
            (*out_vectors)[i] = nullptr;
        } else {
            (*out_vectors)[i] = new char*[vectors[i].size()];
            for (size_t j = 0; j < vectors[i].size(); ++j) {
                (*out_vectors)[i][j] = quiver::string::new_c_str(vectors[i][j]);
            }
        }
    }
}

// Helper to copy a vector of strings to C-style array
inline quiver_error_t copy_strings_to_c(const std::vector<std::string>& values, char*** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new char*[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        (*out_values)[i] = quiver::string::new_c_str(values[i]);
    }
    return QUIVER_OK;
}

// Overload for nullable scalar strings: a SQL NULL becomes a nullptr entry in the array
// (no mask needed — a NULL char* is unambiguous). free_string_array tolerates nullptr slots.
inline quiver_error_t
copy_strings_to_c(const std::vector<std::optional<std::string>>& values, char*** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return QUIVER_OK;
    }
    *out_values = new char*[values.size()];
    for (size_t i = 0; i < values.size(); ++i) {
        (*out_values)[i] = values[i] ? quiver::string::new_c_str(*values[i]) : nullptr;
    }
    return QUIVER_OK;
}

// Helper to convert C++ DataType to C quiver_data_type_t
inline quiver_data_type_t to_c_data_type(quiver::DataType type) {
    switch (type) {
    case quiver::DataType::Integer:
        return QUIVER_DATA_TYPE_INTEGER;
    case quiver::DataType::Real:
        return QUIVER_DATA_TYPE_FLOAT;
    case quiver::DataType::Text:
        return QUIVER_DATA_TYPE_STRING;
    case quiver::DataType::DateTime:
        return QUIVER_DATA_TYPE_DATE_TIME;
    default:
        throw std::runtime_error("Cannot to_c_data_type: unknown data type " + std::to_string(static_cast<int>(type)));
    }
}

inline void convert_scalar_to_c(const quiver::ScalarMetadata& src, quiver_scalar_metadata_t& dst) {
    dst.name = quiver::string::new_c_str(src.name);
    dst.data_type = to_c_data_type(src.data_type);
    dst.not_null = src.not_null ? 1 : 0;
    dst.primary_key = src.primary_key ? 1 : 0;
    dst.default_value = src.default_value.has_value() ? quiver::string::new_c_str(*src.default_value) : nullptr;
    dst.is_foreign_key = src.is_foreign_key ? 1 : 0;
    dst.references_collection =
        src.references_collection.has_value() ? quiver::string::new_c_str(*src.references_collection) : nullptr;
    dst.references_column =
        src.references_column.has_value() ? quiver::string::new_c_str(*src.references_column) : nullptr;
}

inline void free_scalar_fields(quiver_scalar_metadata_t& m) {
    delete[] m.name;
    delete[] m.default_value;
    delete[] m.references_collection;
    delete[] m.references_column;
}

inline void convert_group_to_c(const quiver::GroupMetadata& src, quiver_group_metadata_t& dst) {
    dst.group_name = quiver::string::new_c_str(src.group_name);
    dst.dimension_column = src.dimension_column.empty() ? nullptr : quiver::string::new_c_str(src.dimension_column);
    dst.value_column_count = src.value_columns.size();
    if (src.value_columns.empty()) {
        dst.value_columns = nullptr;
    } else {
        dst.value_columns = new quiver_scalar_metadata_t[src.value_columns.size()];
        for (size_t i = 0; i < src.value_columns.size(); ++i) {
            convert_scalar_to_c(src.value_columns[i], dst.value_columns[i]);
        }
    }
}

inline void free_group_fields(quiver_group_metadata_t& m) {
    delete[] m.group_name;
    delete[] m.dimension_column;
    if (m.value_columns) {
        for (size_t i = 0; i < m.value_column_count; ++i) {
            free_scalar_fields(m.value_columns[i]);
        }
        delete[] m.value_columns;
    }
}

// Decodes the columnar typed-arrays + per-cell mask form into the row-shaped data the C++ core
// takes - the inverse of marshal_group_rows_to_c, shared by every group update C function
// (time series, vector, set). A NULL mask (for the whole parameter or for one column) means dense,
// and a NULL cell becomes an explicit Value{nullptr} in every row so rows stay uniform (the core
// builds its INSERT column list from rows[0]).
inline std::vector<std::map<std::string, quiver::Value>>
unmarshal_group_columns_to_rows(const char* caller,
                                const char* const* column_names,
                                const int* column_types,
                                const void* const* column_data,
                                const uint8_t* const* column_has_value,
                                size_t column_count,
                                size_t row_count) {
    // Named columns with no rows are a caller mistake, and the last layer that can say so: the
    // row-shaped result below carries no column names at all, so the core would see an empty
    // update and clear the group - a typo'd column name silently destroying data. Clearing is
    // spelled column_count == 0 (the same anti-silent-clear trap Lua and JS already enforce).
    if (column_count > 0 && row_count == 0) {
        std::string names;
        for (size_t c = 0; c < column_count; ++c) {
            names += (names.empty() ? "" : ", ") + std::string(column_names[c]);
        }
        throw std::runtime_error(std::string("Cannot ") + caller + ": columns [" + names +
                                 "] contain no rows; pass no columns to clear the group");
    }

    std::vector<std::map<std::string, quiver::Value>> rows;
    rows.reserve(row_count);

    for (size_t r = 0; r < row_count; ++r) {
        std::map<std::string, quiver::Value> row;
        for (size_t c = 0; c < column_count; ++c) {
            std::string col_name(column_names[c]);
            // A masked-out cell, an absent column array, or (for strings) a NULL entry are all
            // SQL NULL - the same contract the element array setters document. The read direction
            // emits exactly that for a NULL STRING cell, so a caller round-tripping a read result
            // with the mask stripped must not land in undefined behaviour.
            const uint8_t* mask = column_has_value ? column_has_value[c] : nullptr;
            if ((mask && mask[r] == 0) || !column_data[c]) {
                row[col_name] = nullptr;
                continue;
            }
            switch (column_types[c]) {
            case QUIVER_DATA_TYPE_INTEGER:
                row[col_name] = static_cast<const int64_t*>(column_data[c])[r];
                break;
            case QUIVER_DATA_TYPE_FLOAT:
                row[col_name] = static_cast<const double*>(column_data[c])[r];
                break;
            case QUIVER_DATA_TYPE_STRING:
            case QUIVER_DATA_TYPE_DATE_TIME: {
                const char* cell = static_cast<const char* const*>(column_data[c])[r];
                row[col_name] = cell ? quiver::Value(std::string(cell)) : quiver::Value(nullptr);
                break;
            }
            default:
                throw std::runtime_error(std::string("Cannot ") + caller + ": unknown column type " +
                                         std::to_string(column_types[c]));
            }
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

// Decodes the single-row typed-pointer form into the row-shaped map the C++ core takes, shared by
// quiver_database_upsert_time_series_row and its _by_label counterpart. Each column_data[c]
// points at a single value, so there is no mask and no row_count - a cell is either supplied or
// its column is simply absent (the core leaves the column at its DEFAULT).
inline std::map<std::string, quiver::Value> unmarshal_single_row(const char* caller,
                                                                 const char* const* column_names,
                                                                 const int* column_types,
                                                                 const void* const* column_data,
                                                                 size_t column_count) {
    std::map<std::string, quiver::Value> row;
    for (size_t c = 0; c < column_count; ++c) {
        std::string col_name(column_names[c]);
        switch (column_types[c]) {
        case QUIVER_DATA_TYPE_INTEGER:
            row[col_name] = static_cast<const int64_t*>(column_data[c])[0];
            break;
        case QUIVER_DATA_TYPE_FLOAT:
            row[col_name] = static_cast<const double*>(column_data[c])[0];
            break;
        case QUIVER_DATA_TYPE_STRING:
        case QUIVER_DATA_TYPE_DATE_TIME:
            row[col_name] = std::string(static_cast<const char* const*>(column_data[c])[0]);
            break;
        default:
            throw std::runtime_error(std::string("Cannot ") + caller + ": unknown column type " +
                                     std::to_string(column_types[c]));
        }
    }
    return row;
}

// Marshals row-shaped group data (time series / vector / set) into the columnar
// typed-arrays + per-cell presence-mask out-params shared by the group read C
// functions. `columns` pairs each column name with its quiver_data_type_t value.
// Results are freed by quiver_database_free_time_series_data.
inline void marshal_group_rows_to_c(const char* caller,
                                    const std::vector<std::pair<std::string, int>>& columns,
                                    const std::vector<std::map<std::string, quiver::Value>>& rows,
                                    char*** out_column_names,
                                    int** out_column_types,
                                    void*** out_column_data,
                                    uint8_t*** out_column_has_value,
                                    size_t* out_column_count,
                                    size_t* out_row_count) {
    if (rows.empty()) {
        *out_column_names = nullptr;
        *out_column_types = nullptr;
        *out_column_data = nullptr;
        *out_column_has_value = nullptr;
        *out_column_count = 0;
        *out_row_count = 0;
        return;
    }

    size_t col_count = columns.size();
    size_t row_count = rows.size();

    // Allocate outer arrays, value-initialized to nullptr so the cleanup
    // path below can safely free a partially-marshaled result
    *out_column_names = new char*[col_count]();
    *out_column_types = new int[col_count]();
    *out_column_data = new void*[col_count]();
    *out_column_has_value = new uint8_t*[col_count]();

    try {
        for (size_t c = 0; c < col_count; ++c) {
            (*out_column_names)[c] = quiver::string::new_c_str(columns[c].first);
            (*out_column_types)[c] = columns[c].second;

            // Allocate the mask before the type switch so the unknown-type
            // default branch leaves a freeable slot behind
            auto* mask = new uint8_t[row_count];
            (*out_column_has_value)[c] = mask;

            switch (columns[c].second) {
            case QUIVER_DATA_TYPE_INTEGER: {
                auto* arr = new int64_t[row_count];
                (*out_column_data)[c] = arr;
                for (size_t r = 0; r < row_count; ++r) {
                    auto& val = rows[r].at(columns[c].first);
                    if (std::holds_alternative<int64_t>(val)) {
                        arr[r] = std::get<int64_t>(val);
                        mask[r] = 1;
                    } else if (std::holds_alternative<double>(val)) {
                        arr[r] = static_cast<int64_t>(std::get<double>(val));
                        mask[r] = 1;
                    } else {
                        arr[r] = 0;
                        mask[r] = 0;
                    }
                }
                break;
            }
            case QUIVER_DATA_TYPE_FLOAT: {
                auto* arr = new double[row_count];
                (*out_column_data)[c] = arr;
                for (size_t r = 0; r < row_count; ++r) {
                    auto& val = rows[r].at(columns[c].first);
                    if (std::holds_alternative<double>(val)) {
                        arr[r] = std::get<double>(val);
                        mask[r] = 1;
                    } else if (std::holds_alternative<int64_t>(val)) {
                        arr[r] = static_cast<double>(std::get<int64_t>(val));
                        mask[r] = 1;
                    } else {
                        arr[r] = 0.0;
                        mask[r] = 0;
                    }
                }
                break;
            }
            case QUIVER_DATA_TYPE_STRING:
            case QUIVER_DATA_TYPE_DATE_TIME: {
                auto** arr = new char*[row_count]();
                (*out_column_data)[c] = arr;
                for (size_t r = 0; r < row_count; ++r) {
                    auto& val = rows[r].at(columns[c].first);
                    if (std::holds_alternative<std::string>(val)) {
                        arr[r] = quiver::string::new_c_str(std::get<std::string>(val));
                        mask[r] = 1;
                    } else {
                        arr[r] = nullptr;
                        mask[r] = 0;
                    }
                }
                break;
            }
            default:
                throw std::runtime_error(std::string("Cannot ") + caller + ": unknown data type " +
                                         std::to_string(columns[c].second));
            }
        }
    } catch (...) {
        // Clean up partially allocated results
        quiver_database_free_time_series_data(
            *out_column_names, *out_column_types, *out_column_data, *out_column_has_value, col_count, row_count);
        *out_column_names = nullptr;
        *out_column_types = nullptr;
        *out_column_data = nullptr;
        *out_column_has_value = nullptr;
        *out_column_count = 0;
        *out_row_count = 0;
        throw;
    }

    *out_column_count = col_count;
    *out_row_count = row_count;
}

#endif  // QUIVER_C_DATABASE_HELPERS_H
