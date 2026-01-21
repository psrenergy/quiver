#include "c_api_internal.h"
#include "margaux/c/element.h"

#include <cstring>
#include <new>
#include <string>

extern "C" {

DECK_DATABASE_C_API element_t* element_create(void) {
    try {
        return new element();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

DECK_DATABASE_C_API void element_destroy(element_t* element) {
    delete element;
}

DECK_DATABASE_C_API void element_clear(element_t* element) {
    if (element) {
        element->element.clear();
    }
}

DECK_DATABASE_C_API margaux_error_t element_set_integer(element_t* element, const char* name, int64_t value) {
    if (!element || !name) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API margaux_error_t element_set_float(element_t* element, const char* name, double value) {
    if (!element || !name) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API margaux_error_t element_set_string(element_t* element, const char* name, const char* value) {
    if (!element || !name || !value) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, std::string(value));
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API margaux_error_t element_set_null(element_t* element, const char* name) {
    if (!element || !name) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    element->element.set_null(name);
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API margaux_error_t element_set_array_integer(element_t* element,
                                                        const char* name,
                                                        const int64_t* values,
                                                        int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    std::vector<int64_t> arr(values, values + count);
    element->element.set(name, arr);
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API margaux_error_t element_set_array_float(element_t* element,
                                                      const char* name,
                                                      const double* values,
                                                      int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    std::vector<double> arr(values, values + count);
    element->element.set(name, arr);
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API margaux_error_t element_set_array_string(element_t* element,
                                                       const char* name,
                                                       const char* const* values,
                                                       int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return DECK_DATABASE_ERROR_INVALID_ARGUMENT;
    }
    std::vector<std::string> arr;
    arr.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        arr.emplace_back(values[i] ? values[i] : "");
    }
    element->element.set(name, arr);
    return DECK_DATABASE_OK;
}

DECK_DATABASE_C_API int element_has_scalars(element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_scalars() ? 1 : 0;
}

DECK_DATABASE_C_API int element_has_arrays(element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_arrays() ? 1 : 0;
}

DECK_DATABASE_C_API size_t element_scalar_count(element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.scalars().size();
}

DECK_DATABASE_C_API size_t element_array_count(element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.arrays().size();
}

DECK_DATABASE_C_API char* element_to_string(element_t* element) {
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

DECK_DATABASE_C_API void margaux_string_free(char* str) {
    delete[] str;
}

}  // extern "C"
