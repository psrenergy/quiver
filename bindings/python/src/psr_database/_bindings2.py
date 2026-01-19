"""CFFI declarations - GENERATED FILE, DO NOT EDIT.

Regenerate with: generator/generator.bat
"""

CDEF = """
// From common.h

typedef enum {
    PSR_OK = 0,
    PSR_ERROR_INVALID_ARGUMENT = -1,
    PSR_ERROR_DATABASE = -2,
    PSR_ERROR_MIGRATION = -3,
    PSR_ERROR_SCHEMA = -4,
    PSR_ERROR_CREATE_ELEMENT = -5,
    PSR_ERROR_NOT_FOUND = -6,
} psr_error_t;

const char* psr_error_string(psr_error_t error);
const char* psr_version(void);

// From database.h

typedef enum {
    PSR_LOG_DEBUG = 0,
    PSR_LOG_INFO = 1,
    PSR_LOG_WARN = 2,
    PSR_LOG_ERROR = 3,
    PSR_LOG_OFF = 4,
} psr_log_level_t;

typedef struct {
    int read_only;
    psr_log_level_t console_level;
} psr_database_options_t;

typedef enum {
    PSR_DATA_STRUCTURE_SCALAR = 0,
    PSR_DATA_STRUCTURE_VECTOR = 1,
    PSR_DATA_STRUCTURE_SET = 2
} psr_data_structure_t;

typedef enum { PSR_DATA_TYPE_INTEGER = 0, PSR_DATA_TYPE_FLOAT = 1, PSR_DATA_TYPE_STRING = 2 } psr_data_type_t;

psr_database_options_t psr_database_options_default(void);

typedef struct psr_database psr_database_t;

psr_database_t* psr_database_open(const char* path, const psr_database_options_t* options);
psr_database_t*
psr_database_from_migrations(const char* db_path, const char* migrations_path, const psr_database_options_t* options);
psr_database_t*
psr_database_from_schema(const char* db_path, const char* schema_path, const psr_database_options_t* options);
void psr_database_close(psr_database_t* db);
int psr_database_is_healthy(psr_database_t* db);
const char* psr_database_path(psr_database_t* db);

int64_t psr_database_current_version(psr_database_t* db);

typedef struct psr_element psr_element_t;
int64_t psr_database_create_element(psr_database_t* db, const char* collection, psr_element_t* element);
psr_error_t psr_database_update_element(psr_database_t* db,
                                                  const char* collection,
                                                  int64_t id,
                                                  const psr_element_t* element);
psr_error_t psr_database_delete_element_by_id(psr_database_t* db, const char* collection, int64_t id);

psr_error_t psr_database_set_scalar_relation(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       const char* from_label,
                                                       const char* to_label);

psr_error_t psr_database_read_scalar_relation(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        char*** out_values,
                                                        size_t* out_count);

psr_error_t psr_database_read_scalar_integers(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t** out_values,
                                                        size_t* out_count);

psr_error_t psr_database_read_scalar_floats(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      double** out_values,
                                                      size_t* out_count);

psr_error_t psr_database_read_scalar_strings(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       char*** out_values,
                                                       size_t* out_count);

psr_error_t psr_database_read_vector_integers(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t*** out_vectors,
                                                        size_t** out_sizes,
                                                        size_t* out_count);

psr_error_t psr_database_read_vector_floats(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      double*** out_vectors,
                                                      size_t** out_sizes,
                                                      size_t* out_count);

psr_error_t psr_database_read_vector_strings(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       char**** out_vectors,
                                                       size_t** out_sizes,
                                                       size_t* out_count);

psr_error_t psr_database_read_set_integers(psr_database_t* db,
                                                     const char* collection,
                                                     const char* attribute,
                                                     int64_t*** out_sets,
                                                     size_t** out_sizes,
                                                     size_t* out_count);

psr_error_t psr_database_read_set_floats(psr_database_t* db,
                                                   const char* collection,
                                                   const char* attribute,
                                                   double*** out_sets,
                                                   size_t** out_sizes,
                                                   size_t* out_count);

psr_error_t psr_database_read_set_strings(psr_database_t* db,
                                                    const char* collection,
                                                    const char* attribute,
                                                    char**** out_sets,
                                                    size_t** out_sizes,
                                                    size_t* out_count);

psr_error_t psr_database_read_scalar_integers_by_id(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              int64_t* out_value,
                                                              int* out_has_value);

psr_error_t psr_database_read_scalar_floats_by_id(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t id,
                                                            double* out_value,
                                                            int* out_has_value);

psr_error_t psr_database_read_scalar_strings_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             char** out_value,
                                                             int* out_has_value);

psr_error_t psr_database_read_vector_integers_by_id(psr_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              int64_t** out_values,
                                                              size_t* out_count);

psr_error_t psr_database_read_vector_floats_by_id(psr_database_t* db,
                                                            const char* collection,
                                                            const char* attribute,
                                                            int64_t id,
                                                            double** out_values,
                                                            size_t* out_count);

psr_error_t psr_database_read_vector_strings_by_id(psr_database_t* db,
                                                             const char* collection,
                                                             const char* attribute,
                                                             int64_t id,
                                                             char*** out_values,
                                                             size_t* out_count);

psr_error_t psr_database_read_set_integers_by_id(psr_database_t* db,
                                                           const char* collection,
                                                           const char* attribute,
                                                           int64_t id,
                                                           int64_t** out_values,
                                                           size_t* out_count);

psr_error_t psr_database_read_set_floats_by_id(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         double** out_values,
                                                         size_t* out_count);

psr_error_t psr_database_read_set_strings_by_id(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          char*** out_values,
                                                          size_t* out_count);

psr_error_t psr_database_read_element_ids(psr_database_t* db,
                                                    const char* collection,
                                                    int64_t** out_ids,
                                                    size_t* out_count);

psr_error_t psr_database_get_attribute_type(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      psr_data_structure_t* out_data_structure,
                                                      psr_data_type_t* out_data_type);

psr_error_t psr_database_update_scalar_integer(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         int64_t value);

psr_error_t psr_database_update_scalar_float(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       int64_t id,
                                                       double value);

psr_error_t psr_database_update_scalar_string(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t id,
                                                        const char* value);

psr_error_t psr_database_update_vector_integers(psr_database_t* db,
                                                          const char* collection,
                                                          const char* attribute,
                                                          int64_t id,
                                                          const int64_t* values,
                                                          size_t count);

psr_error_t psr_database_update_vector_floats(psr_database_t* db,
                                                        const char* collection,
                                                        const char* attribute,
                                                        int64_t id,
                                                        const double* values,
                                                        size_t count);

psr_error_t psr_database_update_vector_strings(psr_database_t* db,
                                                         const char* collection,
                                                         const char* attribute,
                                                         int64_t id,
                                                         const char* const* values,
                                                         size_t count);

psr_error_t psr_database_update_set_integers(psr_database_t* db,
                                                       const char* collection,
                                                       const char* attribute,
                                                       int64_t id,
                                                       const int64_t* values,
                                                       size_t count);

psr_error_t psr_database_update_set_floats(psr_database_t* db,
                                                     const char* collection,
                                                     const char* attribute,
                                                     int64_t id,
                                                     const double* values,
                                                     size_t count);

psr_error_t psr_database_update_set_strings(psr_database_t* db,
                                                      const char* collection,
                                                      const char* attribute,
                                                      int64_t id,
                                                      const char* const* values,
                                                      size_t count);

void psr_free_integer_array(int64_t* values);
void psr_free_float_array(double* values);
void psr_free_string_array(char** values, size_t count);

void psr_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count);
void psr_free_float_vectors(double** vectors, size_t* sizes, size_t count);
void psr_free_string_vectors(char*** vectors, size_t* sizes, size_t count);

// From element.h

typedef struct psr_element psr_element_t;

psr_element_t* psr_element_create(void);
void psr_element_destroy(psr_element_t* element);
void psr_element_clear(psr_element_t* element);

psr_error_t psr_element_set_integer(psr_element_t* element, const char* name, int64_t value);
psr_error_t psr_element_set_float(psr_element_t* element, const char* name, double value);
psr_error_t psr_element_set_string(psr_element_t* element, const char* name, const char* value);
psr_error_t psr_element_set_null(psr_element_t* element, const char* name);

psr_error_t psr_element_set_array_integer(psr_element_t* element,
                                                    const char* name,
                                                    const int64_t* values,
                                                    int32_t count);
psr_error_t psr_element_set_array_float(psr_element_t* element,
                                                  const char* name,
                                                  const double* values,
                                                  int32_t count);
psr_error_t psr_element_set_array_string(psr_element_t* element,
                                                   const char* name,
                                                   const char* const* values,
                                                   int32_t count);

int psr_element_has_scalars(psr_element_t* element);
int psr_element_has_arrays(psr_element_t* element);
size_t psr_element_scalar_count(psr_element_t* element);
size_t psr_element_array_count(psr_element_t* element);

char* psr_element_to_string(psr_element_t* element);
void psr_string_free(char* str);

// From lua_runner.h

typedef struct psr_lua_runner psr_lua_runner_t;

psr_lua_runner_t* psr_lua_runner_new(psr_database_t* db);

void psr_lua_runner_free(psr_lua_runner_t* runner);

psr_error_t psr_lua_runner_run(psr_lua_runner_t* runner, const char* script);

const char* psr_lua_runner_get_error(psr_lua_runner_t* runner);
"""
