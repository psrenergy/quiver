#include "psr/c/database.h"
#include "psr/database.h"

#include <new>
#include <string>

namespace {

psr::LogLevel to_cpp_log_level(psr_log_level_t level) {
    switch (level) {
    case PSR_LOG_DEBUG:
        return psr::LogLevel::debug;
    case PSR_LOG_INFO:
        return psr::LogLevel::info;
    case PSR_LOG_WARN:
        return psr::LogLevel::warn;
    case PSR_LOG_ERROR:
        return psr::LogLevel::error;
    case PSR_LOG_OFF:
        return psr::LogLevel::off;
    default:
        return psr::LogLevel::info;
    }
}

}  // anonymous namespace

// Internal struct definition
struct psr_database {
    psr::Database db;
    psr_database(const std::string& path, psr::LogLevel level) : db(path, level) {}
};

extern "C" {

PSR_C_API psr_database_t* psr_database_open(const char* path, psr_log_level_t console_level, psr_error_t* error) {
    if (!path) {
        if (error)
            *error = PSR_ERROR_INVALID_ARGUMENT;
        return nullptr;
    }

    try {
        auto* db = new psr_database(path, to_cpp_log_level(console_level));
        if (error)
            *error = PSR_OK;
        return db;
    } catch (const std::bad_alloc&) {
        if (error)
            *error = PSR_ERROR_DATABASE;
        return nullptr;
    } catch (const std::exception&) {
        if (error)
            *error = PSR_ERROR_DATABASE;
        return nullptr;
    }
}

PSR_C_API void psr_database_close(psr_database_t* db) {
    if (db) {
        db->db.close();
        delete db;
    }
}

PSR_C_API int psr_database_is_open(psr_database_t* db) {
    if (!db)
        return 0;
    return db->db.is_open() ? 1 : 0;
}

PSR_C_API const char* psr_database_path(psr_database_t* db) {
    if (!db)
        return nullptr;
    return db->db.path().c_str();
}

PSR_C_API const char* psr_error_string(psr_error_t error) {
    switch (error) {
    case PSR_OK:
        return "Success";
    case PSR_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case PSR_ERROR_DATABASE:
        return "Database error";
    default:
        return "Unknown error";
    }
}

PSR_C_API const char* psr_version(void) {
    return PSR_VERSION;
}

}  // extern "C"
