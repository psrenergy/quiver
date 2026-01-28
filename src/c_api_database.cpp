#include "c_api_internal.h"
#include "quiver/c/database.h"
#include "quiver/c/element.h"

#include <new>
#include <string>

namespace {

quiver::LogLevel to_cpp_log_level(quiver_log_level_t level) {
    switch (level) {
    case QUIVER_LOG_DEBUG:
        return quiver::LogLevel::debug;
    case QUIVER_LOG_INFO:
        return quiver::LogLevel::info;
    case QUIVER_LOG_WARN:
        return quiver::LogLevel::warn;
    case QUIVER_LOG_ERROR:
        return quiver::LogLevel::error;
    case QUIVER_LOG_OFF:
        return quiver::LogLevel::off;
    default:
        return quiver::LogLevel::info;
    }
}

quiver::DatabaseOptions to_cpp_options(const quiver_database_options_t* options) {
    quiver::DatabaseOptions cpp_options;
    if (options) {
        cpp_options.read_only = options->read_only != 0;
        cpp_options.console_level = to_cpp_log_level(options->console_level);
    }
    return cpp_options;
}

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
void free_vectors_impl(T** vectors, size_t* sizes, size_t count) {
    (void)sizes;  // unused for numeric types
    if (!vectors)
        return;
    for (size_t i = 0; i < count; ++i) {
        delete[] vectors[i];
    }
    delete[] vectors;
    delete[] sizes;
}

// Helper to copy a vector of strings to C-style array
quiver_error_t copy_strings_to_c(const std::vector<std::string>& values, char*** out_values, size_t* out_count) {
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

}  // namespace

extern "C" {

QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    quiver_database_options_t options;
    options.read_only = 0;
    options.console_level = QUIVER_LOG_INFO;
    return options;
}

QUIVER_C_API quiver_database_t* quiver_database_open(const char* path, const quiver_database_options_t* options) {
    if (!path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        return new quiver_database(path, cpp_options);
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return nullptr;
    }
}

QUIVER_C_API void quiver_database_close(quiver_database_t* db) {
    delete db;
}

QUIVER_C_API int quiver_database_is_healthy(quiver_database_t* db) {
    if (!db) {
        return 0;
    }
    return db->db.is_healthy() ? 1 : 0;
}

QUIVER_C_API const char* quiver_database_path(quiver_database_t* db) {
    if (!db) {
        return nullptr;
    }
    return db->db.path().c_str();
}

QUIVER_C_API quiver_database_t* quiver_database_from_migrations(const char* db_path,
                                                                const char* migrations_path,
                                                                const quiver_database_options_t* options) {
    if (!db_path || !migrations_path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        auto db = quiver::Database::from_migrations(db_path, migrations_path, cpp_options);
        return new quiver_database(std::move(db));
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return nullptr;
    }
}

QUIVER_C_API int64_t quiver_database_current_version(quiver_database_t* db) {
    if (!db) {
        return -1;
    }
    try {
        return db->db.current_version();
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return -1;
    }
}

QUIVER_C_API int64_t quiver_database_create_element(quiver_database_t* db,
                                                    const char* collection,
                                                    quiver_element_t* element) {
    if (!db || !collection || !element) {
        return -1;
    }
    try {
        return db->db.create_element(collection, element->element);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return -1;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id,
                                                           const quiver_element_t* element) {
    if (!db || !collection || !element) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_element(collection, id, element->element);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_delete_element_by_id(quiver_database_t* db,
                                                                 const char* collection,
                                                                 int64_t id) {
    if (!db || !collection) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.delete_element_by_id(collection, id);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_set_scalar_relation(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                const char* from_label,
                                                                const char* to_label) {
    if (!db || !collection || !attribute || !from_label || !to_label) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.set_scalar_relation(collection, attribute, from_label, to_label);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_relation(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 char*** out_values,
                                                                 size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return copy_strings_to_c(db->db.read_scalar_relation(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_database_t*
quiver_database_from_schema(const char* db_path, const char* schema_path, const quiver_database_options_t* options) {
    if (!db_path || !schema_path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        auto db = quiver::Database::from_schema(db_path, schema_path, cpp_options);
        return new quiver_database(std::move(db));
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return nullptr;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t** out_values,
                                                                 size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_scalars_impl(db->db.read_scalar_integers(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_floats(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               double** out_values,
                                                               size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_scalars_impl(db->db.read_scalar_floats(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_strings(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                char*** out_values,
                                                                size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return copy_strings_to_c(db->db.read_scalar_strings(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API void quiver_free_integer_array(int64_t* values) {
    delete[] values;
}

QUIVER_C_API void quiver_free_float_array(double* values) {
    delete[] values;
}

QUIVER_C_API void quiver_free_string_array(char** values, size_t count) {
    if (!values) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        delete[] values[i];
    }
    delete[] values;
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_integers(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t*** out_vectors,
                                                                 size_t** out_sizes,
                                                                 size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_vector_integers(collection, attribute), out_vectors, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_floats(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               double*** out_vectors,
                                                               size_t** out_sizes,
                                                               size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_vector_floats(collection, attribute), out_vectors, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_strings(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                char**** out_vectors,
                                                                size_t** out_sizes,
                                                                size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto vectors = db->db.read_vector_strings(collection, attribute);
        *out_count = vectors.size();
        if (vectors.empty()) {
            *out_vectors = nullptr;
            *out_sizes = nullptr;
            return QUIVER_OK;
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
                    (*out_vectors)[i][j] = new char[vectors[i][j].size() + 1];
                    std::copy(vectors[i][j].begin(), vectors[i][j].end(), (*out_vectors)[i][j]);
                    (*out_vectors)[i][j][vectors[i][j].size()] = '\0';
                }
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API void quiver_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count) {
    free_vectors_impl(vectors, sizes, count);
}

QUIVER_C_API void quiver_free_float_vectors(double** vectors, size_t* sizes, size_t count) {
    free_vectors_impl(vectors, sizes, count);
}

QUIVER_C_API void quiver_free_string_vectors(char*** vectors, size_t* sizes, size_t count) {
    if (!vectors) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        if (vectors[i]) {
            for (size_t j = 0; j < sizes[i]; ++j) {
                delete[] vectors[i][j];
            }
            delete[] vectors[i];
        }
    }
    delete[] vectors;
    delete[] sizes;
}

// Set read functions (reuse vector helpers since sets have same return structure)

QUIVER_C_API quiver_error_t quiver_database_read_set_integers(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t*** out_sets,
                                                              size_t** out_sizes,
                                                              size_t* out_count) {
    if (!db || !collection || !attribute || !out_sets || !out_sizes || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_set_integers(collection, attribute), out_sets, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_floats(quiver_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            double*** out_sets,
                                                            size_t** out_sizes,
                                                            size_t* out_count) {
    if (!db || !collection || !attribute || !out_sets || !out_sizes || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_set_floats(collection, attribute), out_sets, out_sizes, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_strings(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             char**** out_sets,
                                                             size_t** out_sizes,
                                                             size_t* out_count) {
    if (!db || !collection || !attribute || !out_sets || !out_sizes || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto sets = db->db.read_set_strings(collection, attribute);
        *out_count = sets.size();
        if (sets.empty()) {
            *out_sets = nullptr;
            *out_sizes = nullptr;
            return QUIVER_OK;
        }
        *out_sets = new char**[sets.size()];
        *out_sizes = new size_t[sets.size()];
        for (size_t i = 0; i < sets.size(); ++i) {
            (*out_sizes)[i] = sets[i].size();
            if (sets[i].empty()) {
                (*out_sets)[i] = nullptr;
            } else {
                (*out_sets)[i] = new char*[sets[i].size()];
                for (size_t j = 0; j < sets[i].size(); ++j) {
                    (*out_sets)[i][j] = new char[sets[i][j].size() + 1];
                    std::copy(sets[i][j].begin(), sets[i][j].end(), (*out_sets)[i][j]);
                    (*out_sets)[i][j][sets[i][j].size()] = '\0';
                }
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Read scalar by ID functions

QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers_by_id(quiver_database_t* db,
                                                                       const char* collection,
                                                                       const char* attribute,
                                                                       int64_t id,
                                                                       int64_t* out_value,
                                                                       int* out_has_value) {
    if (!db || !collection || !attribute || !out_value || !out_has_value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto result = db->db.read_scalar_integers_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_floats_by_id(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* attribute,
                                                                     int64_t id,
                                                                     double* out_value,
                                                                     int* out_has_value) {
    if (!db || !collection || !attribute || !out_value || !out_has_value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto result = db->db.read_scalar_floats_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_strings_by_id(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      int64_t id,
                                                                      char** out_value,
                                                                      int* out_has_value) {
    if (!db || !collection || !attribute || !out_value || !out_has_value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto result = db->db.read_scalar_strings_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = new char[result->size() + 1];
            std::copy(result->begin(), result->end(), *out_value);
            (*out_value)[result->size()] = '\0';
            *out_has_value = 1;
        } else {
            *out_value = nullptr;
            *out_has_value = 0;
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Read vector by ID functions

QUIVER_C_API quiver_error_t quiver_database_read_vector_integers_by_id(quiver_database_t* db,
                                                                       const char* collection,
                                                                       const char* attribute,
                                                                       int64_t id,
                                                                       int64_t** out_values,
                                                                       size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_vector_integers_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_floats_by_id(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* attribute,
                                                                     int64_t id,
                                                                     double** out_values,
                                                                     size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_vector_floats_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_vector_strings_by_id(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      int64_t id,
                                                                      char*** out_values,
                                                                      size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return copy_strings_to_c(db->db.read_vector_strings_by_id(collection, attribute, id), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Read set by ID functions

QUIVER_C_API quiver_error_t quiver_database_read_set_integers_by_id(quiver_database_t* db,
                                                                    const char* collection,
                                                                    const char* attribute,
                                                                    int64_t id,
                                                                    int64_t** out_values,
                                                                    size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_set_integers_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_floats_by_id(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  double** out_values,
                                                                  size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_set_floats_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_set_strings_by_id(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   char*** out_values,
                                                                   size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return copy_strings_to_c(db->db.read_set_strings_by_id(collection, attribute, id), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_element_ids(quiver_database_t* db,
                                                             const char* collection,
                                                             int64_t** out_ids,
                                                             size_t* out_count) {
    if (!db || !collection || !out_ids || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_scalars_impl(db->db.read_element_ids(collection), out_ids, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Update scalar functions

QUIVER_C_API quiver_error_t quiver_database_update_scalar_integer(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t value) {
    if (!db || !collection || !attribute) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_scalar_integer(collection, attribute, id, value);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_scalar_float(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double value) {
    if (!db || !collection || !attribute) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_scalar_float(collection, attribute, id, value);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_scalar_string(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const char* value) {
    if (!db || !collection || !attribute || !value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_scalar_string(collection, attribute, id, value);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Update vector functions

QUIVER_C_API quiver_error_t quiver_database_update_vector_integers(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   const int64_t* values,
                                                                   size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_vector_integers(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_floats(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const double* values,
                                                                 size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<double> vec(values, values + count);
        db->db.update_vector_floats(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_strings(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  const char* const* values,
                                                                  size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            vec.emplace_back(values[i]);
        }
        db->db.update_vector_strings(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Update set functions

QUIVER_C_API quiver_error_t quiver_database_update_set_integers(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                const int64_t* values,
                                                                size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_set_integers(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_floats(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              const double* values,
                                                              size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<double> vec(values, values + count);
        db->db.update_set_floats(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_strings(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               int64_t id,
                                                               const char* const* values,
                                                               size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            vec.emplace_back(values[i]);
        }
        db->db.update_set_strings(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

// Helper to convert C++ DataType to C quiver_data_type_t
namespace {
quiver_data_type_t to_c_data_type(quiver::DataType type) {
    switch (type) {
    case quiver::DataType::Integer:
        return QUIVER_DATA_TYPE_INTEGER;
    case quiver::DataType::Real:
        return QUIVER_DATA_TYPE_FLOAT;
    case quiver::DataType::Text:
        return QUIVER_DATA_TYPE_STRING;
    case quiver::DataType::DateTime:
        return QUIVER_DATA_TYPE_DATE_TIME;
    }
    return QUIVER_DATA_TYPE_INTEGER;
}

char* strdup_safe(const std::string& str) {
    char* result = new char[str.size() + 1];
    std::copy(str.begin(), str.end(), result);
    result[str.size()] = '\0';
    return result;
}
}  // namespace

QUIVER_C_API quiver_error_t quiver_database_get_scalar_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                quiver_scalar_metadata_t* out_metadata) {
    if (!db || !collection || !attribute || !out_metadata) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto meta = db->db.get_scalar_metadata(collection, attribute);

        out_metadata->name = strdup_safe(meta.name);
        out_metadata->data_type = to_c_data_type(meta.data_type);
        out_metadata->not_null = meta.not_null ? 1 : 0;
        out_metadata->primary_key = meta.primary_key ? 1 : 0;
        if (meta.default_value.has_value()) {
            out_metadata->default_value = strdup_safe(*meta.default_value);
        } else {
            out_metadata->default_value = nullptr;
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_vector_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group_name,
                                                                quiver_vector_metadata_t* out_metadata) {
    if (!db || !collection || !group_name || !out_metadata) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto meta = db->db.get_vector_metadata(collection, group_name);

        out_metadata->group_name = strdup_safe(meta.group_name);
        out_metadata->value_column_count = meta.value_columns.size();

        if (meta.value_columns.empty()) {
            out_metadata->value_columns = nullptr;
        } else {
            out_metadata->value_columns = new quiver_scalar_metadata_t[meta.value_columns.size()];
            for (size_t i = 0; i < meta.value_columns.size(); ++i) {
                out_metadata->value_columns[i].name = strdup_safe(meta.value_columns[i].name);
                out_metadata->value_columns[i].data_type = to_c_data_type(meta.value_columns[i].data_type);
                out_metadata->value_columns[i].not_null = meta.value_columns[i].not_null ? 1 : 0;
                out_metadata->value_columns[i].primary_key = meta.value_columns[i].primary_key ? 1 : 0;
                if (meta.value_columns[i].default_value.has_value()) {
                    out_metadata->value_columns[i].default_value = strdup_safe(*meta.value_columns[i].default_value);
                } else {
                    out_metadata->value_columns[i].default_value = nullptr;
                }
            }
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_set_metadata(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* group_name,
                                                             quiver_set_metadata_t* out_metadata) {
    if (!db || !collection || !group_name || !out_metadata) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto meta = db->db.get_set_metadata(collection, group_name);

        out_metadata->group_name = strdup_safe(meta.group_name);
        out_metadata->value_column_count = meta.value_columns.size();

        if (meta.value_columns.empty()) {
            out_metadata->value_columns = nullptr;
        } else {
            out_metadata->value_columns = new quiver_scalar_metadata_t[meta.value_columns.size()];
            for (size_t i = 0; i < meta.value_columns.size(); ++i) {
                out_metadata->value_columns[i].name = strdup_safe(meta.value_columns[i].name);
                out_metadata->value_columns[i].data_type = to_c_data_type(meta.value_columns[i].data_type);
                out_metadata->value_columns[i].not_null = meta.value_columns[i].not_null ? 1 : 0;
                out_metadata->value_columns[i].primary_key = meta.value_columns[i].primary_key ? 1 : 0;
                if (meta.value_columns[i].default_value.has_value()) {
                    out_metadata->value_columns[i].default_value = strdup_safe(*meta.value_columns[i].default_value);
                } else {
                    out_metadata->value_columns[i].default_value = nullptr;
                }
            }
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API void quiver_free_scalar_metadata(quiver_scalar_metadata_t* metadata) {
    if (!metadata)
        return;
    delete[] metadata->name;
    delete[] metadata->default_value;
    metadata->name = nullptr;
    metadata->default_value = nullptr;
}

QUIVER_C_API void quiver_free_vector_metadata(quiver_vector_metadata_t* metadata) {
    if (!metadata)
        return;
    delete[] metadata->group_name;
    if (metadata->value_columns) {
        for (size_t i = 0; i < metadata->value_column_count; ++i) {
            delete[] metadata->value_columns[i].name;
            delete[] metadata->value_columns[i].default_value;
        }
        delete[] metadata->value_columns;
    }
    metadata->group_name = nullptr;
    metadata->value_columns = nullptr;
    metadata->value_column_count = 0;
}

QUIVER_C_API void quiver_free_set_metadata(quiver_set_metadata_t* metadata) {
    if (!metadata)
        return;
    delete[] metadata->group_name;
    if (metadata->value_columns) {
        for (size_t i = 0; i < metadata->value_column_count; ++i) {
            delete[] metadata->value_columns[i].name;
            delete[] metadata->value_columns[i].default_value;
        }
        delete[] metadata->value_columns;
    }
    metadata->group_name = nullptr;
    metadata->value_columns = nullptr;
    metadata->value_column_count = 0;
}

QUIVER_C_API quiver_error_t quiver_database_list_scalar_attributes(quiver_database_t* db,
                                                                   const char* collection,
                                                                   quiver_scalar_metadata_t** out_metadata,
                                                                   size_t* out_count) {
    if (!db || !collection || !out_metadata || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto attrs = db->db.list_scalar_attributes(collection);
        *out_count = attrs.size();
        if (attrs.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_scalar_metadata_t[attrs.size()];
        for (size_t i = 0; i < attrs.size(); ++i) {
            (*out_metadata)[i].name = strdup_safe(attrs[i].name);
            (*out_metadata)[i].data_type = to_c_data_type(attrs[i].data_type);
            (*out_metadata)[i].not_null = attrs[i].not_null ? 1 : 0;
            (*out_metadata)[i].primary_key = attrs[i].primary_key ? 1 : 0;
            if (attrs[i].default_value.has_value()) {
                (*out_metadata)[i].default_value = strdup_safe(*attrs[i].default_value);
            } else {
                (*out_metadata)[i].default_value = nullptr;
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_vector_groups(quiver_database_t* db,
                                                               const char* collection,
                                                               quiver_vector_metadata_t** out_metadata,
                                                               size_t* out_count) {
    if (!db || !collection || !out_metadata || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto groups = db->db.list_vector_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_vector_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            (*out_metadata)[i].group_name = strdup_safe(groups[i].group_name);
            (*out_metadata)[i].value_column_count = groups[i].value_columns.size();
            if (groups[i].value_columns.empty()) {
                (*out_metadata)[i].value_columns = nullptr;
            } else {
                (*out_metadata)[i].value_columns = new quiver_scalar_metadata_t[groups[i].value_columns.size()];
                for (size_t j = 0; j < groups[i].value_columns.size(); ++j) {
                    (*out_metadata)[i].value_columns[j].name = strdup_safe(groups[i].value_columns[j].name);
                    (*out_metadata)[i].value_columns[j].data_type =
                        to_c_data_type(groups[i].value_columns[j].data_type);
                    (*out_metadata)[i].value_columns[j].not_null = groups[i].value_columns[j].not_null ? 1 : 0;
                    (*out_metadata)[i].value_columns[j].primary_key = groups[i].value_columns[j].primary_key ? 1 : 0;
                    if (groups[i].value_columns[j].default_value.has_value()) {
                        (*out_metadata)[i].value_columns[j].default_value =
                            strdup_safe(*groups[i].value_columns[j].default_value);
                    } else {
                        (*out_metadata)[i].value_columns[j].default_value = nullptr;
                    }
                }
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_set_groups(quiver_database_t* db,
                                                            const char* collection,
                                                            quiver_set_metadata_t** out_metadata,
                                                            size_t* out_count) {
    if (!db || !collection || !out_metadata || !out_count) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto groups = db->db.list_set_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_set_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            (*out_metadata)[i].group_name = strdup_safe(groups[i].group_name);
            (*out_metadata)[i].value_column_count = groups[i].value_columns.size();
            if (groups[i].value_columns.empty()) {
                (*out_metadata)[i].value_columns = nullptr;
            } else {
                (*out_metadata)[i].value_columns = new quiver_scalar_metadata_t[groups[i].value_columns.size()];
                for (size_t j = 0; j < groups[i].value_columns.size(); ++j) {
                    (*out_metadata)[i].value_columns[j].name = strdup_safe(groups[i].value_columns[j].name);
                    (*out_metadata)[i].value_columns[j].data_type =
                        to_c_data_type(groups[i].value_columns[j].data_type);
                    (*out_metadata)[i].value_columns[j].not_null = groups[i].value_columns[j].not_null ? 1 : 0;
                    (*out_metadata)[i].value_columns[j].primary_key = groups[i].value_columns[j].primary_key ? 1 : 0;
                    if (groups[i].value_columns[j].default_value.has_value()) {
                        (*out_metadata)[i].value_columns[j].default_value =
                            strdup_safe(*groups[i].value_columns[j].default_value);
                    } else {
                        (*out_metadata)[i].value_columns[j].default_value = nullptr;
                    }
                }
            }
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API void quiver_free_scalar_metadata_array(quiver_scalar_metadata_t* metadata, size_t count) {
    if (!metadata)
        return;
    for (size_t i = 0; i < count; ++i) {
        delete[] metadata[i].name;
        delete[] metadata[i].default_value;
    }
    delete[] metadata;
}

QUIVER_C_API void quiver_free_vector_metadata_array(quiver_vector_metadata_t* metadata, size_t count) {
    if (!metadata)
        return;
    for (size_t i = 0; i < count; ++i) {
        delete[] metadata[i].group_name;
        if (metadata[i].value_columns) {
            for (size_t j = 0; j < metadata[i].value_column_count; ++j) {
                delete[] metadata[i].value_columns[j].name;
                delete[] metadata[i].value_columns[j].default_value;
            }
            delete[] metadata[i].value_columns;
        }
    }
    delete[] metadata;
}

QUIVER_C_API void quiver_free_set_metadata_array(quiver_set_metadata_t* metadata, size_t count) {
    if (!metadata)
        return;
    for (size_t i = 0; i < count; ++i) {
        delete[] metadata[i].group_name;
        if (metadata[i].value_columns) {
            for (size_t j = 0; j < metadata[i].value_column_count; ++j) {
                delete[] metadata[i].value_columns[j].name;
                delete[] metadata[i].value_columns[j].default_value;
            }
            delete[] metadata[i].value_columns;
        }
    }
    delete[] metadata;
}

QUIVER_C_API quiver_error_t quiver_database_export_to_csv(quiver_database_t* db, const char* table, const char* path) {
    if (!db || !table || !path) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.export_to_csv(table, path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_import_from_csv(quiver_database_t* db,
                                                            const char* table,
                                                            const char* path) {
    if (!db || !table || !path) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.import_from_csv(table, path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         char** out_value,
                                                         int* out_has_value) {
    if (!db || !sql || !out_value || !out_has_value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
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
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_integer(quiver_database_t* db,
                                                          const char* sql,
                                                          int64_t* out_value,
                                                          int* out_has_value) {
    if (!db || !sql || !out_value || !out_has_value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
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
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_database_query_float(quiver_database_t* db,
                                                        const char* sql,
                                                        double* out_value,
                                                        int* out_has_value) {
    if (!db || !sql || !out_value || !out_has_value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
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
        return QUIVER_ERROR_DATABASE;
    }
}

}  // extern "C"
