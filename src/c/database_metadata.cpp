#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

extern "C" {

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
