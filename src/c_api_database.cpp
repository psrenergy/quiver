#include "psr/c/database.h"
#include "psr/c/element.h"
#include "psr/database.h"

#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <variant>

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

PSR_C_API int64_t psr_database_read_scalar_parameters_double(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              double** out_values) {
    if (!db || !collection || !attribute || !out_values) {
        return -1;
    }

    try {
        auto values = db->db.read_scalar_parameters(collection, attribute);

        *out_values = new double[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& val = values[i];
            if (std::holds_alternative<std::nullptr_t>(val)) {
                (*out_values)[i] = std::numeric_limits<double>::quiet_NaN();
            } else if (std::holds_alternative<double>(val)) {
                (*out_values)[i] = std::get<double>(val);
            } else if (std::holds_alternative<int64_t>(val)) {
                (*out_values)[i] = static_cast<double>(std::get<int64_t>(val));
            } else {
                delete[] *out_values;
                *out_values = nullptr;
                return -1;
            }
        }
        return static_cast<int64_t>(values.size());
    } catch (const std::exception&) {
        return -1;
    }
}

PSR_C_API int64_t psr_database_read_scalar_parameters_string(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              char*** out_values) {
    if (!db || !collection || !attribute || !out_values) {
        return -1;
    }

    try {
        auto values = db->db.read_scalar_parameters(collection, attribute);

        *out_values = new char*[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            const auto& val = values[i];
            std::string str;
            if (std::holds_alternative<std::nullptr_t>(val)) {
                str = "";
            } else if (std::holds_alternative<std::string>(val)) {
                str = std::get<std::string>(val);
            } else {
                for (size_t j = 0; j < i; ++j) {
                    delete[] (*out_values)[j];
                }
                delete[] *out_values;
                *out_values = nullptr;
                return -1;
            }
            (*out_values)[i] = new char[str.size() + 1];
            std::memcpy((*out_values)[i], str.c_str(), str.size() + 1);
        }
        return static_cast<int64_t>(values.size());
    } catch (const std::exception&) {
        return -1;
    }
}

PSR_C_API psr_error_t psr_database_read_scalar_parameter_double(psr_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 const char* label,
                                                                 double* out_value,
                                                                 int* is_null) {
    if (!db || !collection || !attribute || !label || !out_value || !is_null) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto value = db->db.read_scalar_parameter(collection, attribute, label);

        if (std::holds_alternative<std::nullptr_t>(value)) {
            *is_null = 1;
            *out_value = std::numeric_limits<double>::quiet_NaN();
        } else if (std::holds_alternative<double>(value)) {
            *is_null = 0;
            *out_value = std::get<double>(value);
        } else if (std::holds_alternative<int64_t>(value)) {
            *is_null = 0;
            *out_value = static_cast<double>(std::get<int64_t>(value));
        } else {
            return PSR_ERROR_DATABASE;
        }
        return PSR_OK;
    } catch (const std::runtime_error&) {
        return PSR_ERROR_NOT_FOUND;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API psr_error_t psr_database_read_scalar_parameter_string(psr_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 const char* label,
                                                                 char** out_value) {
    if (!db || !collection || !attribute || !label || !out_value) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }

    try {
        auto value = db->db.read_scalar_parameter(collection, attribute, label);

        std::string str;
        if (std::holds_alternative<std::nullptr_t>(value)) {
            str = "";
        } else if (std::holds_alternative<std::string>(value)) {
            str = std::get<std::string>(value);
        } else {
            return PSR_ERROR_DATABASE;
        }

        *out_value = new char[str.size() + 1];
        std::memcpy(*out_value, str.c_str(), str.size() + 1);
        return PSR_OK;
    } catch (const std::runtime_error&) {
        return PSR_ERROR_NOT_FOUND;
    } catch (const std::exception&) {
        return PSR_ERROR_DATABASE;
    }
}

PSR_C_API void psr_double_array_free(double* arr) {
    delete[] arr;
}

PSR_C_API void psr_string_array_free(char** arr, size_t count) {
    if (arr) {
        for (size_t i = 0; i < count; ++i) {
            delete[] arr[i];
        }
        delete[] arr;
    }
}

}  // extern "C"
