#ifndef PSR_C_ELEMENT_H
#define PSR_C_ELEMENT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct psr_element psr_element_t;

// Element lifecycle
PSR_C_API psr_element_t* psr_element_create(void);
PSR_C_API void psr_element_destroy(psr_element_t* element);
PSR_C_API void psr_element_clear(psr_element_t* element);

// Scalar setters
PSR_C_API psr_error_t psr_element_set_int(psr_element_t* element, const char* name, int64_t value);
PSR_C_API psr_error_t psr_element_set_double(psr_element_t* element, const char* name, double value);
PSR_C_API psr_error_t psr_element_set_string(psr_element_t* element, const char* name, const char* value);
PSR_C_API psr_error_t psr_element_set_null(psr_element_t* element, const char* name);

// Vector setters
PSR_C_API psr_error_t psr_element_set_vector_int(psr_element_t* element,
                                                 const char* name,
                                                 const int64_t* values,
                                                 size_t count);
PSR_C_API psr_error_t psr_element_set_vector_double(psr_element_t* element,
                                                    const char* name,
                                                    const double* values,
                                                    size_t count);
PSR_C_API psr_error_t psr_element_set_vector_string(psr_element_t* element,
                                                    const char* name,
                                                    const char** values,
                                                    size_t count);

// Accessors
PSR_C_API int psr_element_has_scalars(psr_element_t* element);
PSR_C_API int psr_element_has_vectors(psr_element_t* element);
PSR_C_API size_t psr_element_scalar_count(psr_element_t* element);
PSR_C_API size_t psr_element_vector_count(psr_element_t* element);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_ELEMENT_H
