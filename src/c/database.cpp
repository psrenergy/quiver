#include "quiver/c/database.h"

#include "database_helpers.h"
#include "database_options.h"
#include "internal.h"

#include <new>
#include <string>

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_open(const char* path,
                                                 const quiver_database_options_t* options,
                                                 quiver_database_t** out_db) {
    QUIVER_REQUIRE(path, out_db);

    try {
        if (options) {
            *out_db = new quiver_database(path, convert_database_options(*options));
        } else {
            *out_db = new quiver_database(path, quiver::DatabaseOptions{});
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_close(quiver_database_t* db) {
    delete db;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_is_healthy(quiver_database_t* db, int* out_healthy) {
    QUIVER_REQUIRE(db, out_healthy);

    *out_healthy = db->db.is_healthy() ? 1 : 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_path(quiver_database_t* db, const char** out_path) {
    QUIVER_REQUIRE(db, out_path);

    *out_path = db->db.path().c_str();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_from_migrations(const char* db_path,
                                                            const char* migrations_path,
                                                            const quiver_database_options_t* options,
                                                            quiver_database_t** out_db) {
    QUIVER_REQUIRE(db_path, migrations_path, out_db);

    try {
        if (options) {
            auto db = quiver::Database::from_migrations(db_path, migrations_path, convert_database_options(*options));
            *out_db = new quiver_database(std::move(db));
        } else {
            auto db = quiver::Database::from_migrations(db_path, migrations_path, quiver::DatabaseOptions{});
            *out_db = new quiver_database(std::move(db));
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_current_version(quiver_database_t* db, int64_t* out_version) {
    QUIVER_REQUIRE(db, out_version);

    try {
        *out_version = db->db.current_version();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_from_schema(const char* db_path,
                                                        const char* schema_path,
                                                        const quiver_database_options_t* options,
                                                        quiver_database_t** out_db) {
    QUIVER_REQUIRE(db_path, schema_path, out_db);

    try {
        if (options) {
            auto db = quiver::Database::from_schema(db_path, schema_path, convert_database_options(*options));
            *out_db = new quiver_database(std::move(db));
        } else {
            auto db = quiver::Database::from_schema(db_path, schema_path, quiver::DatabaseOptions{});
            *out_db = new quiver_database(std::move(db));
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_from_database(const char* db_path,
                                                          const char* dir,
                                                          const quiver_database_options_t* options,
                                                          quiver_database_t** out_db) {
    QUIVER_REQUIRE(db_path, dir, out_db);

    try {
        if (options) {
            auto db = quiver::Database::from_database(db_path, dir, convert_database_options(*options));
            *out_db = new quiver_database(std::move(db));
        } else {
            auto db = quiver::Database::from_database(db_path, dir, quiver::DatabaseOptions{});
            *out_db = new quiver_database(std::move(db));
        }
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Schema inspection — human-readable text reports.

QUIVER_C_API quiver_error_t quiver_database_describe(quiver_database_t* db, char** out_report) {
    QUIVER_REQUIRE(db, out_report);

    try {
        *out_report = quiver::string::new_c_str(db->db.describe());
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_describe_collection(quiver_database_t* db,
                                                                const char* collection,
                                                                char** out_report) {
    QUIVER_REQUIRE(db, collection, out_report);

    try {
        *out_report = quiver::string::new_c_str(db->db.describe_collection(collection));
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_summarize_collection(quiver_database_t* db,
                                                                 const char* collection,
                                                                 char** out_report) {
    QUIVER_REQUIRE(db, collection, out_report);

    try {
        *out_report = quiver::string::new_c_str(db->db.summarize_collection(collection));
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// UI metadata getters (English-first; empty string when unknown / no UI loaded).

QUIVER_C_API quiver_error_t quiver_database_get_model_name(quiver_database_t* db, char** out_name) {
    QUIVER_REQUIRE(db, out_name);

    try {
        *out_name = quiver::string::new_c_str(db->db.get_model_name());
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_attribute_unit(quiver_database_t* db,
                                                               const char* collection,
                                                               const char* attribute,
                                                               char** out_unit) {
    QUIVER_REQUIRE(db, collection, attribute, out_unit);

    try {
        *out_unit = quiver::string::new_c_str(db->db.get_attribute_unit(collection, attribute));
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
