#include "margaux/c/common.h"

extern "C" {

DECK_DATABASE_C_API const char* margaux_error_string(margaux_error_t error) {
    switch (error) {
    case DECK_DATABASE_OK:
        return "Success";
    case DECK_DATABASE_ERROR_INVALID_ARGUMENT:
        return "Invalid argument";
    case DECK_DATABASE_ERROR_DATABASE:
        return "Database error";
    case DECK_DATABASE_ERROR_MIGRATION:
        return "Migration error";
    case DECK_DATABASE_ERROR_SCHEMA:
        return "Schema validation error";
    case DECK_DATABASE_ERROR_CREATE_ELEMENT:
        return "Failed to create element";
    case DECK_DATABASE_ERROR_NOT_FOUND:
        return "Not found";
    default:
        return "Unknown error";
    }
}

DECK_DATABASE_C_API const char* margaux_version(void) {
    return DECK_DATABASE_VERSION;
}

}  // extern "C"
