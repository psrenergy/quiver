#ifndef PSR_C_API_INTERNAL_H
#define PSR_C_API_INTERNAL_H

#include "psr/database.h"
#include "psr/element.h"

#include <string>

// Internal structs shared between C API implementation files

struct margaux {
    psr::Database db;
    margaux(const std::string& path, const psr::DatabaseOptions& options) : db(path, options) {}
    margaux(psr::Database&& database) : db(std::move(database)) {}
};

struct psr_element {
    psr::Element element;
};

#endif  // PSR_C_API_INTERNAL_H
