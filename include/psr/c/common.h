#ifndef PSR_C_COMMON_H
#define PSR_C_COMMON_H

#include "platform.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Error codes
typedef enum {
    PSR_OK = 0,
    PSR_ERROR_INVALID_ARGUMENT = -1,
    PSR_ERROR_DATABASE = -2,
    PSR_ERROR_MIGRATION = -3,
    PSR_ERROR_SCHEMA = -4,
    PSR_ERROR_CREATE_ELEMENT = -5,
    PSR_ERROR_NOT_FOUND = -6,
} psr_error_t;

// Utility functions
PSR_C_API const char* psr_error_string(psr_error_t error);
PSR_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_COMMON_H
