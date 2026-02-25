#include "internal.h"
#include "quiver/c/database.h"

#include <string>
#include <vector>

extern "C" {

// Update vector functions

QUIVER_C_API quiver_error_t quiver_database_update_vector_integers(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   int64_t id,
                                                                   const int64_t* values,
                                                                   size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_vector_integers(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_floats(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 int64_t id,
                                                                 const double* values,
                                                                 size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<double> vec(values, values + count);
        db->db.update_vector_floats(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_strings(quiver_database_t* db,
                                                                  const char* collection,
                                                                  const char* attribute,
                                                                  int64_t id,
                                                                  const char* const* values,
                                                                  size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            if (!values[i]) {
                quiver_set_last_error("Null string pointer in values");
                return QUIVER_ERROR;
            }
            vec.emplace_back(values[i]);
        }
        db->db.update_vector_strings(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Update set functions

QUIVER_C_API quiver_error_t quiver_database_update_set_integers(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                int64_t id,
                                                                const int64_t* values,
                                                                size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<int64_t> vec(values, values + count);
        db->db.update_set_integers(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_floats(quiver_database_t* db,
                                                              const char* collection,
                                                              const char* attribute,
                                                              int64_t id,
                                                              const double* values,
                                                              size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<double> vec(values, values + count);
        db->db.update_set_floats(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_strings(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               int64_t id,
                                                               const char* const* values,
                                                               size_t count) {
    QUIVER_REQUIRE(db, collection, attribute);

    if (count > 0 && !values) {
        quiver_set_last_error("Null values with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::vector<std::string> vec;
        vec.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            if (!values[i]) {
                quiver_set_last_error("Null string pointer in values");
                return QUIVER_ERROR;
            }
            vec.emplace_back(values[i]);
        }
        db->db.update_set_strings(collection, attribute, id, vec);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
