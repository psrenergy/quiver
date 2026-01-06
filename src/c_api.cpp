#include "psr/c/common.h"
#include "psr/c/database.h"
#include "psr/c/element.h"
#include "psr/database.h"
#include "psr/element.h"

#include <new>
#include <string>
#include <vector>

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

}  // anonymous namespace

// Internal struct definition
struct psr_database {
    psr::Database db;
    psr_database(const std::string& path, const psr::DatabaseOptions& options) : db(path, options) {}
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
    if (db) {
        delete db;
    }
}

PSR_C_API int psr_database_is_healthy(psr_database_t* db) {
    if (!db)
        return 0;
    return db->db.is_healthy() ? 1 : 0;
}

PSR_C_API const char* psr_database_path(psr_database_t* db) {
    if (!db)
        return nullptr;
    return db->db.path().c_str();
}

PSR_C_API psr_database_t*
psr_database_from_migration(const char* db_path, const char* migrations_path, const psr_database_options_t* options) {
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

PSR_C_API const char* psr_error_string(psr_error_t error) {
    switch (error) {
    case PSR_OK:
        return "Success";
    case PSR_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case PSR_ERROR_DATABASE:
        return "Database error";
    case PSR_ERROR_MIGRATION:
        return "Migration error";
    default:
        return "Unknown error";
    }
}

PSR_C_API const char* psr_version(void) {
    return PSR_VERSION;
}

}  // extern "C"

// Element internal struct
struct psr_element {
    psr::Element element;
};

extern "C" {

PSR_C_API psr_element_t* psr_element_create(void) {
    try {
        return new psr_element();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

PSR_C_API void psr_element_destroy(psr_element_t* element) {
    delete element;
}

PSR_C_API void psr_element_clear(psr_element_t* element) {
    if (element) {
        element->element.clear();
    }
}

PSR_C_API psr_error_t psr_element_set_int(psr_element_t* element, const char* name, int64_t value) {
    if (!element || !name) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_double(psr_element_t* element, const char* name, double value) {
    if (!element || !name) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_string(psr_element_t* element, const char* name, const char* value) {
    if (!element || !name || !value) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, std::string(value));
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_null(psr_element_t* element, const char* name) {
    if (!element || !name) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.set_null(name);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_vector_int(psr_element_t* element,
                                                 const char* name,
                                                 const int64_t* values,
                                                 size_t count) {
    if (!element || !name || (!values && count > 0)) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.set_vector(name, std::vector<int64_t>(values, values + count));
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_vector_double(psr_element_t* element,
                                                    const char* name,
                                                    const double* values,
                                                    size_t count) {
    if (!element || !name || (!values && count > 0)) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.set_vector(name, std::vector<double>(values, values + count));
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_vector_string(psr_element_t* element,
                                                    const char* name,
                                                    const char** values,
                                                    size_t count) {
    if (!element || !name || (!values && count > 0)) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    std::vector<std::string> vec;
    vec.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        vec.emplace_back(values[i] ? values[i] : "");
    }
    element->element.set_vector(name, std::move(vec));
    return PSR_OK;
}

PSR_C_API int psr_element_has_scalars(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_scalars() ? 1 : 0;
}

PSR_C_API int psr_element_has_vectors(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_vectors() ? 1 : 0;
}

PSR_C_API size_t psr_element_scalar_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.scalars().size();
}

PSR_C_API size_t psr_element_vector_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.vectors().size();
}

}  // extern "C"
