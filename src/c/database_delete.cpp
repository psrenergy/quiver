#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_delete_element(quiver_database_t* db, const char* collection, int64_t id) {
    QUIVER_REQUIRE(db, collection);

    try {
        db->db.delete_element(collection, id);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
