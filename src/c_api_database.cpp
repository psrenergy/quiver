#include "psr/c/database.h"
#include "psr/c/element.h"
#include "psr/database.h"

#include <new>
#include <string>

// Forward declare the psr_element struct (defined in c_api_element.cpp)
struct psr_element {
    psr::Element element;
};

namespace {

psr::LogLevel to_cpp_log_level(psr_log_level_t level) {
    switch (level) {
    case PSR_LOG_DEBUG:
        return psr::LogLevel::debug;
    case PSR_LOG_INFO:
        return psr::LogLevel::info;
    case PSR_LOG_WARN:
        return psr::LogLevel::warn;
    case PSR_LOG_ERROR:
        return psr::LogLevel::error;
    case PSR_LOG_OFF:
        return psr::LogLevel::off;
    default:
        return psr::LogLevel::info;
    }
}

psr::DatabaseOptions to_cpp_options(const psr_database_options_t* options) {
    psr::DatabaseOptions cpp_options;
    if (options) {
        cpp_options.read_only = options->read_only != 0;
        cpp_options.console_level = to_cpp_log_level(options->console_level);
    }
    return cpp_options;
}

}  // namespace

struct psr_database {
    psr::Database db;
    psr_database(const std::string& path, const psr::DatabaseOptions& options) : db(path, options) {}
    psr_database(psr::Database&& database) : db(std::move(database)) {}
};

