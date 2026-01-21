#include "c_api_internal.h"
#include "margaux/c/element.h"

#include <cstring>
#include <new>
#include <string>

extern "C" {

MARGAUX_C_API psr_element_t* psr_element_create(void) {
    try {
        return new psr_element();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

MARGAUX_C_API void psr_element_destroy(psr_element_t* element) {
    delete element;
}

MARGAUX_C_API void psr_element_clear(psr_element_t* element) {
    if (element) {
        element->element.clear();
    }
}

MARGAUX_C_API psr_error_t psr_element_set_integer(psr_element_t* element, const char* name, int64_t value) {
    if (!element || !name) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return MARGAUX_OK;
}

MARGAUX_C_API psr_error_t psr_element_set_float(psr_element_t* element, const char* name, double value) {
    if (!element || !name) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, value);
    return MARGAUX_OK;
}

MARGAUX_C_API psr_error_t psr_element_set_string(psr_element_t* element, const char* name, const char* value) {
    if (!element || !name || !value) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    element->element.set(name, std::string(value));
    return MARGAUX_OK;
}

MARGAUX_C_API psr_error_t psr_element_set_null(psr_element_t* element, const char* name) {
    if (!element || !name) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    element->element.set_null(name);
    return MARGAUX_OK;
}

MARGAUX_C_API psr_error_t psr_element_set_array_integer(psr_element_t* element,
                                                        const char* name,
                                                        const int64_t* values,
                                                        int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    std::vector<int64_t> arr(values, values + count);
    element->element.set(name, arr);
    return MARGAUX_OK;
}

MARGAUX_C_API psr_error_t psr_element_set_array_float(psr_element_t* element,
                                                      const char* name,
                                                      const double* values,
                                                      int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    std::vector<double> arr(values, values + count);
    element->element.set(name, arr);
    return MARGAUX_OK;
}

MARGAUX_C_API psr_error_t psr_element_set_array_string(psr_element_t* element,
                                                       const char* name,
                                                       const char* const* values,
                                                       int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return MARGAUX_ERROR_INVALID_ARGUMENT;
    }
    std::vector<std::string> arr;
    arr.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        arr.emplace_back(values[i] ? values[i] : "");
    }
    element->element.set(name, arr);
    return MARGAUX_OK;
}

MARGAUX_C_API int psr_element_has_scalars(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_scalars() ? 1 : 0;
}

MARGAUX_C_API int psr_element_has_arrays(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_arrays() ? 1 : 0;
}

MARGAUX_C_API size_t psr_element_scalar_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.scalars().size();
}

MARGAUX_C_API size_t psr_element_array_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.arrays().size();
}

MARGAUX_C_API char* psr_element_to_string(psr_element_t* element) {
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

MARGAUX_C_API void psr_string_free(char* str) {
    delete[] str;
}

}  // extern "C"
