#include "psr/c/element.h"
#include "psr/element.h"

#include <cstring>
#include <new>
#include <string>

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

PSR_C_API psr_error_t psr_element_set_array_int(psr_element_t* element,
                                                const char* name,
                                                const int64_t* values,
                                                int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    std::vector<int64_t> arr(values, values + count);
    element->element.set(name, arr);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_array_double(psr_element_t* element,
                                                   const char* name,
                                                   const double* values,
                                                   int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    std::vector<double> arr(values, values + count);
    element->element.set(name, arr);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_set_array_string(psr_element_t* element,
                                                   const char* name,
                                                   const char* const* values,
                                                   int32_t count) {
    if (!element || !name || (!values && count > 0) || count < 0) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    std::vector<std::string> arr;
    arr.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        arr.emplace_back(values[i] ? values[i] : "");
    }
    element->element.set(name, arr);
    return PSR_OK;
}

PSR_C_API int psr_element_has_scalars(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_scalars() ? 1 : 0;
}

PSR_C_API int psr_element_has_arrays(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_arrays() ? 1 : 0;
}

PSR_C_API size_t psr_element_scalar_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.scalars().size();
}

PSR_C_API size_t psr_element_array_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.arrays().size();
}

PSR_C_API char* psr_element_to_string(psr_element_t* element) {
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

PSR_C_API void psr_string_free(char* str) {
    delete[] str;
}

}  // extern "C"
