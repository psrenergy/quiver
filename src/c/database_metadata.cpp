#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

#include <cstddef>

// Layout pin (D-15, Phase 3 Task 1 freeze): quiver_ui_metadata_t must stay hole-free at exactly
// 64 bytes. Fields are grouped by type (six pointers, then the int64_t, then the two ints)
// deliberately deviating from quiver_scalar_metadata_t's declaration-order-with-padding shape --
// see include/quiver/c/database.h's comment on the struct. Mirrors the six static_asserts in
// src/c/options.cpp for quiver_database_options_t.
static_assert(sizeof(quiver_ui_metadata_t) == 64, "quiver_ui_metadata_t must stay 64 bytes, hole-free");
static_assert(offsetof(quiver_ui_metadata_t, label) == 0, "label must be at offset 0");
static_assert(offsetof(quiver_ui_metadata_t, tooltip) == 8, "tooltip must be at offset 8");
static_assert(offsetof(quiver_ui_metadata_t, unit) == 16, "unit must be at offset 16");
static_assert(offsetof(quiver_ui_metadata_t, format) == 24, "format must be at offset 24");
static_assert(offsetof(quiver_ui_metadata_t, icon) == 32, "icon must be at offset 32");
static_assert(offsetof(quiver_ui_metadata_t, vocabulary) == 40, "vocabulary must be at offset 40");
static_assert(offsetof(quiver_ui_metadata_t, display_order) == 48, "display_order must be at offset 48");
static_assert(offsetof(quiver_ui_metadata_t, configured) == 56, "configured must be at offset 56");
static_assert(offsetof(quiver_ui_metadata_t, hidden) == 60, "hidden must be at offset 60");

// Converter/free pair for the single-record UI metadata getter (D-42), modelled on
// convert_scalar_to_c/free_scalar_fields (src/c/database_helpers.h). Every one of the six string
// fields is allocated with quiver::string::new_c_str unconditionally, including when empty, so no
// field is ever NULL (D-13: absence is spelled empty string, not a null pointer). Use new_c_str /
// delete[] and no other allocator pair -- src/c/CLAUDE.md "String Handling".
namespace {

void convert_ui_metadata_to_c(const quiver::UIMetadata& src, quiver_ui_metadata_t& dst) {
    dst.label = quiver::string::new_c_str(src.label);
    dst.tooltip = quiver::string::new_c_str(src.tooltip);
    dst.unit = quiver::string::new_c_str(src.unit);
    dst.format = quiver::string::new_c_str(src.format);
    dst.icon = quiver::string::new_c_str(src.icon);
    dst.vocabulary = quiver::string::new_c_str(src.vocabulary);
    dst.display_order = src.display_order;
    dst.configured = src.configured ? 1 : 0;
    dst.hidden = src.hidden ? 1 : 0;
}

void free_ui_metadata_fields(quiver_ui_metadata_t& m) {
    delete[] m.label;
    delete[] m.tooltip;
    delete[] m.unit;
    delete[] m.format;
    delete[] m.icon;
    delete[] m.vocabulary;
}

}  // namespace

