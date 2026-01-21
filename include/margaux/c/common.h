#ifndef PSR_C_COMMON_H
#define PSR_C_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef PSR_DATABASE_C_EXPORTS
#define MARGAUX_C_API __declspec(dllexport)
#else
#define MARGAUX_C_API __declspec(dllimport)
#endif
#else
#define MARGAUX_C_API __attribute__((visibility("default")))
#endif

// Error codes
typedef enum {
    MARGAUX_OK = 0,
    MARGAUX_ERROR_INVALID_ARGUMENT = -1,
    MARGAUX_ERROR_DATABASE = -2,
    MARGAUX_ERROR_MIGRATION = -3,
    MARGAUX_ERROR_SCHEMA = -4,
    MARGAUX_ERROR_CREATE_ELEMENT = -5,
    MARGAUX_ERROR_NOT_FOUND = -6,
} margaux_error_t;

// Utility functions
MARGAUX_C_API const char* psr_error_string(margaux_error_t error);
MARGAUX_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_COMMON_H
