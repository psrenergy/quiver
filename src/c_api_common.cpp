#include "margaux/c/common.h"

extern "C" {

MARGAUX_C_API const char* psr_error_string(psr_error_t error) {
    switch (error) {
    case PSR_OK:
        return "Success";
    case PSR_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case PSR_ERROR_DATABASE:
        return "Database error";
    case PSR_ERROR_MIGRATION:
        return "Migration error";
    case PSR_ERROR_SCHEMA:
        return "Schema validation error";
    case PSR_ERROR_CREATE_ELEMENT:
        return "Failed to create element";
    case PSR_ERROR_NOT_FOUND:
        return "Not found";
    default:
        return "Unknown error";
    }
}

MARGAUX_C_API const char* psr_version(void) {
    return PSR_VERSION;
}

}  // extern "C"
