#include "quiver/c/common.h"

extern "C" {

QUIVER_C_API const char* quiver_error_string(quiver_error_t error) {
    switch (error) {
    case QUIVER_OK:
        return "Success";
    case QUIVER_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case QUIVER_ERROR_DATABASE:
        return "Database error";
    case QUIVER_ERROR_MIGRATION:
        return "Migration error";
    case QUIVER_ERROR_SCHEMA:
        return "Schema validation error";
    case QUIVER_ERROR_CREATE_ELEMENT:
        return "Failed to create element";
    case QUIVER_ERROR_NOT_FOUND:
        return "Not found";
    default:
        return "Unknown error";
    }
}

QUIVER_C_API const char* quiver_version(void) {
    return QUIVER_VERSION;
}

}  // extern "C"
