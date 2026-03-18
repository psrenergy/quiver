#include "quiver/c/binary/binary.h"

#include "../database_helpers.h"
#include "../internal.h"

#include <new>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {

// Open/close

QUIVER_C_API quiver_error_t quiver_binary_open_read(const char* path, quiver_binary_t** out) {
    QUIVER_REQUIRE(path, out);

    try {
        auto binary = quiver::Binary::open_file(path, 'r');
        *out = new quiver_binary(std::move(binary));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_open_write(const char* path,
                                                     quiver_binary_metadata_t* md,
                                                     quiver_binary_t** out) {
    QUIVER_REQUIRE(path, md, out);

    try {
        auto binary = quiver::Binary::open_file(path, 'w', md->metadata);
        *out = new quiver_binary(std::move(binary));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_close(quiver_binary_t* binary) {
    delete binary;
    return QUIVER_OK;
}

// I/O

QUIVER_C_API quiver_error_t quiver_binary_read(quiver_binary_t* binary,
                                               const char* const* dim_names,
                                               const int64_t* dim_values,
                                               size_t dim_count,
                                               int allow_nulls,
                                               double** out_data,
                                               size_t* out_count) {
    QUIVER_REQUIRE(binary, out_data, out_count);
    if (dim_count > 0) {
        QUIVER_REQUIRE(dim_names, dim_values);
    }

    try {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t i = 0; i < dim_count; ++i) {
            dims[dim_names[i]] = dim_values[i];
        }

        auto data = binary->binary.read(dims, allow_nulls != 0);
        *out_count = data.size();
        if (data.empty()) {
            *out_data = nullptr;
        } else {
            *out_data = new double[data.size()];
            std::copy(data.begin(), data.end(), *out_data);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_write(quiver_binary_t* binary,
                                                const char* const* dim_names,
                                                const int64_t* dim_values,
                                                size_t dim_count,
                                                const double* data,
                                                size_t data_count) {
    QUIVER_REQUIRE(binary, data);
    if (dim_count > 0) {
        QUIVER_REQUIRE(dim_names, dim_values);
    }

    try {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t i = 0; i < dim_count; ++i) {
            dims[dim_names[i]] = dim_values[i];
        }

        std::vector<double> data_vec(data, data + data_count);
        binary->binary.write(data_vec, dims);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Getters

QUIVER_C_API quiver_error_t quiver_binary_get_metadata(quiver_binary_t* binary, quiver_binary_metadata_t** out) {
    QUIVER_REQUIRE(binary, out);

    try {
        *out = new quiver_binary_metadata{binary->binary.get_metadata()};
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_get_file_path(quiver_binary_t* binary, char** out) {
    QUIVER_REQUIRE(binary, out);

    *out = quiver::string::new_c_str(binary->binary.get_file_path());
    return QUIVER_OK;
}

// Compare

QUIVER_C_API quiver_error_t quiver_binary_compare_files(const char* path1,
                                                        const char* path2,
                                                        int detailed_report,
                                                        quiver_compare_status_t* out_status,
                                                        char** out_report) {
    QUIVER_REQUIRE(path1, path2, out_status, out_report);

    try {
        auto result = quiver::Binary::compare_files(path1, path2, detailed_report != 0);

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

QUIVER_C_API quiver_error_t quiver_binary_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_free_float_array(double* data) {
    delete[] data;
    return QUIVER_OK;
}

}  // extern "C"
