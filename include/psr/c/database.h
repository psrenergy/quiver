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
PSR_C_API psr_database_t*
psr_database_from_schema(const char* db_path, const char* schema_path, const psr_database_options_t* options);
PSR_C_API void psr_database_close(psr_database_t* db);
PSR_C_API int psr_database_is_healthy(psr_database_t* db);
PSR_C_API const char* psr_database_path(psr_database_t* db);

// Version
PSR_C_API int64_t psr_database_current_version(psr_database_t* db);

// Element operations (requires psr_element_t from element.h)
typedef struct psr_element psr_element_t;
PSR_C_API int64_t psr_database_create_element(psr_database_t* db, const char* collection, psr_element_t* element);

// Read scalar attributes
PSR_C_API psr_error_t psr_database_read_scalar_ints(psr_database_t* db,
                                                    const char* collection,
                                                    const char* attribute,
                                                    int64_t** out_values,
                                                    size_t* out_count);

PSR_C_API psr_error_t psr_database_read_scalar_doubles(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       double** out_values,
                                                       size_t* out_count);

PSR_C_API psr_error_t psr_database_read_scalar_strings(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       char*** out_values,
                                                       size_t* out_count);

// Read vector attributes
PSR_C_API psr_error_t psr_database_read_vector_ints(psr_database_t* db,
                                                    const char* collection,
                                                    const char* attribute,
                                                    int64_t*** out_vectors,
                                                    size_t** out_sizes,
                                                    size_t* out_count);

PSR_C_API psr_error_t psr_database_read_vector_doubles(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       double*** out_vectors,
                                                       size_t** out_sizes,
                                                       size_t* out_count);

PSR_C_API psr_error_t psr_database_read_vector_strings(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       char**** out_vectors,
                                                       size_t** out_sizes,
                                                       size_t* out_count);

// Read set attributes (same structure as vectors, uses same free functions)
PSR_C_API psr_error_t psr_database_read_set_ints(psr_database_t* db,
                                                 const char* collection,
                                                 const char* attribute,
                                                 int64_t*** out_sets,
                                                 size_t** out_sizes,
                                                 size_t* out_count);

PSR_C_API psr_error_t psr_database_read_set_doubles(psr_database_t* db,
                                                    const char* collection,
                                                    const char* attribute,
                                                    double*** out_sets,
                                                    size_t** out_sizes,
                                                    size_t* out_count);

PSR_C_API psr_error_t psr_database_read_set_strings(psr_database_t* db,
                                                    const char* collection,
                                                    const char* attribute,
                                                    char**** out_sets,
                                                    size_t** out_sizes,
                                                    size_t* out_count);

// Memory cleanup for read results
PSR_C_API void psr_free_int_array(int64_t* values);
PSR_C_API void psr_free_double_array(double* values);
PSR_C_API void psr_free_string_array(char** values, size_t count);

// Memory cleanup for vector read results
PSR_C_API void psr_free_int_vectors(int64_t** vectors, size_t* sizes, size_t count);
PSR_C_API void psr_free_double_vectors(double** vectors, size_t* sizes, size_t count);
PSR_C_API void psr_free_string_vectors(char*** vectors, size_t* sizes, size_t count);

#ifdef __cplusplus
}
#endif

#endif  // PSR_C_DATABASE_H
