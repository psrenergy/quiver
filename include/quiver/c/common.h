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

// Error message capture - returns detailed error message from last operation
// Returns empty string if no error occurred. Thread-local storage.
QUIVER_C_API const char* quiver_get_last_error(void);
QUIVER_C_API void quiver_clear_last_error(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_COMMON_H
