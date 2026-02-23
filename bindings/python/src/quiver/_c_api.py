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
""")

_lib = None


def get_lib():
    """Lazily load and return the C API library handle."""
    global _lib
    if _lib is None:
        from quiver._loader import load_library

        _lib = load_library(ffi)
    return _lib
