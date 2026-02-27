#ifndef QUIVER_C_DATABASE_HELPERS_H
#define QUIVER_C_DATABASE_HELPERS_H

#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/database.h"

#include <cstring>
#include <string>
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
quiver_error_t free_vectors_impl(T** vectors, size_t* sizes, size_t count) {
    (void)sizes;  // unused for numeric types
    if (!vectors)
        return QUIVER_OK;
    for (size_t i = 0; i < count; ++i) {
        delete[] vectors[i];
    }
    delete[] vectors;
    delete[] sizes;
    return QUIVER_OK;
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
        (*out_values)[i] = new char[values[i].size() + 1];
        std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
        (*out_values)[i][values[i].size()] = '\0';
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

inline char* strdup_safe(const std::string& str) {
    auto result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}

inline void convert_scalar_to_c(const quiver::ScalarMetadata& src, quiver_scalar_metadata_t& dst) {
    dst.name = strdup_safe(src.name);
    dst.data_type = to_c_data_type(src.data_type);
    dst.not_null = src.not_null ? 1 : 0;
    dst.primary_key = src.primary_key ? 1 : 0;
    dst.default_value = src.default_value.has_value() ? strdup_safe(*src.default_value) : nullptr;
    dst.is_foreign_key = src.is_foreign_key ? 1 : 0;
    dst.references_collection =
        src.references_collection.has_value() ? strdup_safe(*src.references_collection) : nullptr;
    dst.references_column = src.references_column.has_value() ? strdup_safe(*src.references_column) : nullptr;
}

inline void free_scalar_fields(quiver_scalar_metadata_t& m) {
    delete[] m.name;
    delete[] m.default_value;
    delete[] m.references_collection;
    delete[] m.references_column;
}

inline void convert_group_to_c(const quiver::GroupMetadata& src, quiver_group_metadata_t& dst) {
    dst.group_name = strdup_safe(src.group_name);
    dst.dimension_column = src.dimension_column.empty() ? nullptr : strdup_safe(src.dimension_column);
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

#endif  // QUIVER_C_DATABASE_HELPERS_H
