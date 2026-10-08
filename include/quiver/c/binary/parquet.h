#ifndef QUIVER_C_BINARY_PARQUET_H
#define QUIVER_C_BINARY_PARQUET_H

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

QUIVER_C_API quiver_error_t quiver_bin_to_parquet(const char* path);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_BINARY_PARQUET_H
