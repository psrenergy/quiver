#ifndef QUIVER_C_COMMON_H
#define QUIVER_C_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef QUIVER_C_EXPORTS
#define QUIVER_C_API __declspec(dllexport)
#else
#define QUIVER_C_API __declspec(dllimport)
#endif
#else
#define QUIVER_C_API __attribute__((visibility("default")))
#endif

// Error codes
typedef enum {
    QUIVER_OK = 0,
    QUIVER_ERROR = 1,
} quiver_error_t;

// Utility functions
QUIVER_C_API const char* quiver_version(void);

// Message of the most recent failed call on this thread (thread-local storage).
// A successful call does not reset it, so read it only after a call returns QUIVER_ERROR.
QUIVER_C_API const char* quiver_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_COMMON_H
