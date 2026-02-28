#include "database_options.h"
#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_import_csv(quiver_database_t* db,
                                                       const char* collection,
                                                       const char* group,
                                                       const char* path,
                                                       const quiver_csv_options_t* options) {
    QUIVER_REQUIRE(db, collection, group, path, options);

    try {
        auto cpp_options = convert_csv_options(options);
        db->db.import_csv(collection, group, path, cpp_options);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}
}