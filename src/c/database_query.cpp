#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

#include <stdexcept>
#include <string>
#include <vector>

// Converts the C parameter arrays to std::vector<Value>. `caller` is the C++ method the C function
// forwards to, so a Pattern 1 error names the operation the user called (the same convention as
// unmarshal_group_columns_to_rows).
static std::vector<quiver::Value>
convert_params(const char* caller, const int* param_types, const void* const* param_values, size_t param_count) {
    std::vector<quiver::Value> parameters;
    parameters.reserve(param_count);
    for (size_t i = 0; i < param_count; ++i) {
        switch (param_types[i]) {
        case QUIVER_DATA_TYPE_INTEGER:
            parameters.emplace_back(*static_cast<const int64_t*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_FLOAT:
            parameters.emplace_back(*static_cast<const double*>(param_values[i]));
            break;
        case QUIVER_DATA_TYPE_STRING:
            if (!param_values[i]) {
                throw std::runtime_error(
                    std::string("Cannot ") + caller + ": parameter at index " + std::to_string(i) +
                    " has null string value"
                );
            }
            parameters.emplace_back(std::string(static_cast<const char*>(param_values[i])));
            break;
        case QUIVER_DATA_TYPE_NULL:
            parameters.emplace_back(nullptr);
            break;
        default:
            throw std::runtime_error(
                std::string("Cannot ") + caller + ": unknown parameter type " + std::to_string(param_types[i])
            );
        }
    }
    return parameters;
}

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_query_string(
    quiver_database_t* db,
    const char* sql,
    const int* param_types,
    const void* const* param_values,
    size_t param_count,
    char** out_value,
    int* out_has_value
) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }

    try {
        auto parameters = convert_params("query_string", param_types, param_values, param_count);
        auto result = db->db.query_string(sql, parameters);
        if (result.has_value()) {
            *out_value = quiver::string::new_c_str(*result);
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

QUIVER_C_API quiver_error_t quiver_database_query_integer(
    quiver_database_t* db,
    const char* sql,
    const int* param_types,
    const void* const* param_values,
    size_t param_count,
    int64_t* out_value,
    int* out_has_value
) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }

    try {
        auto parameters = convert_params("query_integer", param_types, param_values, param_count);
        auto result = db->db.query_integer(sql, parameters);
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

QUIVER_C_API quiver_error_t quiver_database_query_float(
    quiver_database_t* db,
    const char* sql,
    const int* param_types,
    const void* const* param_values,
    size_t param_count,
    double* out_value,
    int* out_has_value
) {
    QUIVER_REQUIRE(db, sql, out_value, out_has_value);
    if (param_count > 0) {
        QUIVER_REQUIRE(param_types, param_values);
    }

    try {
        auto parameters = convert_params("query_float", param_types, param_values, param_count);
        auto result = db->db.query_float(sql, parameters);
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
