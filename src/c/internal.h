#ifndef QUIVER_C_API_INTERNAL_H
#define QUIVER_C_API_INTERNAL_H

#include "quiver/database.h"
#include "quiver/element.h"

#include <string>

// Thread-local error message storage
void quiver_set_last_error(const std::string& message);
void quiver_set_last_error(const char* message);

// Internal structs shared between C API implementation files

struct quiver_database {
    quiver::Database db;
    quiver_database(const std::string& path, const quiver::DatabaseOptions& options) : db(path, options) {}
    quiver_database(quiver::Database&& database) : db(std::move(database)) {}
};

struct quiver_element {
    quiver::Element element;
};

// Validates a pointer argument is non-null. Sets descriptive error and returns QUIVER_ERROR_INVALID_ARGUMENT.
// Uses stringification to auto-generate messages like "Null argument: db", "Null argument: collection".
#define QUIVER_REQUIRE(ptr) \
    do { \
        if (!(ptr)) { \
            quiver_set_last_error("Null argument: " #ptr); \
            return QUIVER_ERROR_INVALID_ARGUMENT; \
        } \
    } while (0)

#endif  // QUIVER_C_API_INTERNAL_H
