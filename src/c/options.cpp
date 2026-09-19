#include "quiver/c/options.h"

#include <cstddef>

extern "C" {

// Layout pin (T-02-01): the four Wave-2 binding plans hardcode this size and these offsets.
// A future reordering or field insertion fails the build here rather than the install.
static_assert(sizeof(quiver_database_options_t) == 24, "quiver_database_options_t must stay 24 bytes");
static_assert(offsetof(quiver_database_options_t, read_only) == 0, "read_only must be at offset 0");
static_assert(offsetof(quiver_database_options_t, console_level) == 4, "console_level must be at offset 4");
static_assert(offsetof(quiver_database_options_t, ui_config_dir) == 8, "ui_config_dir must be at offset 8");
static_assert(offsetof(quiver_database_options_t, ui_locale) == 16, "ui_locale must be at offset 16");

QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return {0, QUIVER_LOG_INFO, nullptr, nullptr};
}

QUIVER_C_API size_t quiver_database_options_sizeof(void) {
    return sizeof(quiver_database_options_t);
}

QUIVER_C_API quiver_csv_options_t quiver_csv_options_default(void) {
    quiver_csv_options_t options = {};
    options.date_time_format = "";
    return options;
}

}  // extern "C"
