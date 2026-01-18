#ifndef PSR_C_ELEMENT_H
#define PSR_C_ELEMENT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct psr_element psr_element_t;

// Element builder
PSR_C_API psr_element_t* psr_element_create(void);
PSR_C_API void psr_element_destroy(psr_element_t* element);
PSR_C_API void psr_element_clear(psr_element_t* element);

// Scalar setters
PSR_C_API psr_error_t psr_element_set_integer(psr_element_t* element, const char* name, int64_t value);
PSR_C_API psr_error_t psr_element_set_float(psr_element_t* element, const char* name, double value);
PSR_C_API psr_error_t psr_element_set_string(psr_element_t* element, const char* name, const char* value);
PSR_C_API psr_error_t psr_element_set_null(psr_element_t* element, const char* name);

// Array setters - C++ create_element routes these to vector/set tables based on schema
PSR_C_API psr_error_t psr_element_set_array_int(psr_element_t* element,
                                                const char* name,
                                                const int64_t* values,
                                                int32_t count);
PSR_C_API psr_error_t psr_element_set_array_float(psr_element_t* element,
                                                  const char* name,
                                                  const double* values,
                                                  int32_t count);
PSR_C_API psr_error_t psr_element_set_array_string(psr_element_t* element,
                                                   const char* name,
                                                   const char* const* values,
                                                   int32_t count);

// Accessors
PSR_C_API int psr_element_has_scalars(psr_element_t* element);
PSR_C_API int psr_element_has_arrays(psr_element_t* element);
PSR_C_API size_t psr_element_scalar_count(psr_element_t* element);
PSR_C_API size_t psr_element_array_count(psr_element_t* element);

// Pretty print (caller must free returned string with psr_string_free)
PSR_C_API char* psr_element_to_string(psr_element_t* element);
PSR_C_API void psr_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_ELEMENT_H
