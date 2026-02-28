#include "quiver/c/options.h"

extern "C" {

QUIVER_C_API quiver_database_options_t quiver_database_options_default(void) {
    return {0, QUIVER_LOG_INFO};
}

QUIVER_C_API quiver_csv_options_t quiver_csv_options_default(void) {
    quiver_csv_options_t options = {};
    options.date_time_format = "";
    return options;
}

}  // extern "C"
