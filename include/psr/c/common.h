#ifndef QUIVER_C_COMMON_H
#define QUIVER_C_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef QUIVER_DATABASE_C_EXPORTS
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
    QUIVER_ERROR_INVALID_ARGUMENT = -1,
    QUIVER_ERROR_DATABASE = -2,
    QUIVER_ERROR_MIGRATION = -3,
    QUIVER_ERROR_SCHEMA = -4,
    QUIVER_ERROR_CREATE_ELEMENT = -5,
    QUIVER_ERROR_NOT_FOUND = -6,
} quiver_error_t;

// Utility functions
QUIVER_C_API const char* quiver_error_string(quiver_error_t error);
QUIVER_C_API const char* quiver_version(void);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_COMMON_H
