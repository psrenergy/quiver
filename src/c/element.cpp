#include "c_api_internal.h"
#include "quiver/c/element.h"

#include <cstring>
#include <new>
#include <string>

extern "C" {

QUIVER_C_API quiver_element_t* quiver_element_create(void) {
    try {
        return new quiver_element();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

QUIVER_C_API void quiver_element_destroy(quiver_element_t* element) {
    delete element;
}

QUIVER_C_API void quiver_element_clear(quiver_element_t* element) {
    if (element) {
        element->element.clear();
    }
}

QUIVER_C_API quiver_error_t quiver_element_set_integer(quiver_element_t* element, const char* name, int64_t value) {
    if (!element || !name) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_float(quiver_element_t* element, const char* name, double value) {
    if (!element || !name) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_string(quiver_element_t* element, const char* name, const char* value) {
    if (!element || !name || !value) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, std::string(value));
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_null(quiver_element_t* element, const char* name) {
    if (!element || !name) {
        return QUIVER_ERROR_INVALID_ARGUMENT;
    }
    element->element.set_null(name);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_element_set_array_integer(quiver_element_t* element,
                                                             const char* name,
                                                             const int64_t* values,
                                                             int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
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
    if (!element || !name || (!values && count > 0) || count < 0) {
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
    if (!element || !name || (!values && count > 0) || count < 0) {
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

QUIVER_C_API int quiver_element_has_scalars(quiver_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_scalars() ? 1 : 0;
}

QUIVER_C_API int quiver_element_has_arrays(quiver_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_arrays() ? 1 : 0;
}

QUIVER_C_API size_t quiver_element_scalar_count(quiver_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.scalars().size();
}

QUIVER_C_API size_t quiver_element_array_count(quiver_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.arrays().size();
}

QUIVER_C_API char* quiver_element_to_string(quiver_element_t* element) {
    if (!element) {
        return nullptr;
    }
    try {
        std::string str = element->element.to_string();
        char* result = new char[str.size() + 1];
        std::memcpy(result, str.c_str(), str.size() + 1);
        return result;
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

QUIVER_C_API void quiver_string_free(char* str) {
    delete[] str;
}

}  // extern "C"
