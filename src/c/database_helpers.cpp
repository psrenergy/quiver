#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_number_of_elements(quiver_database_t* db,
                                                               const char* collection,
                                                               int64_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_count);

    try {
        *out_count = db->db.number_of_elements(collection);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
