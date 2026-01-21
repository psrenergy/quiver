#ifndef DECK_DATABASE_C_DATABASE_H
#define DECK_DATABASE_C_DATABASE_H

#include "common.h"
#include "element.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log levels for console output
typedef enum {
    DECK_DATABASE_LOG_DEBUG = 0,
    DECK_DATABASE_LOG_INFO = 1,
    DECK_DATABASE_LOG_WARN = 2,
    DECK_DATABASE_LOG_ERROR = 3,
    DECK_DATABASE_LOG_OFF = 4,
} margaux_log_level_t;

// Database options
typedef struct {
    int read_only;
    margaux_log_level_t console_level;
} database_options_t;

// Attribute data structure
typedef enum {
    DECK_DATABASE_DATA_STRUCTURE_SCALAR = 0,
    DECK_DATABASE_DATA_STRUCTURE_VECTOR = 1,
    DECK_DATABASE_DATA_STRUCTURE_SET = 2
} margaux_data_structure_t;

// Attribute data types
typedef enum {
    DECK_DATABASE_DATA_TYPE_INTEGER = 0,
    DECK_DATABASE_DATA_TYPE_FLOAT = 1,
    DECK_DATABASE_DATA_TYPE_STRING = 2
} margaux_data_type_t;

// Returns default options
DECK_DATABASE_C_API database_options_t database_options_default(void);

// Opaque handle type
typedef struct database database_t;

// Database lifecycle
DECK_DATABASE_C_API database_t* database_open(const char* path, const database_options_t* options);
DECK_DATABASE_C_API database_t*
database_from_migrations(const char* db_path, const char* migrations_path, const database_options_t* options);
DECK_DATABASE_C_API database_t*
database_from_schema(const char* db_path, const char* schema_path, const database_options_t* options);
DECK_DATABASE_C_API void database_close(database_t* db);
DECK_DATABASE_C_API int database_is_healthy(database_t* db);
DECK_DATABASE_C_API const char* database_path(database_t* db);

// Version
DECK_DATABASE_C_API int64_t database_current_version(database_t* db);

// Element operations
DECK_DATABASE_C_API int64_t database_create_element(database_t* db, const char* collection, element_t* element);
DECK_DATABASE_C_API margaux_error_t database_update_element(database_t* db,
                                                      const char* collection,
                                                      int64_t id,
                                                      const element_t* element);
DECK_DATABASE_C_API margaux_error_t database_delete_element_by_id(database_t* db, const char* collection, int64_t id);

// Relation operations
DECK_DATABASE_C_API margaux_error_t database_set_scalar_relation(database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           const char* from_label,
                                                           const char* to_label);

DECK_DATABASE_C_API margaux_error_t database_read_scalar_relation(database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            char*** out_values,
                                                            size_t* out_count);

// Read scalar attributes
DECK_DATABASE_C_API margaux_error_t database_read_scalar_integers(database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t** out_values,
                                                            size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_scalar_floats(database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          double** out_values,
                                                          size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_scalar_strings(database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           char*** out_values,
                                                           size_t* out_count);

// Read vector attributes
DECK_DATABASE_C_API margaux_error_t database_read_vector_integers(database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t*** out_vectors,
                                                            size_t** out_sizes,
                                                            size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_vector_floats(database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          double*** out_vectors,
                                                          size_t** out_sizes,
                                                          size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_vector_strings(database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           char**** out_vectors,
                                                           size_t** out_sizes,
                                                           size_t* out_count);

// Read set attributes (same structure as vectors, uses same free functions)
DECK_DATABASE_C_API margaux_error_t database_read_set_integers(database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t*** out_sets,
                                                         size_t** out_sizes,
                                                         size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_set_floats(database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       double*** out_sets,
                                                       size_t** out_sizes,
                                                       size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_set_strings(database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        char**** out_sets,
                                                        size_t** out_sizes,
                                                        size_t* out_count);

// Read scalar attributes by element ID
DECK_DATABASE_C_API margaux_error_t database_read_scalar_integers_by_id(database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t* out_value,
                                                                  int* out_has_value);

DECK_DATABASE_C_API margaux_error_t database_read_scalar_floats_by_id(database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double* out_value,
                                                                int* out_has_value);

DECK_DATABASE_C_API margaux_error_t database_read_scalar_strings_by_id(database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 char** out_value,
                                                                 int* out_has_value);

// Read vector attributes by element ID
DECK_DATABASE_C_API margaux_error_t database_read_vector_integers_by_id(database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t** out_values,
                                                                  size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_vector_floats_by_id(database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double** out_values,
                                                                size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_vector_strings_by_id(database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 char*** out_values,
                                                                 size_t* out_count);

// Read set attributes by element ID
DECK_DATABASE_C_API margaux_error_t database_read_set_integers_by_id(database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               int64_t id,
                                                               int64_t** out_values,
                                                               size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_set_floats_by_id(database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             double** out_values,
                                                             size_t* out_count);

DECK_DATABASE_C_API margaux_error_t database_read_set_strings_by_id(database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              char*** out_values,
                                                              size_t* out_count);

// Read element IDs
DECK_DATABASE_C_API margaux_error_t database_read_element_ids(database_t* db,
                                                        const char* collection,
                                                        int64_t** out_ids,
                                                        size_t* out_count);

// Attribute type query
DECK_DATABASE_C_API margaux_error_t database_get_attribute_type(database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          margaux_data_structure_t* out_data_structure,
                                                          margaux_data_type_t* out_data_type);

// Update scalar attributes (by element ID)
DECK_DATABASE_C_API margaux_error_t database_update_scalar_integer(database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             int64_t value);

DECK_DATABASE_C_API margaux_error_t
database_update_scalar_float(database_t* db, const char* collection, const char* attribute, int64_t id, double value);

DECK_DATABASE_C_API margaux_error_t database_update_scalar_string(database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t id,
                                                            const char* value);

// Update vector attributes (by element ID) - replaces entire vector
DECK_DATABASE_C_API margaux_error_t database_update_vector_integers(database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              const int64_t* values,
                                                              size_t count);

DECK_DATABASE_C_API margaux_error_t database_update_vector_floats(database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t id,
                                                            const double* values,
                                                            size_t count);

DECK_DATABASE_C_API margaux_error_t database_update_vector_strings(database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             const char* const* values,
                                                             size_t count);

// Update set attributes (by element ID) - replaces entire set
DECK_DATABASE_C_API margaux_error_t database_update_set_integers(database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           int64_t id,
                                                           const int64_t* values,
                                                           size_t count);

DECK_DATABASE_C_API margaux_error_t database_update_set_floats(database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         const double* values,
                                                         size_t count);

DECK_DATABASE_C_API margaux_error_t database_update_set_strings(database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          const char* const* values,
                                                          size_t count);

// Memory cleanup for read results
DECK_DATABASE_C_API void margaux_free_integer_array(int64_t* values);
DECK_DATABASE_C_API void margaux_free_float_array(double* values);
DECK_DATABASE_C_API void margaux_free_string_array(char** values, size_t count);

// Memory cleanup for vector read results
DECK_DATABASE_C_API void margaux_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count);
DECK_DATABASE_C_API void margaux_free_float_vectors(double** vectors, size_t* sizes, size_t count);
DECK_DATABASE_C_API void margaux_free_string_vectors(char*** vectors, size_t* sizes, size_t count);

#ifdef __cplusplus
}
#endif

#endif  // DECK_DATABASE_C_DATABASE_H