extern "C" {

PSR_C_API psr_database_options_t psr_database_options_default(void) {
    psr_database_options_t options;
    options.read_only = 0;
    options.console_level = PSR_LOG_INFO;
    return options;
}

PSR_C_API psr_database_t* psr_database_open(const char* path, const psr_database_options_t* options) {
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

PSR_C_API void psr_database_close(psr_database_t* db) {
    delete db;
}

PSR_C_API int psr_database_is_healthy(psr_database_t* db) {
    if (!db) {
        return 0;
    }
    return db->db.is_healthy() ? 1 : 0;
}

PSR_C_API const char* psr_database_path(psr_database_t* db) {
    if (!db) {
        return nullptr;
    }
    return db->db.path().c_str();
}

PSR_C_API psr_database_t*
psr_database_from_migrations(const char* db_path, const char* migrations_path, const psr_database_options_t* options) {
    if (!db_path || !migrations_path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        auto* wrapper = new psr_database(db_path, cpp_options);
        wrapper->db.migrate_up(migrations_path);
        return wrapper;
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

PSR_C_API int64_t psr_database_current_version(psr_database_t* db) {
    if (!db) {
        return -1;
    }
    try {
        return db->db.current_version();
    } catch (const std::exception&) {
        return -1;
    }
}

PSR_C_API psr_error_t psr_database_set_version(psr_database_t* db, int64_t version) {
    if (!db) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.set_version(version);
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_migrate_up(psr_database_t* db, const char* migrations_path) {
    if (!db || !migrations_path) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.migrate_up(migrations_path);
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_MIGRATION;
    }
}

PSR_C_API psr_error_t psr_database_begin_transaction(psr_database_t* db) {
    if (!db) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.begin_transaction();
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_commit(psr_database_t* db) {
    if (!db) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.commit();
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_rollback(psr_database_t* db) {
    if (!db) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    try {
        db->db.rollback();
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API int64_t psr_database_create_element(psr_database_t* db, const char* collection, psr_element_t* element) {
    if (!db || !collection || !element) {
        return -1;
    }
    try {
        return db->db.create_element(collection, element->element);
    } catch (const std::exception&) {
        return -1;
    }
}

PSR_C_API psr_database_t*
psr_database_from_schema(const char* db_path, const char* schema_path, const psr_database_options_t* options) {
    if (!db_path || !schema_path) {
        return nullptr;
    }

    try {
        auto cpp_options = to_cpp_options(options);
        auto db = psr::Database::from_schema(db_path, schema_path, cpp_options);
        return new psr_database(std::move(db));
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (const std::exception&) {
        return nullptr;
    }
}

PSR_C_API psr_error_t psr_database_read_scalar_ints(psr_database_t* db,
                                                    const char* collection,
                                                    const char* attribute,
                                                    int64_t** out_values,
                                                    size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto values = db->db.read_scalar_ints(collection, attribute);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return PSR_OK;
        }
        *out_values = new int64_t[values.size()];
        std::copy(values.begin(), values.end(), *out_values);
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_read_scalar_doubles(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       double** out_values,
                                                       size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto values = db->db.read_scalar_doubles(collection, attribute);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return PSR_OK;
        }
        *out_values = new double[values.size()];
        std::copy(values.begin(), values.end(), *out_values);
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_read_scalar_strings(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       char*** out_values,
                                                       size_t* out_count) {
    if (!db || !collection || !attribute || !out_values || !out_count) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto values = db->db.read_scalar_strings(collection, attribute);
        *out_count = values.size();
        if (values.empty()) {
            *out_values = nullptr;
            return PSR_OK;
        }
        *out_values = new char*[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            (*out_values)[i] = new char[values[i].size() + 1];
            std::copy(values[i].begin(), values[i].end(), (*out_values)[i]);
            (*out_values)[i][values[i].size()] = '\0';
        }
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API void psr_free_int_array(int64_t* values) {
    delete[] values;
}

PSR_C_API void psr_free_double_array(double* values) {
    delete[] values;
}

PSR_C_API void psr_free_string_array(char** values, size_t count) {
    if (!values) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        delete[] values[i];
    }
    delete[] values;
}

PSR_C_API psr_error_t psr_database_read_vector_ints(psr_database_t* db,
                                                     const char* collection,
                                                     const char* attribute,
                                                     int64_t*** out_vectors,
                                                     size_t** out_sizes,
                                                     size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto vectors = db->db.read_vector_ints(collection, attribute);
        *out_count = vectors.size();
        if (vectors.empty()) {
            *out_vectors = nullptr;
            *out_sizes = nullptr;
            return PSR_OK;
        }
        *out_vectors = new int64_t*[vectors.size()];
        *out_sizes = new size_t[vectors.size()];
        for (size_t i = 0; i < vectors.size(); ++i) {
            (*out_sizes)[i] = vectors[i].size();
            if (vectors[i].empty()) {
                (*out_vectors)[i] = nullptr;
            } else {
                (*out_vectors)[i] = new int64_t[vectors[i].size()];
                std::copy(vectors[i].begin(), vectors[i].end(), (*out_vectors)[i]);
            }
        }
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_read_vector_doubles(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        double*** out_vectors,
                                                        size_t** out_sizes,
                                                        size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto vectors = db->db.read_vector_doubles(collection, attribute);
        *out_count = vectors.size();
        if (vectors.empty()) {
            *out_vectors = nullptr;
            *out_sizes = nullptr;
            return PSR_OK;
        }
        *out_vectors = new double*[vectors.size()];
        *out_sizes = new size_t[vectors.size()];
        for (size_t i = 0; i < vectors.size(); ++i) {
            (*out_sizes)[i] = vectors[i].size();
            if (vectors[i].empty()) {
                (*out_vectors)[i] = nullptr;
            } else {
                (*out_vectors)[i] = new double[vectors[i].size()];
                std::copy(vectors[i].begin(), vectors[i].end(), (*out_vectors)[i]);
            }
        }
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_read_vector_strings(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        char**** out_vectors,
                                                        size_t** out_sizes,
                                                        size_t* out_count) {
    if (!db || !collection || !attribute || !out_vectors || !out_sizes || !out_count) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto vectors = db->db.read_vector_strings(collection, attribute);
        *out_count = vectors.size();
        if (vectors.empty()) {
            *out_vectors = nullptr;
            *out_sizes = nullptr;
            return PSR_OK;
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
        return PSR_OK;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API void psr_free_int_vectors(int64_t** vectors, size_t* sizes, size_t count) {
    if (!vectors) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        delete[] vectors[i];
    }
    delete[] vectors;
    delete[] sizes;
}

PSR_C_API void psr_free_double_vectors(double** vectors, size_t* sizes, size_t count) {
    if (!vectors) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        delete[] vectors[i];
    }
    delete[] vectors;
    delete[] sizes;
}

PSR_C_API void psr_free_string_vectors(char*** vectors, size_t* sizes, size_t count) {
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

}  // extern "C"
