#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_run_model(quiver_database_t* db, int* out_exit_code) {
    QUIVER_REQUIRE(db, out_exit_code);

    try {
        *out_exit_code = db->db.run_model();
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
