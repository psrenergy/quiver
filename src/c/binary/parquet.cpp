#include "quiver/c/binary/parquet.h"

#include "../internal.h"
#include "quiver/binary/parquet.h"

extern "C" {

QUIVER_C_API quiver_error_t quiver_bin_to_parquet(const char* path) {
    QUIVER_REQUIRE(path);
    try {
        quiver::bin_to_parquet(path);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
