#ifndef PSR_C_DATABASE_H
#define PSR_C_DATABASE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Platform-specific export macros
#ifdef _WIN32
#ifdef PSR_DATABASE_C_EXPORTS
#define PSR_C_API __declspec(dllexport)
#else
#define PSR_C_API __declspec(dllimport)
#endif
#else
#define PSR_C_API __attribute__((visibility("default")))
#endif

// Error codes
typedef enum {
    PSR_OK = 0,
    PSR_ERROR_INVALID_ARGUMENT = -1,
    PSR_ERROR_DATABASE = -2,
} psr_error_t;

// Log levels for console output
typedef enum {
    PSR_LOG_DEBUG = 0,
    PSR_LOG_INFO = 1,
    PSR_LOG_WARN = 2,
    PSR_LOG_ERROR = 3,
    PSR_LOG_OFF = 4,
} psr_log_level_t;

// Opaque handle type
typedef struct psr_database psr_database_t;

// Database functions
PSR_C_API psr_database_t* psr_database_open(const char* path, psr_log_level_t console_level, psr_error_t* error);
PSR_C_API void psr_database_close(psr_database_t* db);
PSR_C_API int psr_database_is_open(psr_database_t* db);
PSR_C_API const char* psr_database_path(psr_database_t* db);

// Utility functions
PSR_C_API const char* psr_error_string(psr_error_t error);
PSR_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_DATABASE_H
