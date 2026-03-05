#include "quiver/c/blob/blob_metadata.h"

#include "../database_helpers.h"
#include "../internal.h"
#include "utils/datetime.h"

#include <chrono>
#include <new>
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

QUIVER_C_API quiver_error_t quiver_blob_metadata_create(quiver_blob_metadata_t** out) {
    QUIVER_REQUIRE(out);

    try {
        *out = new quiver_blob_metadata{quiver::BlobMetadata{}};
        return QUIVER_OK;
    } catch (const std::bad_alloc&) {
        quiver_set_last_error("Memory allocation failed");
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_destroy(quiver_blob_metadata_t* md) {
    delete md;
    return QUIVER_OK;
}

// Factories

QUIVER_C_API quiver_error_t quiver_blob_metadata_from_toml(const char* toml, quiver_blob_metadata_t** out) {
    QUIVER_REQUIRE(toml, out);

    try {
        auto metadata = quiver::BlobMetadata::from_toml(toml);
        *out = new quiver_blob_metadata{std::move(metadata)};
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_from_element(quiver_element_t* el, quiver_blob_metadata_t** out) {
    QUIVER_REQUIRE(el, out);

    try {
        auto metadata = quiver::BlobMetadata::from_element(el->element);
        *out = new quiver_blob_metadata{std::move(metadata)};
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Serialization

QUIVER_C_API quiver_error_t quiver_blob_metadata_to_toml(quiver_blob_metadata_t* md, char** out_toml) {
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

// Builders

QUIVER_C_API quiver_error_t quiver_blob_metadata_set_initial_datetime(quiver_blob_metadata_t* md, const char* iso8601) {
    QUIVER_REQUIRE(md, iso8601);

    try {
        std::tm tm{};
        if (!quiver::datetime::parse_iso8601(iso8601, tm)) {
            throw std::runtime_error(std::string("Failed to parse initial_datetime: ") + iso8601);
        }
        md->metadata.initial_datetime =
            std::chrono::system_clock::from_time_t(quiver::datetime::mkgmtime_portable(&tm));
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_set_unit(quiver_blob_metadata_t* md, const char* unit) {
    QUIVER_REQUIRE(md, unit);

    md->metadata.unit = unit;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_set_version(quiver_blob_metadata_t* md, const char* version) {
    QUIVER_REQUIRE(md, version);

    md->metadata.version = version;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_set_labels(quiver_blob_metadata_t* md,
                                                            const char* const* labels,
                                                            size_t count) {
    QUIVER_REQUIRE(md, labels);

    md->metadata.labels.clear();
    md->metadata.labels.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        md->metadata.labels.emplace_back(labels[i]);
    }
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_add_dimension(quiver_blob_metadata_t* md,
                                                               const char* name,
                                                               int64_t size) {
    QUIVER_REQUIRE(md, name);

    try {
        md->metadata.add_dimension(name, size);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_add_time_dimension(quiver_blob_metadata_t* md,
                                                                    const char* name,
                                                                    int64_t size,
                                                                    const char* frequency) {
    QUIVER_REQUIRE(md, name, frequency);

    try {
        md->metadata.add_time_dimension(name, size, frequency);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Getters

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_unit(quiver_blob_metadata_t* md, const char** out) {
    QUIVER_REQUIRE(md, out);

    *out = md->metadata.unit.c_str();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_version(quiver_blob_metadata_t* md, const char** out) {
    QUIVER_REQUIRE(md, out);

    *out = md->metadata.version.c_str();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_initial_datetime(quiver_blob_metadata_t* md, char** out) {
    QUIVER_REQUIRE(md, out);

    try {
        auto datetime_str = quiver::datetime::format_utc_datetime(md->metadata.initial_datetime);
        *out = quiver::string::new_c_str(datetime_str);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_number_of_time_dimensions(quiver_blob_metadata_t* md,
                                                                               int64_t* out) {
    QUIVER_REQUIRE(md, out);

    *out = md->metadata.number_of_time_dimensions;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_labels(quiver_blob_metadata_t* md,
                                                            char*** out,
                                                            size_t* out_count) {
    QUIVER_REQUIRE(md, out, out_count);

    return copy_strings_to_c(md->metadata.labels, out, out_count);
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_dimension_count(quiver_blob_metadata_t* md, size_t* out) {
    QUIVER_REQUIRE(md, out);

    *out = md->metadata.dimensions.size();
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_get_dimension(quiver_blob_metadata_t* md,
                                                               size_t index,
                                                               quiver_dimension_t* out) {
    QUIVER_REQUIRE(md, out);

    if (index >= md->metadata.dimensions.size()) {
        quiver_set_last_error("Dimension index out of range");
        return QUIVER_ERROR;
    }

    convert_dimension_to_c(md->metadata.dimensions[index], *out);
    return QUIVER_OK;
}

// Free helpers

QUIVER_C_API quiver_error_t quiver_blob_metadata_free_string(char* str) {
    delete[] str;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_free_string_array(char** strs, size_t count) {
    if (!strs)
        return QUIVER_OK;

    for (size_t i = 0; i < count; ++i) {
        delete[] strs[i];
    }
    delete[] strs;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_blob_metadata_free_dimension(quiver_dimension_t* dim) {
    QUIVER_REQUIRE(dim);

    delete[] dim->name;
    dim->name = nullptr;
    return QUIVER_OK;
}

}  // extern "C"
