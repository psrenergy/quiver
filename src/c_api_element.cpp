#include "psr/c/element.h"
#include "psr/element.h"

#include <new>
#include <string>
#include <vector>

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
