#include "quiver/c/binary/binary_file.h"

#include "../database_helpers.h"
#include "../internal.h"

#include <new>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {

// Open/close

QUIVER_C_API quiver_error_t quiver_binary_file_open_read(const char* path, quiver_binary_file_t** out) {
    QUIVER_REQUIRE(path, out);

    try {
        auto binary_file = quiver::BinaryFile::open_file(path, 'r');
        *out = new quiver_binary_file(std::move(binary_file));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_file_open_write(const char* path,
                                                          quiver_binary_metadata_t* md,
                                                          quiver_binary_file_t** out) {
    QUIVER_REQUIRE(path, md, out);

    try {
        auto binary_file = quiver::BinaryFile::open_file(path, 'w', md->metadata);
        *out = new quiver_binary_file(std::move(binary_file));
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_file_close(quiver_binary_file_t* binary_file) {
    delete binary_file;
    return QUIVER_OK;
}

// I/O

QUIVER_C_API quiver_error_t quiver_binary_file_read(quiver_binary_file_t* binary_file,
                                                    const char* const* dim_names,
                                                    const int64_t* dim_values,
                                                    size_t dim_count,
                                                    int allow_nulls,
                                                    double** out_data,
                                                    size_t* out_count) {
    QUIVER_REQUIRE(binary_file, out_data, out_count);
    if (dim_count > 0) {
        QUIVER_REQUIRE(dim_names, dim_values);
    }

    try {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t i = 0; i < dim_count; ++i) {
            dims[dim_names[i]] = dim_values[i];
        }

        auto data = binary_file->binary_file.read(dims, allow_nulls != 0);
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

QUIVER_C_API quiver_error_t quiver_binary_file_write(quiver_binary_file_t* binary_file,
                                                     const char* const* dim_names,
                                                     const int64_t* dim_values,
                                                     size_t dim_count,
                                                     const double* data,
                                                     size_t data_count) {
    QUIVER_REQUIRE(binary_file, data);
    if (dim_count > 0) {
        QUIVER_REQUIRE(dim_names, dim_values);
    }

    try {
        std::unordered_map<std::string, int64_t> dims;
        for (size_t i = 0; i < dim_count; ++i) {
            dims[dim_names[i]] = dim_values[i];
        }

        std::vector<double> data_vec(data, data + data_count);
        binary_file->binary_file.write(data_vec, dims);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Dimension iteration

QUIVER_C_API quiver_error_t quiver_binary_file_next_dimensions(quiver_binary_file_t* binary_file,
                                                               const int64_t* current_dimensions,
                                                               size_t dim_count,
                                                               int64_t** out_dimensions,
                                                               size_t* out_count) {
    QUIVER_REQUIRE(binary_file, current_dimensions, out_dimensions, out_count);

    try {
        std::vector<int64_t> current(current_dimensions, current_dimensions + dim_count);
        auto result = binary_file->binary_file.next_dimensions(current);
        *out_count = result.size();
        *out_dimensions = new int64_t[result.size()];
        std::copy(result.begin(), result.end(), *out_dimensions);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Getters

QUIVER_C_API quiver_error_t quiver_binary_file_get_metadata(quiver_binary_file_t* binary_file,
                                                            quiver_binary_metadata_t** out) {
    QUIVER_REQUIRE(binary_file, out);

    try {
        *out = new quiver_binary_metadata{binary_file->binary_file.get_metadata()};
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_file_get_file_path(quiver_binary_file_t* binary_file, char** out) {
    QUIVER_REQUIRE(binary_file, out);

    *out = quiver::string::new_c_str(binary_file->binary_file.get_file_path());
    return QUIVER_OK;
}

// Free

QUIVER_C_API quiver_error_t quiver_binary_file_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_file_free_float_array(double* data) {
    delete[] data;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_file_free_int64_array(int64_t* data) {
    delete[] data;
    return QUIVER_OK;
}

}  // extern "C"
