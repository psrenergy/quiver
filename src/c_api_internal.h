#ifndef PSR_C_API_INTERNAL_H
#define PSR_C_API_INTERNAL_H

#include "psr/database.h"
#include "psr/element.h"

#include <string>

// Internal structs shared between C API implementation files

struct psr_database {
    margaux::Database db;
    psr_database(const std::string& path, const margaux::DatabaseOptions& options) : db(path, options) {}
    psr_database(margaux::Database&& database) : db(std::move(database)) {}
};

struct psr_element {
    margaux::Element element;
};

#endif  // PSR_C_API_INTERNAL_H
