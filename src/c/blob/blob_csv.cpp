#include "../internal.h"
#include "quiver/c/blob/blob_csv.h"
#include "quiver/blob/blob_csv.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_blob_csv_bin_to_csv(const char* path, int aggregate_time_dimensions) {
    QUIVER_REQUIRE(path);

    try {
        quiver::BlobCSV::bin_to_csv(path, aggregate_time_dimensions != 0);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_csv_csv_to_bin(const char* path) {
    QUIVER_REQUIRE(path);

    try {
        quiver::BlobCSV::csv_to_bin(path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
