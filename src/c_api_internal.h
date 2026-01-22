#ifndef QUIVER_C_API_INTERNAL_H
#define QUIVER_C_API_INTERNAL_H

#include "quiver/database.h"
#include "quiver/element.h"

#include <string>

// Internal structs shared between C API implementation files

struct quiver_database {
    quiver::Database db;
    quiver_database(const std::string& path, const quiver::DatabaseOptions& options) : db(path, options) {}
    quiver_database(quiver::Database&& database) : db(std::move(database)) {}
};

struct quiver_element {
    quiver::Element element;
};

#endif  // QUIVER_C_API_INTERNAL_H
