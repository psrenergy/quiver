#ifndef QUIVER_C_ELEMENT_H
#define QUIVER_C_ELEMENT_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_element quiver_element_t;

// Element builder
QUIVER_C_API quiver_error_t quiver_element_create(quiver_element_t** out_element);
QUIVER_C_API quiver_error_t quiver_element_destroy(quiver_element_t* element);
QUIVER_C_API quiver_error_t quiver_element_clear(quiver_element_t* element);

// Scalar setters
QUIVER_C_API quiver_error_t quiver_element_set_integer(quiver_element_t* element, const char* name, int64_t value);
QUIVER_C_API quiver_error_t quiver_element_set_float(quiver_element_t* element, const char* name, double value);
QUIVER_C_API quiver_error_t quiver_element_set_string(quiver_element_t* element, const char* name, const char* value);
QUIVER_C_API quiver_error_t quiver_element_set_null(quiver_element_t* element, const char* name);

// Array setters - C++ create_element routes these to vector/set tables based on schema
QUIVER_C_API quiver_error_t quiver_element_set_array_integer(quiver_element_t* element,
                                                             const char* name,
                                                             const int64_t* values,
                                                             int32_t count);
QUIVER_C_API quiver_error_t quiver_element_set_array_float(quiver_element_t* element,
                                                           const char* name,
                                                           const double* values,
                                                           int32_t count);
QUIVER_C_API quiver_error_t quiver_element_set_array_string(quiver_element_t* element,
                                                            const char* name,
                                                            const char* const* values,
                                                            int32_t count);

// Accessors
QUIVER_C_API quiver_error_t quiver_element_has_scalars(quiver_element_t* element, int* out_result);
QUIVER_C_API quiver_error_t quiver_element_has_arrays(quiver_element_t* element, int* out_result);
QUIVER_C_API quiver_error_t quiver_element_scalar_count(quiver_element_t* element, size_t* out_count);
QUIVER_C_API quiver_error_t quiver_element_array_count(quiver_element_t* element, size_t* out_count);

// Pretty print (caller must free returned string with quiver_string_free)
QUIVER_C_API quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
QUIVER_C_API quiver_error_t quiver_string_free(char* str);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_ELEMENT_H
