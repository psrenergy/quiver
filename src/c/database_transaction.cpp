#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_begin_transaction(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.begin_transaction();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_commit(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.commit();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_rollback(quiver_database_t* db) {
    QUIVER_REQUIRE(db);

    try {
        db->db.rollback();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_in_transaction(quiver_database_t* db, bool* out_active) {
    QUIVER_REQUIRE(db, out_active);

    *out_active = db->db.in_transaction();
    return QUIVER_OK;
}

}  // extern "C"
