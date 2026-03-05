#include "../database_helpers.h"
#include "../internal.h"
#include "quiver/c/blob/blob.h"

#include <new>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {

// Open/close

QUIVER_C_API quiver_error_t quiver_blob_open_read(const char* path, quiver_blob_t** out) {
    QUIVER_REQUIRE(path, out);

    try {
        auto blob = quiver::Blob::open_file(path, 'r');
        *out = new quiver_blob(std::move(blob));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_open_write(const char* path,
                                                    quiver_blob_metadata_t* md,
                                                    quiver_blob_t** out) {
    QUIVER_REQUIRE(path, md, out);

    try {
        auto blob = quiver::Blob::open_file(path, 'w', md->metadata);
        *out = new quiver_blob(std::move(blob));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_close(quiver_blob_t* blob) {
    delete blob;
    return QUIVER_OK;
}

// I/O

QUIVER_C_API quiver_error_t quiver_blob_read(quiver_blob_t* blob,
                                              const char* const* dim_names,
                                              const int64_t* dim_values,
                                              size_t dim_count,
                                              int allow_nulls,
                                              double** out_data,
                                              size_t* out_count) {
    QUIVER_REQUIRE(blob, out_data, out_count);

    try {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t i = 0; i < dim_count; ++i) {
            dims[dim_names[i]] = dim_values[i];
        }

        auto data = blob->blob.read(dims, allow_nulls != 0);
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

QUIVER_C_API quiver_error_t quiver_blob_write(quiver_blob_t* blob,
                                               const char* const* dim_names,
                                               const int64_t* dim_values,
                                               size_t dim_count,
                                               const double* data,
                                               size_t data_count) {
    QUIVER_REQUIRE(blob, data);

    try {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t i = 0; i < dim_count; ++i) {
            dims[dim_names[i]] = dim_values[i];
        }

        std::vector<double> data_vec(data, data + data_count);
        blob->blob.write(data_vec, dims);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Getters

QUIVER_C_API quiver_error_t quiver_blob_get_metadata(quiver_blob_t* blob, quiver_blob_metadata_t** out) {
    QUIVER_REQUIRE(blob, out);

    try {
        *out = new quiver_blob_metadata{blob->blob.get_metadata()};
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_get_file_path(quiver_blob_t* blob, const char** out) {
    QUIVER_REQUIRE(blob, out);

    *out = blob->blob.get_file_path().c_str();
    return QUIVER_OK;
}

// Free

QUIVER_C_API quiver_error_t quiver_blob_free_float_array(double* data) {
    delete[] data;
    return QUIVER_OK;
}

}  // extern "C"
