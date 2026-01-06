#include "psr/c/common.h"

extern "C" {

PSR_C_API const char* psr_error_string(psr_error_t error) {
    switch (error) {
    case PSR_OK:
        return "Success";
    case PSR_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case PSR_ERROR_DATABASE:
        return "Database error";
    case PSR_ERROR_MIGRATION:
        return "Migration error";
    default:
        return "Unknown error";
    }
}

PSR_C_API const char* psr_version(void) {
    return PSR_VERSION;
}

}  // extern "C"
