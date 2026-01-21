#ifndef MARGAUX_C_API_INTERNAL_H
#define MARGAUX_C_API_INTERNAL_H

#include "margaux/database.h"
#include "margaux/element.h"

#include <string>

// Internal structs shared between C API implementation files

struct margaux {
    margaux::Database db;
    margaux(const std::string& path, const margaux::DatabaseOptions& options) : db(path, options) {}
    margaux(margaux::Database&& database) : db(std::move(database)) {}
};

struct margaux_element {
    margaux::Element element;
};

#endif  // MARGAUX_C_API_INTERNAL_H
