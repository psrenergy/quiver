#ifndef PSR_C_DATABASE_H
#define PSR_C_DATABASE_H

#include "platform.h"
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log levels for console output
typedef enum {
    PSR_LOG_DEBUG = 0,
    PSR_LOG_INFO = 1,
    PSR_LOG_WARN = 2,
    PSR_LOG_ERROR = 3,
    PSR_LOG_OFF = 4,
} psr_log_level_t;

// Database options
typedef struct {
    int read_only;
    psr_log_level_t console_level;
} psr_database_options_t;

// Returns default options
PSR_C_API psr_database_options_t psr_database_options_default(void);

// Opaque handle type
typedef struct psr_database psr_database_t;

// Database lifecycle
PSR_C_API psr_database_t* psr_database_open(const char* path, const psr_database_options_t* options);
PSR_C_API psr_database_t*
psr_database_from_migrations(const char* db_path, const char* migrations_path, const psr_database_options_t* options);
PSR_C_API psr_database_t*
psr_database_from_schema(const char* db_path, const char* schema_path, const psr_database_options_t* options);
PSR_C_API void psr_database_close(psr_database_t* db);
PSR_C_API int psr_database_is_healthy(psr_database_t* db);
PSR_C_API const char* psr_database_path(psr_database_t* db);

// Version and migration
PSR_C_API int64_t psr_database_current_version(psr_database_t* db);
PSR_C_API psr_error_t psr_database_set_version(psr_database_t* db, int64_t version);
PSR_C_API psr_error_t psr_database_migrate_up(psr_database_t* db, const char* migrations_path);

// Transaction management
PSR_C_API psr_error_t psr_database_begin_transaction(psr_database_t* db);
PSR_C_API psr_error_t psr_database_commit(psr_database_t* db);
PSR_C_API psr_error_t psr_database_rollback(psr_database_t* db);

// Element operations (requires psr_element_t from element.h)
typedef struct psr_element psr_element_t;
PSR_C_API int64_t psr_database_create_element(psr_database_t* db, const char* collection, psr_element_t* element);

// Scalar parameter reading - arrays (return count, -1 on error)
PSR_C_API int64_t psr_database_read_scalar_parameters_double(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             double** out_values);
PSR_C_API int64_t psr_database_read_scalar_parameters_string(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             char*** out_values);
PSR_C_API int64_t psr_database_read_scalar_parameters_int(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t** out_values);

// Scalar parameter reading - single value
PSR_C_API psr_error_t psr_database_read_scalar_parameter_double(psr_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                const char* label,
                                                                double* out_value,
                                                                int* is_null);
PSR_C_API psr_error_t psr_database_read_scalar_parameter_string(psr_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                const char* label,
                                                                char** out_value);
PSR_C_API psr_error_t psr_database_read_scalar_parameter_int(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             const char* label,
                                                             int64_t* out_value,
                                                             int* is_null);

// Array memory management
PSR_C_API void psr_double_array_free(double* arr);
PSR_C_API void psr_string_array_free(char** arr, size_t count);
PSR_C_API void psr_int_array_free(int64_t* arr);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_DATABASE_H
