#ifndef QUIVER_C_DATABASE_H
#define QUIVER_C_DATABASE_H

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log levels for console output
typedef enum {
    QUIVER_LOG_DEBUG = 0,
    QUIVER_LOG_INFO = 1,
    QUIVER_LOG_WARN = 2,
    QUIVER_LOG_ERROR = 3,
    QUIVER_LOG_OFF = 4,
} quiver_log_level_t;

// Database options
typedef struct {
    int read_only;
    quiver_log_level_t console_level;
} quiver_database_options_t;

// Attribute data structure
typedef enum {
    QUIVER_DATA_STRUCTURE_SCALAR = 0,
    QUIVER_DATA_STRUCTURE_VECTOR = 1,
    QUIVER_DATA_STRUCTURE_SET = 2
} quiver_data_structure_t;

// Attribute data types
typedef enum {
    QUIVER_DATA_TYPE_INTEGER = 0,
    QUIVER_DATA_TYPE_FLOAT = 1,
    QUIVER_DATA_TYPE_STRING = 2,
    QUIVER_DATA_TYPE_DATE_TIME = 3,
    QUIVER_DATA_TYPE_NULL = 4
} quiver_data_type_t;

// Returns default options
QUIVER_C_API quiver_database_options_t quiver_database_options_default(void);

// Opaque handle type
typedef struct quiver_database quiver_database_t;

// Database lifecycle
QUIVER_C_API quiver_database_t* quiver_database_open(const char* path, const quiver_database_options_t* options);
QUIVER_C_API quiver_database_t* quiver_database_from_migrations(const char* db_path,
                                                                const char* migrations_path,
                                                                const quiver_database_options_t* options);
QUIVER_C_API quiver_database_t*
quiver_database_from_schema(const char* db_path, const char* schema_path, const quiver_database_options_t* options);
QUIVER_C_API void quiver_database_close(quiver_database_t* db);
QUIVER_C_API int quiver_database_is_healthy(quiver_database_t* db);
QUIVER_C_API const char* quiver_database_path(quiver_database_t* db);

// Version
QUIVER_C_API int64_t quiver_database_current_version(quiver_database_t* db);

// Element operations (requires quiver_element_t from element.h)
typedef struct quiver_element quiver_element_t;
QUIVER_C_API int64_t quiver_database_create_element(quiver_database_t* db,
                                                    const char* collection,
                                                    quiver_element_t* element);
QUIVER_C_API quiver_error_t quiver_database_update_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id,
                                                           const quiver_element_t* element);
QUIVER_C_API quiver_error_t quiver_database_delete_element_by_id(quiver_database_t* db,
                                                                 const char* collection,
                                                                 int64_t id);

// Relation operations
QUIVER_C_API quiver_error_t quiver_database_set_scalar_relation(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                const char* from_label,
                                                                const char* to_label);

QUIVER_C_API quiver_error_t quiver_database_read_scalar_relation(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 char*** out_values,
                                                                 size_t* out_count);

