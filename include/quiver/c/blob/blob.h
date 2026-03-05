#ifndef QUIVER_C_BLOB_H
#define QUIVER_C_BLOB_H

#include "../common.h"
#include "blob_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle type
typedef struct quiver_blob quiver_blob_t;

// Open/close
QUIVER_C_API quiver_error_t quiver_blob_open_read(const char* path, quiver_blob_t** out);
QUIVER_C_API quiver_error_t quiver_blob_open_write(const char* path, quiver_blob_metadata_t* md, quiver_blob_t** out);
QUIVER_C_API quiver_error_t quiver_blob_close(quiver_blob_t* blob);

// I/O (dimensions as parallel arrays)
QUIVER_C_API quiver_error_t quiver_blob_read(quiver_blob_t* blob,
                                             const char* const* dim_names,
                                             const int64_t* dim_values,
                                             size_t dim_count,
                                             int allow_nulls,
                                             double** out_data,
                                             size_t* out_count);
QUIVER_C_API quiver_error_t quiver_blob_write(quiver_blob_t* blob,
                                              const char* const* dim_names,
                                              const int64_t* dim_values,
                                              size_t dim_count,
                                              const double* data,
                                              size_t data_count);

// Getters
QUIVER_C_API quiver_error_t quiver_blob_get_metadata(quiver_blob_t* blob, quiver_blob_metadata_t** out);
QUIVER_C_API quiver_error_t quiver_blob_get_file_path(quiver_blob_t* blob, const char** out);

// Free
QUIVER_C_API quiver_error_t quiver_blob_free_float_array(double* data);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_BLOB_H
