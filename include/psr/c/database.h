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

// Attribute data structure
typedef enum {
    PSR_DATA_STRUCTURE_SCALAR = 0,
    PSR_DATA_STRUCTURE_VECTOR = 1,
    PSR_DATA_STRUCTURE_SET = 2
} psr_data_structure_t;

// Attribute data types
typedef enum { PSR_DATA_TYPE_INTEGER = 0, PSR_DATA_TYPE_REAL = 1, PSR_DATA_TYPE_TEXT = 2 } psr_data_type_t;

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
PSR_C_API psr_error_t psr_database_update_element(psr_database_t* db,
                                                  const char* collection,
                                                  int64_t id,
                                                  const psr_element_t* element);
PSR_C_API psr_error_t psr_database_delete_element_by_id(psr_database_t* db, const char* collection, int64_t id);

// Relation operations
PSR_C_API psr_error_t psr_database_set_scalar_relation(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       const char* from_label,
                                                       const char* to_label);

PSR_C_API psr_error_t psr_database_read_scalar_relation(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        char*** out_values,
                                                        size_t* out_count);

// Read scalar attributes
PSR_C_API psr_error_t psr_database_read_scalar_integers(psr_database_t* db,
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
PSR_C_API psr_error_t psr_database_read_vector_integers(psr_database_t* db,
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
PSR_C_API psr_error_t psr_database_read_set_integers(psr_database_t* db,
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

// Read scalar attributes by element ID
PSR_C_API psr_error_t psr_database_read_scalar_integers_by_id(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              int64_t* out_value,
                                                              int* out_has_value);

PSR_C_API psr_error_t psr_database_read_scalar_doubles_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             double* out_value,
                                                             int* out_has_value);

PSR_C_API psr_error_t psr_database_read_scalar_strings_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             char** out_value,
                                                             int* out_has_value);

// Read vector attributes by element ID
PSR_C_API psr_error_t psr_database_read_vector_integers_by_id(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              int64_t** out_values,
                                                              size_t* out_count);

PSR_C_API psr_error_t psr_database_read_vector_doubles_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             double** out_values,
                                                             size_t* out_count);

PSR_C_API psr_error_t psr_database_read_vector_strings_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             char*** out_values,
                                                             size_t* out_count);

// Read set attributes by element ID
PSR_C_API psr_error_t psr_database_read_set_integers_by_id(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           int64_t id,
                                                           int64_t** out_values,
                                                           size_t* out_count);

PSR_C_API psr_error_t psr_database_read_set_doubles_by_id(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          double** out_values,
                                                          size_t* out_count);

PSR_C_API psr_error_t psr_database_read_set_strings_by_id(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          char*** out_values,
                                                          size_t* out_count);

// Read element IDs
PSR_C_API psr_error_t psr_database_read_element_ids(psr_database_t* db,
                                                    const char* collection,
                                                    int64_t** out_ids,
                                                    size_t* out_count);

// Attribute type query
PSR_C_API psr_error_t psr_database_get_attribute_type(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      psr_data_structure_t* out_data_structure,
                                                      psr_data_type_t* out_data_type);

// Update scalar attributes (by element ID)
PSR_C_API psr_error_t psr_database_update_scalar_integer(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         int64_t value);

PSR_C_API psr_error_t psr_database_update_scalar_double(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t id,
                                                        double value);

PSR_C_API psr_error_t psr_database_update_scalar_string(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t id,
                                                        const char* value);

// Update vector attributes (by element ID) - replaces entire vector
PSR_C_API psr_error_t psr_database_update_vector_integers(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          const int64_t* values,
                                                          size_t count);

PSR_C_API psr_error_t psr_database_update_vector_doubles(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         const double* values,
                                                         size_t count);

PSR_C_API psr_error_t psr_database_update_vector_strings(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         const char* const* values,
                                                         size_t count);

// Update set attributes (by element ID) - replaces entire set
PSR_C_API psr_error_t psr_database_update_set_integers(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       int64_t id,
                                                       const int64_t* values,
                                                       size_t count);

PSR_C_API psr_error_t psr_database_update_set_doubles(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      int64_t id,
                                                      const double* values,
                                                      size_t count);

PSR_C_API psr_error_t psr_database_update_set_strings(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      int64_t id,
                                                      const char* const* values,
                                                      size_t count);

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
