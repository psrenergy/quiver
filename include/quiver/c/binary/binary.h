#ifndef QUIVER_C_BINARY_H
#define QUIVER_C_BINARY_H

#include "../common.h"
#include "binary_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_binary quiver_binary_t;

// Open/close
QUIVER_C_API quiver_error_t quiver_binary_open_read(const char* path, quiver_binary_t** out);
QUIVER_C_API quiver_error_t quiver_binary_open_write(const char* path,
                                                     quiver_binary_metadata_t* md,
                                                     quiver_binary_t** out);
QUIVER_C_API quiver_error_t quiver_binary_close(quiver_binary_t* binary);

// I/O (dimensions as parallel arrays)
QUIVER_C_API quiver_error_t quiver_binary_read(quiver_binary_t* binary,
                                               const char* const* dim_names,
                                               const int64_t* dim_values,
                                               size_t dim_count,
                                               int allow_nulls,
                                               double** out_data,
                                               size_t* out_count);
QUIVER_C_API quiver_error_t quiver_binary_write(quiver_binary_t* binary,
                                                const char* const* dim_names,
                                                const int64_t* dim_values,
                                                size_t dim_count,
                                                const double* data,
                                                size_t data_count);

// Getters
QUIVER_C_API quiver_error_t quiver_binary_get_metadata(quiver_binary_t* binary, quiver_binary_metadata_t** out);
QUIVER_C_API quiver_error_t quiver_binary_get_file_path(quiver_binary_t* binary, char** out);

// Free
QUIVER_C_API quiver_error_t quiver_binary_free_string(char* str);
QUIVER_C_API quiver_error_t quiver_binary_free_float_array(double* data);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_BINARY_H
