#include "quiver/c/database.h"

#include "database_helpers.h"
#include "internal.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_update_scalar_relation(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* attribute,
                                                                   const char* from_label,
                                                                   const char* to_label) {
    QUIVER_REQUIRE(db, collection, attribute, from_label, to_label);

    try {
        db->db.update_scalar_relation(collection, attribute, from_label, to_label);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_scalar_relation(quiver_database_t* db,
                                                                 const char* collection,
                                                                 const char* attribute,
                                                                 char*** out_values,
                                                                 size_t* out_count) {
    QUIVER_REQUIRE(db, collection, attribute, out_values, out_count);

    try {
        return copy_strings_to_c(db->db.read_scalar_relation(collection, attribute), out_values, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
