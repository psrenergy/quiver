from cffi import FFI

ffi = FFI()

# Phase 1 CFFI declarations: lifecycle subset from C API headers.
# Copied exactly from include/quiver/c/ headers with QUIVER_C_API stripped.
ffi.cdef("""
    // common.h
    typedef enum {
        QUIVER_OK = 0,
        QUIVER_ERROR = 1,
    } quiver_error_t;

    const char* quiver_version(void);
    const char* quiver_get_last_error(void);
    void quiver_clear_last_error(void);

    // options.h
    typedef enum {
        QUIVER_LOG_DEBUG = 0,
        QUIVER_LOG_INFO = 1,
        QUIVER_LOG_WARN = 2,
        QUIVER_LOG_ERROR = 3,
        QUIVER_LOG_OFF = 4,
    } quiver_log_level_t;

    typedef struct {
        int read_only;
        quiver_log_level_t console_level;
    } quiver_database_options_t;

    // database.h
    quiver_database_options_t quiver_database_options_default(void);

    typedef struct quiver_database quiver_database_t;

    quiver_error_t quiver_database_open(const char* path,
                                        const quiver_database_options_t* options,
                                        quiver_database_t** out_db);
    quiver_error_t quiver_database_from_migrations(const char* db_path,
                                                    const char* migrations_path,
                                                    const quiver_database_options_t* options,
                                                    quiver_database_t** out_db);
    quiver_error_t quiver_database_from_schema(const char* db_path,
                                                const char* schema_path,
                                                const quiver_database_options_t* options,
                                                quiver_database_t** out_db);
    quiver_error_t quiver_database_close(quiver_database_t* db);
    quiver_error_t quiver_database_is_healthy(quiver_database_t* db, int* out_healthy);
    quiver_error_t quiver_database_path(quiver_database_t* db, const char** out_path);
    quiver_error_t quiver_database_current_version(quiver_database_t* db, int64_t* out_version);
    quiver_error_t quiver_database_describe(quiver_database_t* db);

    // database.h - element operations
    typedef struct quiver_element quiver_element_t;
    quiver_error_t quiver_database_create_element(quiver_database_t* db,
                                                   const char* collection,
                                                   quiver_element_t* element,
                                                   int64_t* out_id);

    // element.h
    quiver_error_t quiver_element_create(quiver_element_t** out_element);
    quiver_error_t quiver_element_destroy(quiver_element_t* element);
    quiver_error_t quiver_element_clear(quiver_element_t* element);

    quiver_error_t quiver_element_set_integer(quiver_element_t* element, const char* name, int64_t value);
    quiver_error_t quiver_element_set_float(quiver_element_t* element, const char* name, double value);
    quiver_error_t quiver_element_set_string(quiver_element_t* element, const char* name, const char* value);
    quiver_error_t quiver_element_set_null(quiver_element_t* element, const char* name);

    quiver_error_t quiver_element_set_array_integer(quiver_element_t* element,
                                                     const char* name,
                                                     const int64_t* values,
                                                     int32_t count);
    quiver_error_t quiver_element_set_array_float(quiver_element_t* element,
                                                   const char* name,
                                                   const double* values,
                                                   int32_t count);
    quiver_error_t quiver_element_set_array_string(quiver_element_t* element,
                                                    const char* name,
                                                    const char* const* values,
                                                    int32_t count);

    quiver_error_t quiver_element_has_scalars(quiver_element_t* element, int* out_result);
    quiver_error_t quiver_element_has_arrays(quiver_element_t* element, int* out_result);
    quiver_error_t quiver_element_scalar_count(quiver_element_t* element, size_t* out_count);
    quiver_error_t quiver_element_array_count(quiver_element_t* element, size_t* out_count);

    quiver_error_t quiver_element_to_string(quiver_element_t* element, char** out_string);
    quiver_error_t quiver_element_free_string(char* str);

    // Data type enum
    typedef enum {
        QUIVER_DATA_TYPE_INTEGER = 0,
        QUIVER_DATA_TYPE_FLOAT = 1,
        QUIVER_DATA_TYPE_STRING = 2,
        QUIVER_DATA_TYPE_DATE_TIME = 3,
        QUIVER_DATA_TYPE_NULL = 4,
    } quiver_data_type_t;

    // Read scalar attributes
    quiver_error_t quiver_database_read_scalar_integers(quiver_database_t* db,
        const char* collection, const char* attribute,
        int64_t** out_values, size_t* out_count);
    quiver_error_t quiver_database_read_scalar_floats(quiver_database_t* db,
        const char* collection, const char* attribute,
        double** out_values, size_t* out_count);
    quiver_error_t quiver_database_read_scalar_strings(quiver_database_t* db,
        const char* collection, const char* attribute,
        char*** out_values, size_t* out_count);

    // Read scalar by ID
    quiver_error_t quiver_database_read_scalar_integer_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_read_scalar_float_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        double* out_value, int* out_has_value);
    quiver_error_t quiver_database_read_scalar_string_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        char** out_value, int* out_has_value);

    // Read vector attributes (bulk)
    quiver_error_t quiver_database_read_vector_integers(quiver_database_t* db,
        const char* collection, const char* attribute,
        int64_t*** out_vectors, size_t** out_sizes, size_t* out_count);
    quiver_error_t quiver_database_read_vector_floats(quiver_database_t* db,
        const char* collection, const char* attribute,
        double*** out_vectors, size_t** out_sizes, size_t* out_count);
    quiver_error_t quiver_database_read_vector_strings(quiver_database_t* db,
        const char* collection, const char* attribute,
        char**** out_vectors, size_t** out_sizes, size_t* out_count);

    // Read vector by ID
    quiver_error_t quiver_database_read_vector_integers_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        int64_t** out_values, size_t* out_count);
    quiver_error_t quiver_database_read_vector_floats_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        double** out_values, size_t* out_count);
    quiver_error_t quiver_database_read_vector_strings_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        char*** out_values, size_t* out_count);

    // Read set attributes (bulk)
    quiver_error_t quiver_database_read_set_integers(quiver_database_t* db,
        const char* collection, const char* attribute,
        int64_t*** out_sets, size_t** out_sizes, size_t* out_count);
    quiver_error_t quiver_database_read_set_floats(quiver_database_t* db,
        const char* collection, const char* attribute,
        double*** out_sets, size_t** out_sizes, size_t* out_count);
    quiver_error_t quiver_database_read_set_strings(quiver_database_t* db,
        const char* collection, const char* attribute,
        char**** out_sets, size_t** out_sizes, size_t* out_count);

    // Read set by ID
    quiver_error_t quiver_database_read_set_integers_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        int64_t** out_values, size_t* out_count);
    quiver_error_t quiver_database_read_set_floats_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        double** out_values, size_t* out_count);
    quiver_error_t quiver_database_read_set_strings_by_id(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        char*** out_values, size_t* out_count);

    // Read element IDs
    quiver_error_t quiver_database_read_element_ids(quiver_database_t* db,
        const char* collection, int64_t** out_ids, size_t* out_count);

    // Read scalar relation
    quiver_error_t quiver_database_read_scalar_relation(quiver_database_t* db,
        const char* collection, const char* attribute,
        char*** out_values, size_t* out_count);

    // Update scalar relation (by label)
    quiver_error_t quiver_database_update_scalar_relation(quiver_database_t* db,
        const char* collection, const char* attribute,
        const char* from_label, const char* to_label);

    // Free functions for read results
    quiver_error_t quiver_database_free_integer_array(int64_t* values);
    quiver_error_t quiver_database_free_float_array(double* values);
    quiver_error_t quiver_database_free_string_array(char** values, size_t count);
    quiver_error_t quiver_database_free_integer_vectors(int64_t** vectors, size_t* sizes, size_t count);
    quiver_error_t quiver_database_free_float_vectors(double** vectors, size_t* sizes, size_t count);
    quiver_error_t quiver_database_free_string_vectors(char*** vectors, size_t* sizes, size_t count);

    // Metadata struct types
    typedef struct {
        const char* name;
        quiver_data_type_t data_type;
        int not_null;
        int primary_key;
        const char* default_value;
        int is_foreign_key;
        const char* references_collection;
        const char* references_column;
    } quiver_scalar_metadata_t;

    typedef struct {
        const char* group_name;
        const char* dimension_column;
        quiver_scalar_metadata_t* value_columns;
        size_t value_column_count;
    } quiver_group_metadata_t;

    // Metadata queries
    quiver_error_t quiver_database_get_scalar_metadata(quiver_database_t* db,
        const char* collection, const char* attribute,
        quiver_scalar_metadata_t* out_metadata);
    quiver_error_t quiver_database_get_vector_metadata(quiver_database_t* db,
        const char* collection, const char* group_name,
        quiver_group_metadata_t* out_metadata);
    quiver_error_t quiver_database_get_set_metadata(quiver_database_t* db,
        const char* collection, const char* group_name,
        quiver_group_metadata_t* out_metadata);
    quiver_error_t quiver_database_get_time_series_metadata(quiver_database_t* db,
        const char* collection, const char* group_name,
        quiver_group_metadata_t* out_metadata);

    // Metadata free
    quiver_error_t quiver_database_free_scalar_metadata(quiver_scalar_metadata_t* metadata);
    quiver_error_t quiver_database_free_group_metadata(quiver_group_metadata_t* metadata);

    // Metadata list
    quiver_error_t quiver_database_list_scalar_attributes(quiver_database_t* db,
        const char* collection,
        quiver_scalar_metadata_t** out_metadata, size_t* out_count);
    quiver_error_t quiver_database_list_vector_groups(quiver_database_t* db,
        const char* collection,
        quiver_group_metadata_t** out_metadata, size_t* out_count);
    quiver_error_t quiver_database_list_set_groups(quiver_database_t* db,
        const char* collection,
        quiver_group_metadata_t** out_metadata, size_t* out_count);
    quiver_error_t quiver_database_list_time_series_groups(quiver_database_t* db,
        const char* collection,
        quiver_group_metadata_t** out_metadata, size_t* out_count);

    // Metadata array free
    quiver_error_t quiver_database_free_scalar_metadata_array(
        quiver_scalar_metadata_t* metadata, size_t count);
    quiver_error_t quiver_database_free_group_metadata_array(
        quiver_group_metadata_t* metadata, size_t count);

    // Update element
    quiver_error_t quiver_database_update_element(quiver_database_t* db,
        const char* collection, int64_t id, const quiver_element_t* element);
    quiver_error_t quiver_database_delete_element(quiver_database_t* db,
        const char* collection, int64_t id);

    // Update scalar attributes
    quiver_error_t quiver_database_update_scalar_integer(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id, int64_t value);
    quiver_error_t quiver_database_update_scalar_float(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id, double value);
    quiver_error_t quiver_database_update_scalar_string(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id, const char* value);

    // Update vector attributes
    quiver_error_t quiver_database_update_vector_integers(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        const int64_t* values, size_t count);
    quiver_error_t quiver_database_update_vector_floats(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        const double* values, size_t count);
    quiver_error_t quiver_database_update_vector_strings(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        const char* const* values, size_t count);

    // Update set attributes
    quiver_error_t quiver_database_update_set_integers(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        const int64_t* values, size_t count);
    quiver_error_t quiver_database_update_set_floats(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        const double* values, size_t count);
    quiver_error_t quiver_database_update_set_strings(quiver_database_t* db,
        const char* collection, const char* attribute, int64_t id,
        const char* const* values, size_t count);

    // Transaction control
    quiver_error_t quiver_database_begin_transaction(quiver_database_t* db);
    quiver_error_t quiver_database_commit(quiver_database_t* db);
    quiver_error_t quiver_database_rollback(quiver_database_t* db);
    quiver_error_t quiver_database_in_transaction(quiver_database_t* db, _Bool* out_active);

    // Query methods - simple
    quiver_error_t quiver_database_query_string(quiver_database_t* db,
        const char* sql, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer(quiver_database_t* db,
        const char* sql, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float(quiver_database_t* db,
        const char* sql, double* out_value, int* out_has_value);

    // Query methods - parameterized
    quiver_error_t quiver_database_query_string_params(quiver_database_t* db,
        const char* sql, const int* param_types, void**  param_values,
        size_t param_count, char** out_value, int* out_has_value);
    quiver_error_t quiver_database_query_integer_params(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, int64_t* out_value, int* out_has_value);
    quiver_error_t quiver_database_query_float_params(quiver_database_t* db,
        const char* sql, const int* param_types, void** param_values,
        size_t param_count, double* out_value, int* out_has_value);

    // Time series group read/update/free
    quiver_error_t quiver_database_read_time_series_group(quiver_database_t* db,
        const char* collection, const char* group, int64_t id,
        char*** out_column_names, int** out_column_types,
        void*** out_column_data, size_t* out_column_count, size_t* out_row_count);

    quiver_error_t quiver_database_update_time_series_group(quiver_database_t* db,
        const char* collection, const char* group, int64_t id,
        const char* const* column_names, const int* column_types,
        void** column_data, size_t column_count, size_t row_count);

    quiver_error_t quiver_database_free_time_series_data(char** column_names,
        int* column_types, void** column_data,
        size_t column_count, size_t row_count);

    // Time series files
    quiver_error_t quiver_database_has_time_series_files(quiver_database_t* db,
        const char* collection, int* out_result);

    quiver_error_t quiver_database_list_time_series_files_columns(quiver_database_t* db,
        const char* collection, char*** out_columns, size_t* out_count);

    quiver_error_t quiver_database_read_time_series_files(quiver_database_t* db,
        const char* collection, char*** out_columns, char*** out_paths, size_t* out_count);

    quiver_error_t quiver_database_update_time_series_files(quiver_database_t* db,
        const char* collection, const char* const* columns, const char* const* paths,
        size_t count);

    quiver_error_t quiver_database_free_time_series_files(char** columns, char** paths, size_t count);
""")

_lib = None


def get_lib():
    """Lazily load and return the C API library handle."""
    global _lib
    if _lib is None:
        from quiver._loader import load_library

        _lib = load_library(ffi)
    return _lib
