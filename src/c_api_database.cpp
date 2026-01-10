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

// Convert a single psr::Value to psr_value_t
psr_value_t convert_value(const psr::Value& value) {
    psr_value_t result;
    result.data = {};

    std::visit(
        [&result](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                result.type = PSR_VALUE_NULL;
            } else if constexpr (std::is_same_v<T, int64_t>) {
                result.type = PSR_VALUE_INT64;
                result.data.int_value = arg;
            } else if constexpr (std::is_same_v<T, double>) {
                result.type = PSR_VALUE_DOUBLE;
                result.data.double_value = arg;
            } else if constexpr (std::is_same_v<T, std::string>) {
                result.type = PSR_VALUE_STRING;
                result.data.string_value = new char[arg.size() + 1];
                std::memcpy(result.data.string_value, arg.c_str(), arg.size() + 1);
            } else {
                // For other types (vectors in Value), treat as null
                result.type = PSR_VALUE_NULL;
            }
        },
        value);

    return result;
}

// Convert a vector of psr::Value to a psr_value_t with PSR_VALUE_ARRAY type
psr_value_t convert_value_array(const std::vector<psr::Value>& values) {
    psr_value_t result;
    result.type = PSR_VALUE_ARRAY;
    result.data.array_value.count = values.size();

    if (values.empty()) {
        result.data.array_value.elements = nullptr;
    } else {
        result.data.array_value.elements = new psr_value_t[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            result.data.array_value.elements[i] = convert_value(values[i]);
        }
    }

    return result;
}

psr_read_result_t make_error_result(psr_error_t error) {
    psr_read_result_t result;
    result.error = error;
    result.values = nullptr;
    result.count = 0;
    return result;
}

psr_read_result_t make_success_result(psr_value_t* values, size_t count) {
    psr_read_result_t result;
    result.error = PSR_OK;
    result.values = values;
    result.count = count;
    return result;
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

// Generic tagged union read functions

PSR_C_API psr_read_result_t psr_database_read_scalar_parameters(psr_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute) {
    if (!db || !collection || !attribute) {
        return make_error_result(PSR_ERROR_INVALID_ARGUMENT);
    }

    try {
        auto values = db->db.read_scalar_parameters(collection, attribute);

        if (values.empty()) {
            return make_success_result(nullptr, 0);
        }

        auto* result_values = new psr_value_t[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            result_values[i] = convert_value(values[i]);
        }

        return make_success_result(result_values, values.size());
    } catch (const std::runtime_error&) {
        return make_error_result(PSR_ERROR_NOT_FOUND);
    } catch (const std::exception&) {
        return make_error_result(PSR_ERROR_DATABASE);
    }
}

PSR_C_API psr_read_result_t psr_database_read_scalar_parameter(psr_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                const char* label) {
    if (!db || !collection || !attribute || !label) {
        return make_error_result(PSR_ERROR_INVALID_ARGUMENT);
    }

    try {
        auto value = db->db.read_scalar_parameter(collection, attribute, label);

        auto* result_values = new psr_value_t[1];
        result_values[0] = convert_value(value);

        return make_success_result(result_values, 1);
    } catch (const std::runtime_error&) {
        return make_error_result(PSR_ERROR_NOT_FOUND);
    } catch (const std::exception&) {
        return make_error_result(PSR_ERROR_DATABASE);
    }
}

PSR_C_API psr_read_result_t psr_database_read_vector_parameters(psr_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute) {
    if (!db || !collection || !attribute) {
        return make_error_result(PSR_ERROR_INVALID_ARGUMENT);
    }

    try {
        auto results = db->db.read_vector_parameters(collection, attribute);

        if (results.empty()) {
            return make_success_result(nullptr, 0);
        }

        auto* result_values = new psr_value_t[results.size()];
        for (size_t i = 0; i < results.size(); ++i) {
            result_values[i] = convert_value_array(results[i]);
        }

        return make_success_result(result_values, results.size());
    } catch (const std::runtime_error&) {
        return make_error_result(PSR_ERROR_NOT_FOUND);
    } catch (const std::exception&) {
        return make_error_result(PSR_ERROR_DATABASE);
    }
}

PSR_C_API psr_read_result_t psr_database_read_vector_parameter(psr_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                const char* label) {
    if (!db || !collection || !attribute || !label) {
        return make_error_result(PSR_ERROR_INVALID_ARGUMENT);
    }

    try {
        auto values = db->db.read_vector_parameter(collection, attribute, label);

        if (values.empty()) {
            return make_success_result(nullptr, 0);
        }

        auto* result_values = new psr_value_t[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            result_values[i] = convert_value(values[i]);
        }

        return make_success_result(result_values, values.size());
    } catch (const std::runtime_error&) {
        return make_error_result(PSR_ERROR_NOT_FOUND);
    } catch (const std::exception&) {
        return make_error_result(PSR_ERROR_DATABASE);
    }
}

PSR_C_API psr_read_result_t psr_database_read_set_parameters(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute) {
    if (!db || !collection || !attribute) {
        return make_error_result(PSR_ERROR_INVALID_ARGUMENT);
    }

    try {
        auto results = db->db.read_set_parameters(collection, attribute);

        if (results.empty()) {
            return make_success_result(nullptr, 0);
        }

        auto* result_values = new psr_value_t[results.size()];
        for (size_t i = 0; i < results.size(); ++i) {
            result_values[i] = convert_value_array(results[i]);
        }

        return make_success_result(result_values, results.size());
    } catch (const std::runtime_error&) {
        return make_error_result(PSR_ERROR_NOT_FOUND);
    } catch (const std::exception&) {
        return make_error_result(PSR_ERROR_DATABASE);
    }
}

PSR_C_API psr_read_result_t psr_database_read_set_parameter(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             const char* label) {
    if (!db || !collection || !attribute || !label) {
        return make_error_result(PSR_ERROR_INVALID_ARGUMENT);
    }

    try {
        auto values = db->db.read_set_parameter(collection, attribute, label);

        if (values.empty()) {
            return make_success_result(nullptr, 0);
        }

        auto* result_values = new psr_value_t[values.size()];
        for (size_t i = 0; i < values.size(); ++i) {
            result_values[i] = convert_value(values[i]);
        }

        return make_success_result(result_values, values.size());
    } catch (const std::runtime_error&) {
        return make_error_result(PSR_ERROR_NOT_FOUND);
    } catch (const std::exception&) {
        return make_error_result(PSR_ERROR_DATABASE);
    }
}

// Memory management for tagged union values

PSR_C_API void psr_value_free(psr_value_t* value) {
    if (!value)
        return;

    switch (value->type) {
    case PSR_VALUE_STRING:
        delete[] value->data.string_value;
        value->data.string_value = nullptr;
        break;
    case PSR_VALUE_ARRAY:
        if (value->data.array_value.elements) {
            for (size_t i = 0; i < value->data.array_value.count; ++i) {
                psr_value_free(&value->data.array_value.elements[i]);
            }
            delete[] value->data.array_value.elements;
            value->data.array_value.elements = nullptr;
        }
        break;
    default:
        break;
    }
    value->type = PSR_VALUE_NULL;
}

PSR_C_API void psr_read_result_free(psr_read_result_t* result) {
    if (!result || !result->values)
        return;

    for (size_t i = 0; i < result->count; ++i) {
        psr_value_free(&result->values[i]);
    }
    delete[] result->values;
    result->values = nullptr;
    result->count = 0;
}

}  // extern "C"
