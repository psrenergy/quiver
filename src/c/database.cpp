#include "quiver/c/database.h"

#include "internal.h"

#include <new>
#include <string>

extern "C" {

QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return quiver::default_database_options();
}

QUIVER_C_API quiver_error_t quiver_database_open(const char* path,
                                                 const quiver_database_options_t* options,
                                                 quiver_database_t** out_db) {
    QUIVER_REQUIRE(path, out_db);

    try {
        if (options) {
            *out_db = new quiver_database(path, *options);
        } else {
            auto default_options = quiver::default_database_options();
            *out_db = new quiver_database(path, default_options);
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
            auto db = quiver::Database::from_migrations(db_path, migrations_path, *options);
            *out_db = new quiver_database(std::move(db));
        } else {
            auto default_options = quiver::default_database_options();
            auto db = quiver::Database::from_migrations(db_path, migrations_path, default_options);
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
            auto db = quiver::Database::from_schema(db_path, schema_path, *options);
            *out_db = new quiver_database(std::move(db));
        } else {
            auto default_options = quiver::default_database_options();
            auto db = quiver::Database::from_schema(db_path, schema_path, default_options);
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

// Schema inspection

QUIVER_C_API quiver_error_t quiver_database_describe(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.describe();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// CSV operations

QUIVER_C_API quiver_error_t quiver_database_export_csv(quiver_database_t* db, const char* collection, const char* group, const char* path) {
    QUIVER_REQUIRE(db, collection, group, path);

    try {
        db->db.export_csv(collection, group, path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_import_csv(quiver_database_t* db, const char* table, const char* path) {
    QUIVER_REQUIRE(db, table, path);

    try {
        db->db.import_csv(table, path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
