#ifndef PSR_C_ELEMENT_H
#define PSR_C_ELEMENT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle types
typedef struct psr_element psr_element_t;
typedef struct psr_vector_group psr_vector_group_t;
typedef struct psr_set_group psr_set_group_t;

// Element lifecycle
PSR_C_API psr_element_t* psr_element_create(void);
PSR_C_API void psr_element_destroy(psr_element_t* element);
PSR_C_API void psr_element_clear(psr_element_t* element);

// Scalar setters
PSR_C_API psr_error_t psr_element_set_int(psr_element_t* element, const char* name, int64_t value);
PSR_C_API psr_error_t psr_element_set_double(psr_element_t* element, const char* name, double value);
PSR_C_API psr_error_t psr_element_set_string(psr_element_t* element, const char* name, const char* value);
PSR_C_API psr_error_t psr_element_set_null(psr_element_t* element, const char* name);

// Vector group builder
PSR_C_API psr_vector_group_t* psr_vector_group_create(void);
PSR_C_API void psr_vector_group_destroy(psr_vector_group_t* group);
PSR_C_API psr_error_t psr_vector_group_add_row(psr_vector_group_t* group);
PSR_C_API psr_error_t psr_vector_group_set_int(psr_vector_group_t* group, const char* column, int64_t value);
PSR_C_API psr_error_t psr_vector_group_set_double(psr_vector_group_t* group, const char* column, double value);
PSR_C_API psr_error_t psr_vector_group_set_string(psr_vector_group_t* group, const char* column, const char* value);
PSR_C_API psr_error_t psr_element_add_vector_group(psr_element_t* element, const char* name, psr_vector_group_t* group);

// Set group builder
PSR_C_API psr_set_group_t* psr_set_group_create(void);
PSR_C_API void psr_set_group_destroy(psr_set_group_t* group);
PSR_C_API psr_error_t psr_set_group_add_row(psr_set_group_t* group);
PSR_C_API psr_error_t psr_set_group_set_int(psr_set_group_t* group, const char* column, int64_t value);
PSR_C_API psr_error_t psr_set_group_set_double(psr_set_group_t* group, const char* column, double value);
PSR_C_API psr_error_t psr_set_group_set_string(psr_set_group_t* group, const char* column, const char* value);
PSR_C_API psr_error_t psr_element_add_set_group(psr_element_t* element, const char* name, psr_set_group_t* group);

// Accessors
PSR_C_API int psr_element_has_scalars(psr_element_t* element);
PSR_C_API int psr_element_has_vector_groups(psr_element_t* element);
PSR_C_API int psr_element_has_set_groups(psr_element_t* element);
PSR_C_API size_t psr_element_scalar_count(psr_element_t* element);
PSR_C_API size_t psr_element_vector_group_count(psr_element_t* element);
PSR_C_API size_t psr_element_set_group_count(psr_element_t* element);

// Pretty print (caller must free returned string with psr_string_free)
PSR_C_API char* psr_element_to_string(psr_element_t* element);
PSR_C_API void psr_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_ELEMENT_H
