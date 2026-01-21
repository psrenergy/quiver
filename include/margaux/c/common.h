#ifndef DECK_DATABASE_C_COMMON_H
#define DECK_DATABASE_C_COMMON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef DECK_DATABASE_DATABASE_C_EXPORTS
#define DECK_DATABASE_C_API __declspec(dllexport)
#else
#define DECK_DATABASE_C_API __declspec(dllimport)
#endif
#else
#define DECK_DATABASE_C_API __attribute__((visibility("default")))
#endif

// Error codes
typedef enum {
    DECK_DATABASE_OK = 0,
    DECK_DATABASE_ERROR_INVALID_ARGUMENT = -1,
    DECK_DATABASE_ERROR_DATABASE = -2,
    DECK_DATABASE_ERROR_MIGRATION = -3,
    DECK_DATABASE_ERROR_SCHEMA = -4,
    DECK_DATABASE_ERROR_CREATE_ELEMENT = -5,
    DECK_DATABASE_ERROR_NOT_FOUND = -6,
} margaux_error_t;

// Utility functions
DECK_DATABASE_C_API const char* margaux_error_string(margaux_error_t error);
DECK_DATABASE_C_API const char* margaux_version(void);

#ifdef __cplusplus
}
#endif

#endif  // DECK_DATABASE_C_COMMON_H
