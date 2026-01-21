#ifndef MARGAUX_C_ELEMENT_H
#define MARGAUX_C_ELEMENT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct margaux_element margaux_element_t;

// Element builder
MARGAUX_C_API margaux_element_t* margaux_element_create(void);
MARGAUX_C_API void margaux_element_destroy(margaux_element_t* element);
MARGAUX_C_API void margaux_element_clear(margaux_element_t* element);

// Scalar setters
MARGAUX_C_API margaux_error_t margaux_element_set_integer(margaux_element_t* element, const char* name, int64_t value);
MARGAUX_C_API margaux_error_t margaux_element_set_float(margaux_element_t* element, const char* name, double value);
MARGAUX_C_API margaux_error_t margaux_element_set_string(margaux_element_t* element, const char* name, const char* value);
MARGAUX_C_API margaux_error_t margaux_element_set_null(margaux_element_t* element, const char* name);

// Array setters - C++ create_element routes these to vector/set tables based on schema
MARGAUX_C_API margaux_error_t margaux_element_set_array_integer(margaux_element_t* element,
                                                    const char* name,
                                                    const int64_t* values,
                                                    int32_t count);
MARGAUX_C_API margaux_error_t margaux_element_set_array_float(margaux_element_t* element,
                                                  const char* name,
                                                  const double* values,
                                                  int32_t count);
MARGAUX_C_API margaux_error_t margaux_element_set_array_string(margaux_element_t* element,
                                                   const char* name,
                                                   const char* const* values,
                                                   int32_t count);

// Accessors
MARGAUX_C_API int margaux_element_has_scalars(margaux_element_t* element);
MARGAUX_C_API int margaux_element_has_arrays(margaux_element_t* element);
MARGAUX_C_API size_t margaux_element_scalar_count(margaux_element_t* element);
MARGAUX_C_API size_t margaux_element_array_count(margaux_element_t* element);

// Pretty print (caller must free returned string with margaux_string_free)
MARGAUX_C_API char* margaux_element_to_string(margaux_element_t* element);
MARGAUX_C_API void margaux_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  // MARGAUX_C_ELEMENT_H