// Read scalar attributes
QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t** out_values,
                                                                 size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_scalar_floats(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               double** out_values,
                                                               size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_scalar_strings(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                char*** out_values,
                                                                size_t* out_count);

// Read vector attributes
QUIVER_C_API quiver_error_t quiver_database_read_vector_integers(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t*** out_vectors,
                                                                 size_t** out_sizes,
                                                                 size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_vector_floats(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               double*** out_vectors,
                                                               size_t** out_sizes,
                                                               size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_vector_strings(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                char**** out_vectors,
                                                                size_t** out_sizes,
                                                                size_t* out_count);

// Read set attributes (same structure as vectors, uses same free functions)
QUIVER_C_API quiver_error_t quiver_database_read_set_integers(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t*** out_sets,
                                                              size_t** out_sizes,
                                                              size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_set_floats(quiver_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            double*** out_sets,
                                                            size_t** out_sizes,
                                                            size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_set_strings(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             char**** out_sets,
                                                             size_t** out_sizes,
                                                             size_t* out_count);

// Read scalar attributes by element ID
QUIVER_C_API quiver_error_t quiver_database_read_scalar_integers_by_id(quiver_database_t* db,
                                                                       const char* collection,
                                                                       const char* attribute,
                                                                       int64_t id,
                                                                       int64_t* out_value,
                                                                       int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_read_scalar_floats_by_id(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* attribute,
                                                                     int64_t id,
                                                                     double* out_value,
                                                                     int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_read_scalar_strings_by_id(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      int64_t id,
                                                                      char** out_value,
                                                                      int* out_has_value);

// Read vector attributes by element ID
QUIVER_C_API quiver_error_t quiver_database_read_vector_integers_by_id(quiver_database_t* db,
                                                                       const char* collection,
                                                                       const char* attribute,
                                                                       int64_t id,
                                                                       int64_t** out_values,
                                                                       size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_vector_floats_by_id(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* attribute,
                                                                     int64_t id,
                                                                     double** out_values,
                                                                     size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_vector_strings_by_id(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      int64_t id,
                                                                      char*** out_values,
                                                                      size_t* out_count);

// Read set attributes by element ID
QUIVER_C_API quiver_error_t quiver_database_read_set_integers_by_id(quiver_database_t* db,
                                                                    const char* collection,
                                                                    const char* attribute,
                                                                    int64_t id,
                                                                    int64_t** out_values,
                                                                    size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_set_floats_by_id(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  double** out_values,
                                                                  size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_read_set_strings_by_id(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   char*** out_values,
                                                                   size_t* out_count);

// Read element IDs
QUIVER_C_API quiver_error_t quiver_database_read_element_ids(quiver_database_t* db,
                                                             const char* collection,
                                                             int64_t** out_ids,
                                                             size_t* out_count);

// Attribute metadata types
typedef struct {
    const char* name;
    quiver_data_type_t data_type;
    int not_null;
    int primary_key;
    const char* default_value;  // NULL if no default
} quiver_scalar_metadata_t;

typedef struct {
    const char* group_name;
    quiver_scalar_metadata_t* value_columns;
    size_t value_column_count;
} quiver_vector_metadata_t;

typedef struct {
    const char* group_name;
    quiver_scalar_metadata_t* value_columns;
    size_t value_column_count;
} quiver_set_metadata_t;

// Attribute metadata queries
QUIVER_C_API quiver_error_t quiver_database_get_scalar_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                quiver_scalar_metadata_t* out_metadata);

QUIVER_C_API quiver_error_t quiver_database_get_vector_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group_name,
                                                                quiver_vector_metadata_t* out_metadata);

QUIVER_C_API quiver_error_t quiver_database_get_set_metadata(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* group_name,
                                                             quiver_set_metadata_t* out_metadata);

// Free metadata
QUIVER_C_API void quiver_free_scalar_metadata(quiver_scalar_metadata_t* metadata);
QUIVER_C_API void quiver_free_vector_metadata(quiver_vector_metadata_t* metadata);
QUIVER_C_API void quiver_free_set_metadata(quiver_set_metadata_t* metadata);

// List attributes/groups - returns full metadata
QUIVER_C_API quiver_error_t quiver_database_list_scalar_attributes(quiver_database_t* db,
                                                                   const char* collection,
                                                                   quiver_scalar_metadata_t** out_metadata,
                                                                   size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_list_vector_groups(quiver_database_t* db,
                                                               const char* collection,
                                                               quiver_vector_metadata_t** out_metadata,
                                                               size_t* out_count);

QUIVER_C_API quiver_error_t quiver_database_list_set_groups(quiver_database_t* db,
                                                            const char* collection,
                                                            quiver_set_metadata_t** out_metadata,
                                                            size_t* out_count);

// Free metadata arrays
QUIVER_C_API void quiver_free_scalar_metadata_array(quiver_scalar_metadata_t* metadata, size_t count);
QUIVER_C_API void quiver_free_vector_metadata_array(quiver_vector_metadata_t* metadata, size_t count);
QUIVER_C_API void quiver_free_set_metadata_array(quiver_set_metadata_t* metadata, size_t count);

// Update scalar attributes (by element ID)
QUIVER_C_API quiver_error_t quiver_database_update_scalar_integer(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  int64_t value);

QUIVER_C_API quiver_error_t quiver_database_update_scalar_float(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                double value);

QUIVER_C_API quiver_error_t quiver_database_update_scalar_string(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const char* value);

// Update vector attributes (by element ID) - replaces entire vector
QUIVER_C_API quiver_error_t quiver_database_update_vector_integers(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   const int64_t* values,
                                                                   size_t count);

QUIVER_C_API quiver_error_t quiver_database_update_vector_floats(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const double* values,
                                                                 size_t count);

QUIVER_C_API quiver_error_t quiver_database_update_vector_strings(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  const char* const* values,
                                                                  size_t count);

// Update set attributes (by element ID) - replaces entire set
QUIVER_C_API quiver_error_t quiver_database_update_set_integers(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                const int64_t* values,
                                                                size_t count);

QUIVER_C_API quiver_error_t quiver_database_update_set_floats(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              const double* values,
                                                              size_t count);

QUIVER_C_API quiver_error_t quiver_database_update_set_strings(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               int64_t id,
                                                               const char* const* values,
                                                               size_t count);

// Memory cleanup for read results
QUIVER_C_API void quiver_free_integer_array(int64_t* values);
QUIVER_C_API void quiver_free_float_array(double* values);
QUIVER_C_API void quiver_free_string_array(char** values, size_t count);

// Memory cleanup for vector read results
QUIVER_C_API void quiver_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count);
QUIVER_C_API void quiver_free_float_vectors(double** vectors, size_t* sizes, size_t count);
QUIVER_C_API void quiver_free_string_vectors(char*** vectors, size_t* sizes, size_t count);

// CSV operations
QUIVER_C_API quiver_error_t quiver_database_export_to_csv(quiver_database_t* db, const char* table, const char* path);
QUIVER_C_API quiver_error_t quiver_database_import_from_csv(quiver_database_t* db, const char* table, const char* path);

// Query methods - execute SQL and return first row's first column
QUIVER_C_API quiver_error_t quiver_database_query_string(quiver_database_t* db,
                                                         const char* sql,
                                                         char** out_value,
                                                         int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_integer(quiver_database_t* db,
                                                          const char* sql,
                                                          int64_t* out_value,
                                                          int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_float(quiver_database_t* db,
                                                        const char* sql,
                                                        double* out_value,
                                                        int* out_has_value);

// Parameterized query methods
// param_types[i]: QUIVER_DATA_TYPE_INTEGER (0), QUIVER_DATA_TYPE_FLOAT (1),
//                 QUIVER_DATA_TYPE_STRING (2), QUIVER_DATA_TYPE_NULL (4)
// param_values[i]: pointer to int64_t, double, const char*, or NULL
QUIVER_C_API quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
                                                                 const char* sql,
                                                                 const int* param_types,
                                                                 const void* const* param_values,
                                                                 size_t param_count,
                                                                 char** out_value,
                                                                 int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_integer_params(quiver_database_t* db,
                                                                  const char* sql,
                                                                  const int* param_types,
                                                                  const void* const* param_values,
                                                                  size_t param_count,
                                                                  int64_t* out_value,
                                                                  int* out_has_value);

QUIVER_C_API quiver_error_t quiver_database_query_float_params(quiver_database_t* db,
                                                                const char* sql,
                                                                const int* param_types,
                                                                const void* const* param_values,
                                                                size_t param_count,
                                                                double* out_value,
                                                                int* out_has_value);

#ifdef __cplusplus
}
#endif

#endif  // QUIVER_C_DATABASE_H
