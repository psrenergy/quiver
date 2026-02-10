#include "quiver/c/database.h"

#include "database_helpers.h"
#include "internal.h"

#include <stdexcept>
#include <string>
#include <vector>

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

extern "C" {

// Plain query functions

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

// Parameterized query functions

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

}  // extern "C"
