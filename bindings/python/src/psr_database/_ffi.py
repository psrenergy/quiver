"""CFFI bindings for PSR Database C API."""

import os
import sys
from pathlib import Path

from cffi import FFI

ffi = FFI()

ffi.cdef("""
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

    // Log levels
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

    psr_database_options_t psr_database_options_default(void);

    // Opaque handles
    typedef struct psr_database psr_database_t;

    // Utility
    const char* psr_error_string(psr_error_t error);
    const char* psr_version(void);

    // Database lifecycle
    psr_database_t* psr_database_open(const char* path, const psr_database_options_t* options);
    psr_database_t* psr_database_from_schema(const char* db_path, const char* schema_path, const psr_database_options_t* options);
    psr_database_t* psr_database_from_migrations(const char* db_path, const char* migrations_path, const psr_database_options_t* options);
    void psr_database_close(psr_database_t* db);
    int psr_database_is_healthy(psr_database_t* db);
    const char* psr_database_path(psr_database_t* db);
    int64_t psr_database_current_version(psr_database_t* db);
""")


def _find_library() -> str:
    """Find the PSR Database C library."""
    if sys.platform == "win32":
        lib_name = "libpsr_database_c.dll"
    elif sys.platform == "darwin":
        lib_name = "libpsr_database_c.dylib"
    else:
        lib_name = "libpsr_database_c.so"

    # 1. Environment variable
    if "PSR_DATABASE_LIB_PATH" in os.environ:
        path = Path(os.environ["PSR_DATABASE_LIB_PATH"]) / lib_name
        if path.exists():
            return str(path)

    # 2. Development path (bindings/python/src/psr_database -> build/bin)
    project_root = Path(__file__).parent.parent.parent.parent.parent
    path = project_root / "build" / "bin" / lib_name
    if path.exists():
        return str(path)

    # 3. System PATH
    return lib_name


lib = ffi.dlopen(_find_library())
