#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/c/options.h"
#include "quiver/options.h"

#include <string>

static quiver::CSVOptions convert_options(const quiver_csv_options_t* opts) {
    quiver::CSVOptions cpp_opts;
    cpp_opts.date_time_format = opts->date_time_format ? opts->date_time_format : "";

    size_t offset = 0;
    for (size_t i = 0; i < opts->enum_group_count; ++i) {
        std::string attr_name = opts->enum_attribute_names[i];
        std::string locale_name = opts->enum_locale_names[i];
        auto& locale_map = cpp_opts.enum_labels[attr_name][locale_name];
        for (size_t j = 0; j < opts->enum_entry_counts[i]; ++j) {
            locale_map[opts->enum_labels[offset + j]] = opts->enum_values[offset + j];
        }
        offset += opts->enum_entry_counts[i];
    }

    return cpp_opts;
}

extern "C" {

QUIVER_C_API quiver_csv_options_t quiver_csv_options_default(void) {
    quiver_csv_options_t opts = {};
    opts.date_time_format = "";
    return opts;
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

QUIVER_C_API quiver_error_t quiver_database_import_csv(quiver_database_t* db,
                                                       const char* collection,
                                                       const char* group,
                                                       const char* path,
                                                       const quiver_csv_options_t* opts) {
    QUIVER_REQUIRE(db, collection, group, path, opts);

    try {
        auto cpp_opts = convert_options(opts);
        db->db.import_csv(collection, group, path, cpp_opts);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
