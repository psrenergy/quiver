#ifndef QUIVER_C_CSV_CONVERTER_H
#define QUIVER_C_CSV_CONVERTER_H

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

QUIVER_C_API quiver_error_t quiver_csv_converter_bin_to_csv(const char* path, int aggregate_time_dimensions);
QUIVER_C_API quiver_error_t quiver_csv_converter_csv_to_bin(const char* path);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_CSV_CONVERTER_H
