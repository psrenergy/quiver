#include "quiver/c/database.h"

#include "database_helpers.h"
#include "internal.h"

#include <new>
#include <string>

extern "C" {

QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return quiver::default_database_options();
}

QUIVER_C_API quiver_error_t quiver_database_open(const char* path,
                                                 const quiver_database_options_t* options,
                                                 quiver_database_t** out_db) {
    QUIVER_REQUIRE(path, out_db);

    try {
        if (options) {
            *out_db = new quiver_database(path, *options);
        } else {
            auto default_options = quiver::default_database_options();
            *out_db = new quiver_database(path, default_options);
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_close(quiver_database_t* db) {
    delete db;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_is_healthy(quiver_database_t* db, int* out_healthy) {
    QUIVER_REQUIRE(db, out_healthy);

    *out_healthy = db->db.is_healthy() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_path(quiver_database_t* db, const char** out_path) {
    QUIVER_REQUIRE(db, out_path);

    *out_path = db->db.path().c_str();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_from_migrations(const char* db_path,
                                                            const char* migrations_path,
                                                            const quiver_database_options_t* options,
                                                            quiver_database_t** out_db) {
    QUIVER_REQUIRE(db_path, migrations_path, out_db);

    try {
        if (options) {
            auto db = quiver::Database::from_migrations(db_path, migrations_path, *options);
            *out_db = new quiver_database(std::move(db));
        } else {
            auto default_options = quiver::default_database_options();
            auto db = quiver::Database::from_migrations(db_path, migrations_path, default_options);
            *out_db = new quiver_database(std::move(db));
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_current_version(quiver_database_t* db, int64_t* out_version) {
    QUIVER_REQUIRE(db, out_version);

    try {
        *out_version = db->db.current_version();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_from_schema(const char* db_path,
                                                        const char* schema_path,
                                                        const quiver_database_options_t* options,
                                                        quiver_database_t** out_db) {
    QUIVER_REQUIRE(db_path, schema_path, out_db);

    try {
        if (options) {
            auto db = quiver::Database::from_schema(db_path, schema_path, *options);
            *out_db = new quiver_database(std::move(db));
        } else {
            auto default_options = quiver::default_database_options();
            auto db = quiver::Database::from_schema(db_path, schema_path, default_options);
            *out_db = new quiver_database(std::move(db));
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Update scalar functions

QUIVER_C_API quiver_error_t quiver_database_update_scalar_integer(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t value) {
    QUIVER_REQUIRE(db, collection, attribute);

    try {
        db->db.update_scalar_integer(collection, attribute, id, value);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_scalar_float(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double value) {
    QUIVER_REQUIRE(db, collection, attribute);

    try {
        db->db.update_scalar_float(collection, attribute, id, value);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_scalar_string(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const char* value) {
    QUIVER_REQUIRE(db, collection, attribute, value);

    try {
        db->db.update_scalar_string(collection, attribute, id, value);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Update vector functions

QUIVER_C_API quiver_error_t quiver_database_update_vector_integers(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   const int64_t* values,
                                                                   size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_vector_integers(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_floats(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const double* values,
                                                                 size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<double> vec(values, values + count);
        db->db.update_vector_floats(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_strings(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  const char* const* values,
                                                                  size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            if (!values[i]) {
                quiver_set_last_error("Null string pointer in values");
                return QUIVER_ERROR;
            }
            vec.emplace_back(values[i]);
        }
        db->db.update_vector_strings(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Update set functions

QUIVER_C_API quiver_error_t quiver_database_update_set_integers(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                const int64_t* values,
                                                                size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_set_integers(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_floats(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              const double* values,
                                                              size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<double> vec(values, values + count);
        db->db.update_set_floats(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_strings(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               int64_t id,
                                                               const char* const* values,
                                                               size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            if (!values[i]) {
                quiver_set_last_error("Null string pointer in values");
                return QUIVER_ERROR;
            }
            vec.emplace_back(values[i]);
        }
        db->db.update_set_strings(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Metadata functions

QUIVER_C_API quiver_error_t quiver_database_get_scalar_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                quiver_scalar_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, attribute, out_metadata);

    try {
        convert_scalar_to_c(db->db.get_scalar_metadata(collection, attribute), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_vector_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group_name,
                                                                quiver_group_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, group_name, out_metadata);

    try {
        convert_group_to_c(db->db.get_vector_metadata(collection, group_name), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_set_metadata(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* group_name,
                                                             quiver_group_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, group_name, out_metadata);

    try {
        convert_group_to_c(db->db.get_set_metadata(collection, group_name), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_free_scalar_metadata(quiver_scalar_metadata_t* metadata) {
    QUIVER_REQUIRE(metadata);

    free_scalar_fields(*metadata);
    metadata->name = nullptr;
    metadata->default_value = nullptr;
    metadata->references_collection = nullptr;
    metadata->references_column = nullptr;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_free_group_metadata(quiver_group_metadata_t* metadata) {
    QUIVER_REQUIRE(metadata);

    free_group_fields(*metadata);
    metadata->group_name = nullptr;
    metadata->dimension_column = nullptr;
    metadata->value_columns = nullptr;
    metadata->value_column_count = 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_list_scalar_attributes(quiver_database_t* db,
                                                                   const char* collection,
                                                                   quiver_scalar_metadata_t** out_metadata,
                                                                   size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto attributes = db->db.list_scalar_attributes(collection);
        *out_count = attributes.size();
        if (attributes.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_scalar_metadata_t[attributes.size()];
        for (size_t i = 0; i < attributes.size(); ++i) {
            convert_scalar_to_c(attributes[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_vector_groups(quiver_database_t* db,
                                                               const char* collection,
                                                               quiver_group_metadata_t** out_metadata,
                                                               size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto groups = db->db.list_vector_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_group_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            convert_group_to_c(groups[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_set_groups(quiver_database_t* db,
                                                            const char* collection,
                                                            quiver_group_metadata_t** out_metadata,
                                                            size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto groups = db->db.list_set_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_group_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            convert_group_to_c(groups[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_free_scalar_metadata_array(quiver_scalar_metadata_t* metadata, size_t count) {
    QUIVER_REQUIRE(metadata);

    for (size_t i = 0; i < count; ++i) {
        free_scalar_fields(metadata[i]);
    }
    delete[] metadata;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_free_group_metadata_array(quiver_group_metadata_t* metadata, size_t count) {
    QUIVER_REQUIRE(metadata);

    for (size_t i = 0; i < count; ++i) {
        free_group_fields(metadata[i]);
    }
    delete[] metadata;
    return QUIVER_OK;
}

// CSV operations

QUIVER_C_API quiver_error_t quiver_database_export_csv(quiver_database_t* db, const char* table, const char* path) {
    QUIVER_REQUIRE(db, table, path);

    try {
        db->db.export_csv(table, path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_import_csv(quiver_database_t* db, const char* table, const char* path) {
    QUIVER_REQUIRE(db, table, path);

    try {
        db->db.import_csv(table, path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Query functions

QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         char** out_value,
                                                         int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);

    try {
        auto result = db->db.query_string(sql);
        if (result.has_value()) {
            *out_value = strdup_safe(*result);
            *out_has_value = 1;
        } else {
            *out_value = nullptr;
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_integer(quiver_database_t* db,
                                                          const char* sql,
                                                          int64_t* out_value,
                                                          int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);

    try {
        auto result = db->db.query_integer(sql);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_float(quiver_database_t* db,
                                                        const char* sql,
                                                        double* out_value,
                                                        int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);

    try {
        auto result = db->db.query_float(sql);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Helper to convert C param arrays to std::vector<Value>
static std::vector<quiver::Value>
convert_params(const int* param_types, const void* const* param_values, size_t param_count) {
    std::vector<quiver::Value> params;
    params.reserve(param_count);
    for (size_t i = 0; i < param_count; ++i) {
        switch (param_types[i]) {
        case QUIVER_DATA_TYPE_INTEGER:
            params.emplace_back(*static_cast<const int64_t*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_FLOAT:
            params.emplace_back(*static_cast<const double*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_STRING:
            if (!param_values[i]) {
                throw std::runtime_error("Null string pointer in parameter at index " + std::to_string(i));
            }
            params.emplace_back(std::string(static_cast<const char*>(param_values[i])));
            break;
        case QUIVER_DATA_TYPE_NULL:
            params.emplace_back(nullptr);
            break;
        default:
            throw std::runtime_error("Unknown parameter type: " + std::to_string(param_types[i]));
        }
    }
    return params;
}

QUIVER_C_API quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
                                                                const char* sql,
                                                                const int* param_types,
                                                                const void* const* param_values,
                                                                size_t param_count,
                                                                char** out_value,
                                                                int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);

    if (param_count > 0 && (!param_types || !param_values)) {
        quiver_set_last_error("Null param_types or param_values with non-zero param_count");
        return QUIVER_ERROR;
    }
    try {
        auto params = convert_params(param_types, param_values, param_count);
        auto result = db->db.query_string(sql, params);
        if (result.has_value()) {
            *out_value = strdup_safe(*result);
            *out_has_value = 1;
        } else {
            *out_value = nullptr;
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_integer_params(quiver_database_t* db,
                                                                 const char* sql,
                                                                 const int* param_types,
                                                                 const void* const* param_values,
                                                                 size_t param_count,
                                                                 int64_t* out_value,
                                                                 int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);

    if (param_count > 0 && (!param_types || !param_values)) {
        quiver_set_last_error("Null param_types or param_values with non-zero param_count");
        return QUIVER_ERROR;
    }

    try {
        auto params = convert_params(param_types, param_values, param_count);
        auto result = db->db.query_integer(sql, params);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_float_params(quiver_database_t* db,
                                                               const char* sql,
                                                               const int* param_types,
                                                               const void* const* param_values,
                                                               size_t param_count,
                                                               double* out_value,
                                                               int* out_has_value) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);

    if (param_count > 0 && (!param_types || !param_values)) {
        quiver_set_last_error("Null param_types or param_values with non-zero param_count");
        return QUIVER_ERROR;
    }
    try {
        auto params = convert_params(param_types, param_values, param_count);
        auto result = db->db.query_float(sql, params);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Schema inspection

QUIVER_C_API quiver_error_t quiver_database_describe(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.describe();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Time series metadata and operations

QUIVER_C_API quiver_error_t quiver_database_get_time_series_metadata(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* group_name,
                                                                     quiver_group_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, group_name, out_metadata);

    try {
        convert_group_to_c(db->db.get_time_series_metadata(collection, group_name), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_time_series_groups(quiver_database_t* db,
                                                                    const char* collection,
                                                                    quiver_group_metadata_t** out_metadata,
                                                                    size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto groups = db->db.list_time_series_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_group_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            convert_group_to_c(groups[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_time_series_group(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* group,
                                                                   int64_t id,
                                                                   char*** out_date_times,
                                                                   double** out_values,
                                                                   size_t* out_row_count) {
    QUIVER_REQUIRE(db, collection, group, out_date_times, out_values, out_row_count);

    try {
        auto metadata = db->db.get_time_series_metadata(collection, group);
        const auto& dim_col = metadata.dimension_column;
        auto val_col = metadata.value_columns.empty() ? "value" : metadata.value_columns[0].name;

        auto rows = db->db.read_time_series_group(collection, group, id);
        *out_row_count = rows.size();

        if (rows.empty()) {
            *out_date_times = nullptr;
            *out_values = nullptr;
            return QUIVER_OK;
        }

        *out_date_times = new char*[rows.size()];
        *out_values = new double[rows.size()];

        for (size_t i = 0; i < rows.size(); ++i) {
            // Get dimension column
            auto dt_it = rows[i].find(dim_col);
            if (dt_it != rows[i].end() && std::holds_alternative<std::string>(dt_it->second)) {
                (*out_date_times)[i] = strdup_safe(std::get<std::string>(dt_it->second));
            } else {
                (*out_date_times)[i] = strdup_safe("");
            }

            // Get value column
            auto val_it = rows[i].find(val_col);
            if (val_it != rows[i].end()) {
                if (std::holds_alternative<double>(val_it->second)) {
                    (*out_values)[i] = std::get<double>(val_it->second);
                } else if (std::holds_alternative<int64_t>(val_it->second)) {
                    (*out_values)[i] = static_cast<double>(std::get<int64_t>(val_it->second));
                } else {
                    (*out_values)[i] = 0.0;
                }
            } else {
                (*out_values)[i] = 0.0;
            }
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_time_series_group(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* group,
                                                                     int64_t id,
                                                                     const char* const* date_times,
                                                                     const double* values,
                                                                     size_t row_count) {
    QUIVER_REQUIRE(db, collection, group);

    if (row_count > 0 && (!date_times || !values)) {
        quiver_set_last_error("Null date_times or values with non-zero row_count");
        return QUIVER_ERROR;
    }

    try {
        auto metadata = db->db.get_time_series_metadata(collection, group);
        const auto& dim_col = metadata.dimension_column;
        auto val_col = metadata.value_columns.empty() ? "value" : metadata.value_columns[0].name;

        std::vector<std::map<std::string, quiver::Value>> rows;
        rows.reserve(row_count);

        for (size_t i = 0; i < row_count; ++i) {
            std::map<std::string, quiver::Value> row;
            row[dim_col] = std::string(date_times[i]);
            row[val_col] = values[i];
            rows.push_back(std::move(row));
        }

        db->db.update_time_series_group(collection, group, id, rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_free_time_series_data(char** date_times, double* values, size_t row_count) {
    QUIVER_REQUIRE(values, date_times);

    if (date_times) {
        for (size_t i = 0; i < row_count; ++i) {
            delete[] date_times[i];
        }
        delete[] date_times;
    }
    delete[] values;
    return QUIVER_OK;
}

// Time series files operations

QUIVER_C_API quiver_error_t quiver_database_has_time_series_files(quiver_database_t* db,
                                                                  const char* collection,
                                                                  int* out_result) {
    QUIVER_REQUIRE(db, collection, out_result);

    try {
        *out_result = db->db.has_time_series_files(collection) ? 1 : 0;
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_time_series_files_columns(quiver_database_t* db,
                                                                           const char* collection,
                                                                           char*** out_columns,
                                                                           size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_columns, out_count);

    try {
        auto columns = db->db.list_time_series_files_columns(collection);
        return copy_strings_to_c(columns, out_columns, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_time_series_files(quiver_database_t* db,
                                                                   const char* collection,
                                                                   char*** out_columns,
                                                                   char*** out_paths,
                                                                   size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_columns, out_paths, out_count);

    try {
        auto paths_map = db->db.read_time_series_files(collection);
        *out_count = paths_map.size();

        if (paths_map.empty()) {
            *out_columns = nullptr;
            *out_paths = nullptr;
            return QUIVER_OK;
        }

        *out_columns = new char*[paths_map.size()];
        *out_paths = new char*[paths_map.size()];

        size_t i = 0;
        for (const auto& [col_name, path] : paths_map) {
            (*out_columns)[i] = strdup_safe(col_name);
            if (path) {
                (*out_paths)[i] = strdup_safe(*path);
            } else {
                (*out_paths)[i] = nullptr;
            }
            ++i;
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_time_series_files(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* const* columns,
                                                                     const char* const* paths,
                                                                     size_t count) {
    QUIVER_REQUIRE(db, collection);

    if (count > 0 && (!columns || !paths)) {
        quiver_set_last_error("Null columns or paths with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::map<std::string, std::optional<std::string>> paths_map;
        for (size_t i = 0; i < count; ++i) {
            if (paths[i]) {
                paths_map[columns[i]] = std::string(paths[i]);
            } else {
                paths_map[columns[i]] = std::nullopt;
            }
        }

        db->db.update_time_series_files(collection, paths_map);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_free_time_series_files(char** columns, char** paths, size_t count) {
    QUIVER_REQUIRE(columns, paths);

    for (size_t i = 0; i < count; ++i) {
        delete[] columns[i];
    }
    delete[] columns;

    for (size_t i = 0; i < count; ++i) {
        delete[] paths[i];
    }
    delete[] paths;
    return QUIVER_OK;
}

}  // extern "C"
