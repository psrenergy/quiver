#include "psr/c/element.h"
#include "psr/element.h"

#include <cstring>
#include <new>
#include <string>
#include <vector>

struct psr_element {
    psr::Element element;
};

struct psr_vector_group {
    psr::VectorGroup group;
};

struct psr_set_group {
    psr::SetGroup group;
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

// Vector group builder
PSR_C_API psr_vector_group_t* psr_vector_group_create(void) {
    try {
        return new psr_vector_group();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

PSR_C_API void psr_vector_group_destroy(psr_vector_group_t* group) {
    delete group;
}

PSR_C_API psr_error_t psr_vector_group_add_row(psr_vector_group_t* group) {
    if (!group) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.emplace_back();
    return PSR_OK;
}

PSR_C_API psr_error_t psr_vector_group_set_int(psr_vector_group_t* group, const char* column, int64_t value) {
    if (!group || !column || group->group.empty()) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.back()[column] = value;
    return PSR_OK;
}

PSR_C_API psr_error_t psr_vector_group_set_double(psr_vector_group_t* group, const char* column, double value) {
    if (!group || !column || group->group.empty()) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.back()[column] = value;
    return PSR_OK;
}

PSR_C_API psr_error_t psr_vector_group_set_string(psr_vector_group_t* group, const char* column, const char* value) {
    if (!group || !column || !value || group->group.empty()) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.back()[column] = std::string(value);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_add_vector_group(psr_element_t* element,
                                                   const char* name,
                                                   psr_vector_group_t* group) {
    if (!element || !name || !group) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.add_vector_group(name, std::move(group->group));
    return PSR_OK;
}

// Set group builder
PSR_C_API psr_set_group_t* psr_set_group_create(void) {
    try {
        return new psr_set_group();
    } catch (const std::bad_alloc&) {
        return nullptr;
    }
}

PSR_C_API void psr_set_group_destroy(psr_set_group_t* group) {
    delete group;
}

PSR_C_API psr_error_t psr_set_group_add_row(psr_set_group_t* group) {
    if (!group) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.emplace_back();
    return PSR_OK;
}

PSR_C_API psr_error_t psr_set_group_set_int(psr_set_group_t* group, const char* column, int64_t value) {
    if (!group || !column || group->group.empty()) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.back()[column] = value;
    return PSR_OK;
}

PSR_C_API psr_error_t psr_set_group_set_double(psr_set_group_t* group, const char* column, double value) {
    if (!group || !column || group->group.empty()) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.back()[column] = value;
    return PSR_OK;
}

PSR_C_API psr_error_t psr_set_group_set_string(psr_set_group_t* group, const char* column, const char* value) {
    if (!group || !column || !value || group->group.empty()) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    group->group.back()[column] = std::string(value);
    return PSR_OK;
}

PSR_C_API psr_error_t psr_element_add_set_group(psr_element_t* element, const char* name, psr_set_group_t* group) {
    if (!element || !name || !group) {
        return PSR_ERROR_INVALID_ARGUMENT;
    }
    element->element.add_set_group(name, std::move(group->group));
    return PSR_OK;
}

PSR_C_API int psr_element_has_scalars(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_scalars() ? 1 : 0;
}

PSR_C_API int psr_element_has_vector_groups(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_vector_groups() ? 1 : 0;
}

PSR_C_API int psr_element_has_set_groups(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.has_set_groups() ? 1 : 0;
}

PSR_C_API size_t psr_element_scalar_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.scalars().size();
}

PSR_C_API size_t psr_element_vector_group_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.vector_groups().size();
}

PSR_C_API size_t psr_element_set_group_count(psr_element_t* element) {
    if (!element) {
        return 0;
    }
    return element->element.set_groups().size();
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
