#include "quiver/c/binary/binary_csv.h"

#include "../internal.h"
#include "quiver/binary/binary_csv.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_binary_csv_bin_to_csv(const char* path, int aggregate_time_dimensions) {
    QUIVER_REQUIRE(path);

    try {
        quiver::CSVConverter::bin_to_csv(path, aggregate_time_dimensions != 0);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_csv_csv_to_bin(const char* path) {
    QUIVER_REQUIRE(path);

    try {
        quiver::CSVConverter::csv_to_bin(path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