extern "C" {

// Native sizeof accessors (SAFE-01/D-07). No QUIVER_REQUIRE, no try/catch -- a sizeof cannot
// throw (see the exception list in src/c/CLAUDE.md).

QUIVER_C_API size_t quiver_scalar_metadata_sizeof(void) {
    return sizeof(quiver_scalar_metadata_t);
}

QUIVER_C_API size_t quiver_group_metadata_sizeof(void) {
    return sizeof(quiver_group_metadata_t);
}

QUIVER_C_API size_t quiver_ui_metadata_sizeof(void) {
    return sizeof(quiver_ui_metadata_t);
}

// Metadata get functions

QUIVER_C_API quiver_error_t quiver_database_get_scalar_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* attribute,
                                                                quiver_scalar_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, attribute, out_metadata);

    try {
        convert_scalar_to_c(db->db.get_scalar_metadata(collection, attribute), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_vector_metadata(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group_name,
                                                                quiver_group_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, group_name, out_metadata);

    try {
        convert_group_to_c(db->db.get_vector_metadata(collection, group_name), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_set_metadata(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* group_name,
                                                             quiver_group_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, group_name, out_metadata);

    try {
        convert_group_to_c(db->db.get_set_metadata(collection, group_name), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_get_attribute_ui_metadata(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* attribute,
                                                                      quiver_ui_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, attribute, out_metadata);

    try {
        convert_ui_metadata_to_c(db->db.get_attribute_ui_metadata(collection, attribute), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Metadata free functions (co-located with get/list)

QUIVER_C_API quiver_error_t quiver_database_free_scalar_metadata(quiver_scalar_metadata_t* metadata) {
    QUIVER_REQUIRE(metadata);

    free_scalar_fields(*metadata);
    metadata->name = nullptr;
    metadata->default_value = nullptr;
    metadata->references_collection = nullptr;
    metadata->references_column = nullptr;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_free_group_metadata(quiver_group_metadata_t* metadata) {
    QUIVER_REQUIRE(metadata);

    free_group_fields(*metadata);
    metadata->group_name = nullptr;
    metadata->dimension_column = nullptr;
    metadata->value_columns = nullptr;
    metadata->value_column_count = 0;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_free_ui_metadata(quiver_ui_metadata_t* metadata) {
    QUIVER_REQUIRE(metadata);

    free_ui_metadata_fields(*metadata);
    metadata->label = nullptr;
    metadata->tooltip = nullptr;
    metadata->unit = nullptr;
    metadata->format = nullptr;
    metadata->icon = nullptr;
    metadata->vocabulary = nullptr;
    return QUIVER_OK;
}

// Metadata list functions

QUIVER_C_API quiver_error_t quiver_database_list_scalar_attributes(quiver_database_t* db,
                                                                   const char* collection,
                                                                   quiver_scalar_metadata_t** out_metadata,
                                                                   size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto attributes = db->db.list_scalar_attributes(collection);
        *out_count = attributes.size();
        if (attributes.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_scalar_metadata_t[attributes.size()];
        for (size_t i = 0; i < attributes.size(); ++i) {
            convert_scalar_to_c(attributes[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_vector_groups(quiver_database_t* db,
                                                               const char* collection,
                                                               quiver_group_metadata_t** out_metadata,
                                                               size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto groups = db->db.list_vector_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_group_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            convert_group_to_c(groups[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_set_groups(quiver_database_t* db,
                                                            const char* collection,
                                                            quiver_group_metadata_t** out_metadata,
                                                            size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto groups = db->db.list_set_groups(collection);
        *out_count = groups.size();
        if (groups.empty()) {
            *out_metadata = nullptr;
            return QUIVER_OK;
        }
        *out_metadata = new quiver_group_metadata_t[groups.size()];
        for (size_t i = 0; i < groups.size(); ++i) {
            convert_group_to_c(groups[i], (*out_metadata)[i]);
        }
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Metadata array free functions (co-located with list)

QUIVER_C_API quiver_error_t quiver_database_free_scalar_metadata_array(quiver_scalar_metadata_t* metadata,
                                                                       size_t count) {
    QUIVER_REQUIRE(metadata);

    for (size_t i = 0; i < count; ++i) {
        free_scalar_fields(metadata[i]);
    }
    delete[] metadata;
    return QUIVER_OK;
}

QUIVER_C_API quiver_error_t quiver_database_free_group_metadata_array(quiver_group_metadata_t* metadata, size_t count) {
    QUIVER_REQUIRE(metadata);

    for (size_t i = 0; i < count; ++i) {
        free_group_fields(metadata[i]);
    }
    delete[] metadata;
    return QUIVER_OK;
}

}  // extern "C"
