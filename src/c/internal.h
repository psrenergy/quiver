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

// Validates pointer arguments are non-null. Sets descriptive error and returns QUIVER_ERROR_INVALID_ARGUMENT.
// Uses stringification to auto-generate messages like "Null argument: db", "Null argument: collection".
// Supports 1-6 arguments: QUIVER_REQUIRE(db, collection, attribute)
#define QUIVER_REQUIRE_1(a)                                                                                            \
    do {                                                                                                               \
        if (!(a)) {                                                                                                    \
            quiver_set_last_error("Null argument: " #a);                                                               \
            return QUIVER_ERROR_INVALID_ARGUMENT;                                                                      \
        }                                                                                                              \
    } while (0)
#define QUIVER_REQUIRE_2(a, b)                                                                                         \
    do {                                                                                                               \
        QUIVER_REQUIRE_1(a);                                                                                           \
        QUIVER_REQUIRE_1(b);                                                                                           \
    } while (0)
#define QUIVER_REQUIRE_3(a, b, c)                                                                                      \
    do {                                                                                                               \
        QUIVER_REQUIRE_1(a);                                                                                           \
        QUIVER_REQUIRE_1(b);                                                                                           \
        QUIVER_REQUIRE_1(c);                                                                                           \
    } while (0)
#define QUIVER_REQUIRE_4(a, b, c, d)                                                                                   \
    do {                                                                                                               \
        QUIVER_REQUIRE_1(a);                                                                                           \
        QUIVER_REQUIRE_1(b);                                                                                           \
        QUIVER_REQUIRE_1(c);                                                                                           \
        QUIVER_REQUIRE_1(d);                                                                                           \
    } while (0)
#define QUIVER_REQUIRE_5(a, b, c, d, e)                                                                                \
    do {                                                                                                               \
        QUIVER_REQUIRE_1(a);                                                                                           \
        QUIVER_REQUIRE_1(b);                                                                                           \
        QUIVER_REQUIRE_1(c);                                                                                           \
        QUIVER_REQUIRE_1(d);                                                                                           \
        QUIVER_REQUIRE_1(e);                                                                                           \
    } while (0)
#define QUIVER_REQUIRE_6(a, b, c, d, e, f)                                                                             \
    do {                                                                                                               \
        QUIVER_REQUIRE_1(a);                                                                                           \
        QUIVER_REQUIRE_1(b);                                                                                           \
        QUIVER_REQUIRE_1(c);                                                                                           \
        QUIVER_REQUIRE_1(d);                                                                                           \
        QUIVER_REQUIRE_1(e);                                                                                           \
        QUIVER_REQUIRE_1(f);                                                                                           \
    } while (0)

#define QUIVER_EXPAND(x) x
#define QUIVER_REQUIRE_N(_1, _2, _3, _4, _5, _6, N, ...) N
#define QUIVER_REQUIRE(...)                                                                                            \
    QUIVER_EXPAND(QUIVER_REQUIRE_N(__VA_ARGS__,                                                                        \
                                   QUIVER_REQUIRE_6,                                                                   \
                                   QUIVER_REQUIRE_5,                                                                   \
                                   QUIVER_REQUIRE_4,                                                                   \
                                   QUIVER_REQUIRE_3,                                                                   \
                                   QUIVER_REQUIRE_2,                                                                   \
                                   QUIVER_REQUIRE_1)(__VA_ARGS__))

#endif  // QUIVER_C_API_INTERNAL_H
