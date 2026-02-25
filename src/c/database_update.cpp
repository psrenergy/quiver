#include "internal.h"
#include "quiver/c/database.h"

#include <string>

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_update_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id,
                                                           const quiver_element_t* element) {
    QUIVER_REQUIRE(db, collection, element);

    try {
        db->db.update_element(collection, id, element->element);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
