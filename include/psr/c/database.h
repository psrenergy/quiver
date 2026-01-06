#ifndef PSR_C_DATABASE_H
#define PSR_C_DATABASE_H

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

// Schema operations
PSR_C_API psr_error_t psr_database_apply_schema(psr_database_t* db, const char* schema_path);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_DATABASE_H
