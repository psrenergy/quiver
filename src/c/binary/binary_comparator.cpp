#include "quiver/c/binary/binary_comparator.h"

#include "../database_helpers.h"
#include "../internal.h"
#include "quiver/binary/binary_comparator.h"

#include <string>

extern "C" {

QUIVER_C_API quiver_compare_options_t quiver_compare_options_default(void) {
    return {1e-6, 0.0, 0, 100};
}

QUIVER_C_API quiver_error_t quiver_binary_compare_files(const char* path1,
                                                        const char* path2,
                                                        const quiver_compare_options_t* options,
                                                        quiver_compare_status_t* out_status,
                                                        char** out_report) {
    QUIVER_REQUIRE(path1, path2, options, out_status, out_report);

    try {
        auto result = quiver::BinaryComparator::compare(path1,
                                                        path2,
                                                        {.absolute_tolerance = options->absolute_tolerance,
                                                         .relative_tolerance = options->relative_tolerance,
                                                         .detailed_report = options->detailed_report != 0,
                                                         .max_report_lines = options->max_report_lines});

        switch (result.status) {
        case quiver::CompareStatus::FileMatch:
            *out_status = QUIVER_COMPARE_FILE_MATCH;
            break;
        case quiver::CompareStatus::MetadataMismatch:
            *out_status = QUIVER_COMPARE_METADATA_MISMATCH;
            break;
        case quiver::CompareStatus::DataMismatch:
            *out_status = QUIVER_COMPARE_DATA_MISMATCH;
            break;
        }

        if (result.report.empty()) {
            *out_report = nullptr;
        } else {
            *out_report = quiver::string::new_c_str(result.report);
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Free

QUIVER_C_API quiver_error_t quiver_binary_comparator_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

}  // extern "C"
