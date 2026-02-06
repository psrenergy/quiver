#ifndef QUIVER_C_OPTIONS_H
#define QUIVER_C_OPTIONS_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    QUIVER_LOG_DEBUG = 0,
    QUIVER_LOG_INFO = 1,
    QUIVER_LOG_WARN = 2,
    QUIVER_LOG_ERROR = 3,
    QUIVER_LOG_OFF = 4,
} quiver_log_level_t;

typedef struct {
    int read_only;
    quiver_log_level_t console_level;
} quiver_database_options_t;

// Returns default options
QUIVER_C_API quiver_database_options_t quiver_database_options_default(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_OPTIONS_H
