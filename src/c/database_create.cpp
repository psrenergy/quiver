#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/c/element.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_create_element(quiver_database_t* db,
                                                           const char* collection,
                                                           quiver_element_t* element,
                                                           int64_t* out_id) {
    QUIVER_REQUIRE(db, collection, element, out_id);

    try {
        *out_id = db->db.create_element(collection, element->element);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

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

QUIVER_C_API quiver_error_t quiver_database_delete_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id) {
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
