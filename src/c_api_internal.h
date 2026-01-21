#ifndef MARGAUX_C_API_INTERNAL_H
#define MARGAUX_C_API_INTERNAL_H

#include "margaux/database.h"
#include "margaux/element.h"

#include <string>

// Internal structs shared between C API implementation files

struct database {
    margaux::Database db;
    database(const std::string& path, const margaux::DatabaseOptions& options) : db(path, options) {}
    database(margaux::Database&& database) : db(std::move(database)) {}
};

struct element {
    margaux::Element element;
};

#endif  // MARGAUX_C_API_INTERNAL_H
