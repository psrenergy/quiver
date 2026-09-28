#include "quiver/c/binary/binary_metadata.h"

#include "../database_helpers.h"
#include "../internal.h"
#include "utils/datetime.h"

#include <string>
#include <vector>

namespace {

quiver_time_frequency_t to_c_frequency(quiver::TimeFrequency freq) {
    switch (freq) {
    case quiver::TimeFrequency::Yearly:
        return QUIVER_TIME_FREQUENCY_YEARLY;
    case quiver::TimeFrequency::Monthly:
        return QUIVER_TIME_FREQUENCY_MONTHLY;
    case quiver::TimeFrequency::Weekly:
        return QUIVER_TIME_FREQUENCY_WEEKLY;
    case quiver::TimeFrequency::Daily:
        return QUIVER_TIME_FREQUENCY_DAILY;
    case quiver::TimeFrequency::Hourly:
        return QUIVER_TIME_FREQUENCY_HOURLY;
    }
    return QUIVER_TIME_FREQUENCY_YEARLY;
}

void convert_dimension_to_c(const quiver::Dimension& src, quiver_dimension_t& dst) {
    dst.name = quiver::string::new_c_str(src.name);
    dst.size = src.size;
    dst.is_time_dimension = src.is_time_dimension() ? 1 : 0;
    if (src.is_time_dimension()) {
        dst.time_properties.frequency = to_c_frequency(src.time->frequency);
        dst.time_properties.initial_value = src.time->initial_value;
        dst.time_properties.parent_dimension_index = src.time->parent_dimension_index;
    } else {
        dst.time_properties = {};
    }
}

}  // namespace

extern "C" {

// Lifecycle

QUIVER_C_API quiver_error_t quiver_binary_metadata_free(quiver_binary_metadata_t* md) {
    delete md;
    return QUIVER_OK;
}

// Factories

QUIVER_C_API quiver_error_t quiver_binary_metadata_from_toml(const char* toml, quiver_binary_metadata_t** out) {
    QUIVER_REQUIRE(toml, out);

    try {
        auto metadata = quiver::BinaryMetadata::from_toml_content(toml);
        *out = new quiver_binary_metadata{std::move(metadata)};
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_from_element(quiver_element_t* el, quiver_binary_metadata_t** out) {
    QUIVER_REQUIRE(el, out);

    try {
        auto metadata = quiver::BinaryMetadata::from_element(el->element);
        *out = new quiver_binary_metadata{std::move(metadata)};
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Serialization

QUIVER_C_API quiver_error_t quiver_binary_metadata_to_toml(quiver_binary_metadata_t* md, char** out_toml) {
    QUIVER_REQUIRE(md, out_toml);

    try {
        auto toml = md->metadata.to_toml();
        *out_toml = quiver::string::new_c_str(toml);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Getters

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_unit(quiver_binary_metadata_t* md, char** out) {
    QUIVER_REQUIRE(md, out);

    *out = quiver::string::new_c_str(md->metadata.unit);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_version(quiver_binary_metadata_t* md, char** out) {
    QUIVER_REQUIRE(md, out);

    *out = quiver::string::new_c_str(md->metadata.version);
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_initial_datetime(quiver_binary_metadata_t* md, char** out) {
    QUIVER_REQUIRE(md, out);

    try {
        auto datetime_str = quiver::datetime::format_utc(md->metadata.initial_datetime);
        *out = quiver::string::new_c_str(datetime_str);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_number_of_time_dimensions(quiver_binary_metadata_t* md,
                                                                                 int64_t* out) {
    QUIVER_REQUIRE(md, out);

    *out = md->metadata.number_of_time_dimensions();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_labels(quiver_binary_metadata_t* md,
                                                              char*** out,
                                                              size_t* out_count) {
    QUIVER_REQUIRE(md, out, out_count);

    return copy_strings_to_c(md->metadata.labels, out, out_count);
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_dimension_count(quiver_binary_metadata_t* md, size_t* out) {
    QUIVER_REQUIRE(md, out);

    *out = md->metadata.dimensions.size();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_get_dimension(quiver_binary_metadata_t* md,
                                                                 size_t index,
                                                                 quiver_dimension_t* out) {
    QUIVER_REQUIRE(md, out);

    if (index >= md->metadata.dimensions.size()) {
        quiver_set_last_error("Cannot get_dimension: index out of range");
        return QUIVER_ERROR;
    }

    convert_dimension_to_c(md->metadata.dimensions[index], *out);
    return QUIVER_OK;
}

// Free helpers

QUIVER_C_API quiver_error_t quiver_binary_metadata_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_free_string_array(char** strs, size_t count) {
    if (!strs)
        return QUIVER_OK;

    for (size_t i = 0; i < count; ++i) {
        delete[] strs[i];
    }
    delete[] strs;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_binary_metadata_free_dimension(quiver_dimension_t* dim) {
    QUIVER_REQUIRE(dim);

    delete[] dim->name;
    dim->name = nullptr;
    return QUIVER_OK;
}

}  // extern "C"
