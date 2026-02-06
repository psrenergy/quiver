#include "quiver/c/common.h"

#include "internal.h"

#include <string>

// Thread-local storage for error messages
static thread_local std::string g_last_error;

void quiver_set_last_error(const std::string& message) {
    g_last_error = message;
}

void quiver_set_last_error(const char* message) {
    g_last_error = message ? message : "";
}

extern "C" {

QUIVER_C_API const char* quiver_get_last_error(void) {
    return g_last_error.c_str();
}

QUIVER_C_API void quiver_clear_last_error(void) {
    g_last_error.clear();
}

QUIVER_C_API const char* quiver_version(void) {
    return QUIVER_VERSION;
}

}  // extern "C"
