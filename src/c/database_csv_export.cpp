#include "database_options.h"
#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

extern "C" {

QUIVER_C_API quiver_csv_options_t quiver_csv_options_default(void) {
    return csv_options_default();
}

QUIVER_C_API quiver_error_t quiver_database_export_csv(quiver_database_t* db,
                                                       const char* collection,
                                                       const char* group,
                                                       const char* path,
                                                       const quiver_csv_options_t* opts) {
    QUIVER_REQUIRE(db, collection, group, path, opts);

    try {
        auto cpp_opts = convert_options(opts);
        db->db.export_csv(collection, group, path, cpp_opts);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
