#ifndef MARGAUX_C_ELEMENT_H
#define MARGAUX_C_ELEMENT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct element element_t;

// Element builder
MARGAUX_C_API element_t* element_create(void);
MARGAUX_C_API void element_destroy(element_t* element);
MARGAUX_C_API void element_clear(element_t* element);

// Scalar setters
MARGAUX_C_API margaux_error_t element_set_integer(element_t* element, const char* name, int64_t value);
MARGAUX_C_API margaux_error_t element_set_float(element_t* element, const char* name, double value);
MARGAUX_C_API margaux_error_t element_set_string(element_t* element, const char* name, const char* value);
MARGAUX_C_API margaux_error_t element_set_null(element_t* element, const char* name);

// Array setters - C++ create_element routes these to vector/set tables based on schema
MARGAUX_C_API margaux_error_t element_set_array_integer(element_t* element,
                                                        const char* name,
                                                        const int64_t* values,
                                                        int32_t count);
MARGAUX_C_API margaux_error_t element_set_array_float(element_t* element,
                                                      const char* name,
                                                      const double* values,
                                                      int32_t count);
MARGAUX_C_API margaux_error_t element_set_array_string(element_t* element,
                                                       const char* name,
                                                       const char* const* values,
                                                       int32_t count);

// Accessors
MARGAUX_C_API int element_has_scalars(element_t* element);
MARGAUX_C_API int element_has_arrays(element_t* element);
MARGAUX_C_API size_t element_scalar_count(element_t* element);
MARGAUX_C_API size_t element_array_count(element_t* element);

// Pretty print (caller must free returned string with psr_string_free)
MARGAUX_C_API char* element_to_string(element_t* element);
MARGAUX_C_API void psr_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  // MARGAUX_C_ELEMENT_H
