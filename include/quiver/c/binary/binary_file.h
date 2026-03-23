#ifndef QUIVER_C_BINARY_FILE_H
#define QUIVER_C_BINARY_FILE_H

#include "../common.h"
#include "binary_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_binary_file quiver_binary_file_t;

// Open/close
QUIVER_C_API quiver_error_t quiver_binary_file_open_read(const char* path, quiver_binary_file_t** out);
QUIVER_C_API quiver_error_t quiver_binary_file_open_write(const char* path,
                                                          quiver_binary_metadata_t* md,
                                                          quiver_binary_file_t** out);
QUIVER_C_API quiver_error_t quiver_binary_file_close(quiver_binary_file_t* binary_file);

// I/O (dimensions as parallel arrays)
QUIVER_C_API quiver_error_t quiver_binary_file_read(quiver_binary_file_t* binary_file,
                                                    const char* const* dim_names,
                                                    const int64_t* dim_values,
                                                    size_t dim_count,
                                                    int allow_nulls,
                                                    double** out_data,
                                                    size_t* out_count);
QUIVER_C_API quiver_error_t quiver_binary_file_write(quiver_binary_file_t* binary_file,
                                                     const char* const* dim_names,
                                                     const int64_t* dim_values,
                                                     size_t dim_count,
                                                     const double* data,
                                                     size_t data_count);

// Getters
QUIVER_C_API quiver_error_t quiver_binary_file_get_metadata(quiver_binary_file_t* binary_file,
                                                            quiver_binary_metadata_t** out);
QUIVER_C_API quiver_error_t quiver_binary_file_get_file_path(quiver_binary_file_t* binary_file, char** out);

// Free
QUIVER_C_API quiver_error_t quiver_binary_file_free_string(char* str);
QUIVER_C_API quiver_error_t quiver_binary_file_free_float_array(double* data);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_BINARY_FILE_H
