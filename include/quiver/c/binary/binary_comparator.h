#ifndef QUIVER_C_BINARY_COMPARATOR_H
#define QUIVER_C_BINARY_COMPARATOR_H

#include "../common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Compare status codes
typedef enum {
    QUIVER_COMPARE_FILE_MATCH = 0,
    QUIVER_COMPARE_METADATA_MISMATCH = 1,
    QUIVER_COMPARE_DATA_MISMATCH = 2,
} quiver_compare_status_t;

// Compare options
typedef struct {
    double absolute_tolerance;
    double relative_tolerance;
    int detailed_report;
    int max_report_lines;
} quiver_compare_options_t;

QUIVER_C_API quiver_compare_options_t quiver_compare_options_default(void);

// Compare
QUIVER_C_API quiver_error_t quiver_binary_compare_files(const char* path1,
                                                        const char* path2,
                                                        const quiver_compare_options_t* options,
                                                        quiver_compare_status_t* out_status,
                                                        char** out_report);

// Free
QUIVER_C_API quiver_error_t quiver_binary_comparator_free_string(char* str);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_BINARY_COMPARATOR_H
