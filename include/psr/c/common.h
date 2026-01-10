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
    PSR_ERROR_MIGRATION = -3,
    PSR_ERROR_SCHEMA = -4,
    PSR_ERROR_CREATE_ELEMENT = -5,
    PSR_ERROR_NOT_FOUND = -6,
} psr_error_t;

// Value type tags for tagged union
typedef enum {
    PSR_VALUE_NULL = 0,
    PSR_VALUE_INT64 = 1,
    PSR_VALUE_DOUBLE = 2,
    PSR_VALUE_STRING = 3,
    PSR_VALUE_ARRAY = 4,
} psr_value_type_t;

// Forward declaration for recursive struct
typedef struct psr_value psr_value_t;

// Tagged union for dynamic values
struct psr_value {
    psr_value_type_t type;
    union {
        int64_t int_value;
        double double_value;
        char* string_value;
        struct {
            psr_value_t* elements;
            size_t count;
        } array_value;
    } data;
};

// Result container for read operations
typedef struct {
    psr_error_t error;
    psr_value_t* values;
    size_t count;
} psr_read_result_t;

// Utility functions
PSR_C_API const char* psr_error_string(psr_error_t error);
PSR_C_API const char* psr_version(void);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_COMMON_H
