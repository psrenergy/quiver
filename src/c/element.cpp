#include "quiver/c/element.h"

#include "internal.h"

#include <cstring>
#include <new>
#include <string>

extern "C" {

QUIVER_C_API quiver_error_t quiver_element_create(quiver_element_t** out_element) {
    QUIVER_REQUIRE(out_element);

    try {
        *out_element = new quiver_element();
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_element_destroy(quiver_element_t* element) {
    QUIVER_REQUIRE(element);

    delete element;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_clear(quiver_element_t* element) {
    QUIVER_REQUIRE(element);

    element->element.clear();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_integer(quiver_element_t* element, const char* name, int64_t value) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);

    element->element.set(name, value);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_float(quiver_element_t* element, const char* name, double value) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);

    element->element.set(name, value);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_string(quiver_element_t* element, const char* name, const char* value) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);
    QUIVER_REQUIRE(value);

    element->element.set(name, std::string(value));
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_null(quiver_element_t* element, const char* name) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);

    element->element.set_null(name);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_array_integer(quiver_element_t* element,
                                                             const char* name,
                                                             const int64_t* values,
                                                             int32_t count) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);

    if ((!values && count > 0) || count < 0) {
        quiver_set_last_error("Invalid values/count combination");
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    std::vector<int64_t> arr(values, values + count);
    element->element.set(name, arr);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_array_float(quiver_element_t* element,
                                                           const char* name,
                                                           const double* values,
                                                           int32_t count) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);

    if ((!values && count > 0) || count < 0) {
        quiver_set_last_error("Invalid values/count combination");
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    std::vector<double> arr(values, values + count);
    element->element.set(name, arr);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_array_string(quiver_element_t* element,
                                                            const char* name,
                                                            const char* const* values,
                                                            int32_t count) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(name);

    if ((!values && count > 0) || count < 0) {
        quiver_set_last_error("Invalid values/count combination");
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    std::vector<std::string> arr;
    arr.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        arr.emplace_back(values[i] ? values[i] : "");
    }
    element->element.set(name, arr);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_has_scalars(quiver_element_t* element, int* out_result) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(out_result);

    *out_result = element->element.has_scalars() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_has_arrays(quiver_element_t* element, int* out_result) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(out_result);

    *out_result = element->element.has_arrays() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_scalar_count(quiver_element_t* element, size_t* out_count) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(out_count);

    *out_count = element->element.scalars().size();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_array_count(quiver_element_t* element, size_t* out_count) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(out_count);

    *out_count = element->element.arrays().size();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string) {
    QUIVER_REQUIRE(element);
    QUIVER_REQUIRE(out_string);

    try {
        auto str = element->element.to_string();
        auto result = new char[str.size() + 1];
        std::memcpy(result, str.c_str(), str.size() + 1);
        *out_string = result;
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR_DATABASE;
    }
}

QUIVER_C_API quiver_error_t quiver_string_free(char* str) {
    delete[] str;
    return QUIVER_OK;
}

}  // extern "C"
