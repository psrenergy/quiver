#include "c_api_internal.h"
#include "margaux/c/database.h"
#include "margaux/c/element.h"

#include <new>
#include <string>

namespace {

margaux::LogLevel to_cpp_log_level(psr_log_level_t level) {
    switch (level) {
    case PSR_LOG_DEBUG:
        return margaux::LogLevel::debug;
    case PSR_LOG_INFO:
        return margaux::LogLevel::info;
    case PSR_LOG_WARN:
        return margaux::LogLevel::warn;
    case PSR_LOG_ERROR:
        return margaux::LogLevel::error;
    case PSR_LOG_OFF:
        return margaux::LogLevel::off;
    default:
        return margaux::LogLevel::info;
    }
}

margaux::DatabaseOptions to_cpp_options(const psr_database_options_t* options) {
    margaux::DatabaseOptions cpp_options;
    if (options) {
        cpp_options.read_only = options->read_only != 0;
        cpp_options.console_level = to_cpp_log_level(options->console_level);
    }
    return cpp_options;
}

// Helper template for reading numeric scalars
template <typename T>
psr_error_t read_scalars_impl(const std::vector<T>& values, T** out_values, size_t* out_count) {
    *out_count = values.size();
    if (values.empty()) {
        *out_values = nullptr;
        return MARGAUX_OK;
    }
    *out_values = new T[values.size()];
    std::copy(values.begin(), values.end(), *out_values);
    return MARGAUX_OK;
}

// Helper template for reading numeric vectors
template <typename T>
psr_error_t
read_vectors_impl(const std::vector<std::vector<T>>& vectors, T*** out_vectors, size_t** out_sizes, size_t* out_count) {
    *out_count = vectors.size();
    if (vectors.empty()) {
        *out_vectors = nullptr;
        *out_sizes = nullptr;
        return MARGAUX_OK;
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
    return MARGAUX_OK;
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

}  // namespace

extern "C" {

MARGAUX_C_API psr_database_options_t psr_database_options_default(void) {
    psr_database_options_t options;
    options.read_only = 0;
    options.console_level = PSR_LOG_INFO;
    return options;
}

MARGAUX_C_API psr_database_t* psr_database_open(const char* path, const psr_database_options_t* options) {
    if (!path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        return new psr_database(path, cpp_options);
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

MARGAUX_C_API void psr_database_close(psr_database_t* db) {
    delete db;
}

MARGAUX_C_API int psr_database_is_healthy(psr_database_t* db) {
    if (!db) {
        return 0;
    }
    return db->db.is_healthy() ? 1 : 0;
}

MARGAUX_C_API const char* psr_database_path(psr_database_t* db) {
    if (!db) {
        return nullptr;
    }
    return db->db.path().c_str();
}

MARGAUX_C_API psr_database_t*
psr_database_from_migrations(const char* db_path, const char* migrations_path, const psr_database_options_t* options) {
    if (!db_path || !migrations_path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        auto db = margaux::Database::from_migrations(db_path, migrations_path, cpp_options);
        return new psr_database(std::move(db));
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

MARGAUX_C_API int64_t psr_database_current_version(psr_database_t* db) {
    if (!db) {
        return -1;
    }
    try {
        return db->db.current_version();
    } catch (const std::exception&) {
        return -1;
    }
}

MARGAUX_C_API int64_t psr_database_create_element(psr_database_t* db, const char* collection, psr_element_t* element) {
    if (!db || !collection || !element) {
        return -1;
    }
    try {
        return db->db.create_element(collection, element->element);
    } catch (const std::exception&) {
        return -1;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_element(psr_database_t* db,
                                                      const char* collection,
                                                      int64_t id,
                                                      const psr_element_t* element) {
    if (!db || !collection || !element) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_element(collection, id, element->element);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_delete_element_by_id(psr_database_t* db, const char* collection, int64_t id) {
    if (!db || !collection) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.delete_element_by_id(collection, id);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_set_scalar_relation(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           const char* from_label,
                                                           const char* to_label) {
    if (!db || !collection || !attribute || !from_label || !to_label) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.set_scalar_relation(collection, attribute, from_label, to_label);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_scalar_relation(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            char*** out_values,
                                                            size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_scalar_relation(collection, attribute);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return MARGAUX_OK;
        }
        *out_values = new char*[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            (*out_values)[i] = new char[values[i].size() + 1];
            std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
            (*out_values)[i][values[i].size()] = '\0';
        }
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_database_t*
psr_database_from_schema(const char* db_path, const char* schema_path, const psr_database_options_t* options) {
    if (!db_path || !schema_path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        auto db = margaux::Database::from_schema(db_path, schema_path, cpp_options);
        return new psr_database(std::move(db));
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_scalar_integers(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t** out_values,
                                                            size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_scalars_impl(db->db.read_scalar_integers(collection, attribute), out_values, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_scalar_floats(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          double** out_values,
                                                          size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_scalars_impl(db->db.read_scalar_floats(collection, attribute), out_values, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_scalar_strings(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           char*** out_values,
                                                           size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto values = db->db.read_scalar_strings(collection, attribute);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return MARGAUX_OK;
        }
        *out_values = new char*[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            (*out_values)[i] = new char[values[i].size() + 1];
            std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
            (*out_values)[i][values[i].size()] = '\0';
        }
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API void psr_free_integer_array(int64_t* values) {
    delete[] values;
}

MARGAUX_C_API void psr_free_float_array(double* values) {
    delete[] values;
}

MARGAUX_C_API void psr_free_string_array(char** values, size_t count) {
    if (!values) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        delete[] values[i];
    }
    delete[] values;
}

MARGAUX_C_API psr_error_t psr_database_read_vector_integers(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t*** out_vectors,
                                                            size_t** out_sizes,
                                                            size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_vector_integers(collection, attribute), out_vectors, out_sizes, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_vector_floats(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          double*** out_vectors,
                                                          size_t** out_sizes,
                                                          size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_vector_floats(collection, attribute), out_vectors, out_sizes, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_vector_strings(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           char**** out_vectors,
                                                           size_t** out_sizes,
                                                           size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto vectors = db->db.read_vector_strings(collection, attribute);
        *out_count = vectors.size();
        if (vectors.empty()) {
            *out_vectors = nullptr;
            *out_sizes = nullptr;
            return MARGAUX_OK;
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
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API void psr_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count) {
    free_vectors_impl(vectors, sizes, count);
}

MARGAUX_C_API void psr_free_float_vectors(double** vectors, size_t* sizes, size_t count) {
    free_vectors_impl(vectors, sizes, count);
}

MARGAUX_C_API void psr_free_string_vectors(char*** vectors, size_t* sizes, size_t count) {
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

MARGAUX_C_API psr_error_t psr_database_read_set_integers(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t*** out_sets,
                                                         size_t** out_sizes,
                                                         size_t* out_count) {
    if (!db || !collection || !attribute || !out_sets || !out_sizes || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_set_integers(collection, attribute), out_sets, out_sizes, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_set_floats(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       double*** out_sets,
                                                       size_t** out_sizes,
                                                       size_t* out_count) {
    if (!db || !collection || !attribute || !out_sets || !out_sizes || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_vectors_impl(db->db.read_set_floats(collection, attribute), out_sets, out_sizes, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_set_strings(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        char**** out_sets,
                                                        size_t** out_sizes,
                                                        size_t* out_count) {
    if (!db || !collection || !attribute || !out_sets || !out_sizes || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto sets = db->db.read_set_strings(collection, attribute);
        *out_count = sets.size();
        if (sets.empty()) {
            *out_sets = nullptr;
            *out_sizes = nullptr;
            return MARGAUX_OK;
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
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

// Read scalar by ID functions

MARGAUX_C_API psr_error_t psr_database_read_scalar_integers_by_id(psr_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t* out_value,
                                                                  int* out_has_value) {
    if (!db || !collection || !attribute || !out_value || !out_has_value) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto result = db->db.read_scalar_integers_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_scalar_floats_by_id(psr_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double* out_value,
                                                                int* out_has_value) {
    if (!db || !collection || !attribute || !out_value || !out_has_value) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto result = db->db.read_scalar_floats_by_id(collection, attribute, id);
        if (result.has_value()) {
            *out_value = *result;
            *out_has_value = 1;
        } else {
            *out_has_value = 0;
        }
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_scalar_strings_by_id(psr_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 char** out_value,
                                                                 int* out_has_value) {
    if (!db || !collection || !attribute || !out_value || !out_has_value) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
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
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

// Read vector by ID functions

MARGAUX_C_API psr_error_t psr_database_read_vector_integers_by_id(psr_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t** out_values,
                                                                  size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_vector_integers_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_vector_floats_by_id(psr_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double** out_values,
                                                                size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_vector_floats_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_vector_strings_by_id(psr_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 char*** out_values,
                                                                 size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_vector_strings_by_id(collection, attribute, id);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return MARGAUX_OK;
        }
        *out_values = new char*[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            (*out_values)[i] = new char[values[i].size() + 1];
            std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
            (*out_values)[i][values[i].size()] = '\0';
        }
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

// Read set by ID functions

MARGAUX_C_API psr_error_t psr_database_read_set_integers_by_id(psr_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               int64_t id,
                                                               int64_t** out_values,
                                                               size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_set_integers_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_set_floats_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             double** out_values,
                                                             size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_set_floats_by_id(collection, attribute, id);
        return read_scalars_impl(values, out_values, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_set_strings_by_id(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              char*** out_values,
                                                              size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto values = db->db.read_set_strings_by_id(collection, attribute, id);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return MARGAUX_OK;
        }
        *out_values = new char*[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            (*out_values)[i] = new char[values[i].size() + 1];
            std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
            (*out_values)[i][values[i].size()] = '\0';
        }
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_read_element_ids(psr_database_t* db,
                                                        const char* collection,
                                                        int64_t** out_ids,
                                                        size_t* out_count) {
    if (!db || !collection || !out_ids || !out_count) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        return read_scalars_impl(db->db.read_element_ids(collection), out_ids, out_count);
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

// Update scalar functions

MARGAUX_C_API psr_error_t psr_database_update_scalar_integer(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             int64_t value) {
    if (!db || !collection || !attribute) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_scalar_integer(collection, attribute, id, value);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_scalar_float(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           int64_t id,
                                                           double value) {
    if (!db || !collection || !attribute) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_scalar_float(collection, attribute, id, value);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_scalar_string(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t id,
                                                            const char* value) {
    if (!db || !collection || !attribute || !value) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.update_scalar_string(collection, attribute, id, value);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

// Update vector functions

MARGAUX_C_API psr_error_t psr_database_update_vector_integers(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              const int64_t* values,
                                                              size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_vector_integers(collection, attribute, id, vec);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_vector_floats(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t id,
                                                            const double* values,
                                                            size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<double> vec(values, values + count);
        db->db.update_vector_floats(collection, attribute, id, vec);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_vector_strings(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             const char* const* values,
                                                             size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            vec.emplace_back(values[i]);
        }
        db->db.update_vector_strings(collection, attribute, id, vec);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

// Update set functions

MARGAUX_C_API psr_error_t psr_database_update_set_integers(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           int64_t id,
                                                           const int64_t* values,
                                                           size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_set_integers(collection, attribute, id, vec);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_set_floats(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         const double* values,
                                                         size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<double> vec(values, values + count);
        db->db.update_set_floats(collection, attribute, id, vec);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_update_set_strings(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          const char* const* values,
                                                          size_t count) {
    if (!db || !collection || !attribute || (count > 0 && !values)) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            vec.emplace_back(values[i]);
        }
        db->db.update_set_strings(collection, attribute, id, vec);
        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

MARGAUX_C_API psr_error_t psr_database_get_attribute_type(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          psr_data_structure_t* out_data_structure,
                                                          psr_data_type_t* out_data_type) {
    if (!db || !collection || !attribute || !out_data_structure || !out_data_type) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    try {
        auto attr_type = db->db.get_attribute_type(collection, attribute);

        switch (attr_type.data_structure) {
        case margaux::DataStructure::Scalar:
            *out_data_structure = PSR_DATA_STRUCTURE_SCALAR;
            break;
        case margaux::DataStructure::Vector:
            *out_data_structure = PSR_DATA_STRUCTURE_VECTOR;
            break;
        case margaux::DataStructure::Set:
            *out_data_structure = PSR_DATA_STRUCTURE_SET;
            break;
        }

        switch (attr_type.data_type) {
        case margaux::DataType::Integer:
            *out_data_type = PSR_DATA_TYPE_INTEGER;
            break;
        case margaux::DataType::Real:
            *out_data_type = PSR_DATA_TYPE_FLOAT;
            break;
        case margaux::DataType::Text:
            *out_data_type = PSR_DATA_TYPE_STRING;
            break;
        }

        return MARGAUX_OK;
    } catch (const std::exception&) {
        return MARGAUX_ERROR_DATABASE;
    }
}

}  // extern "C"
