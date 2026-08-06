#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"

#include <map>
#include <optional>
#include <string>
#include <vector>

extern "C" {

QUIVER_C_API quiver_error_t quiver_database_update_element(quiver_database_t* db,
                                                           const char* collection,
                                                           int64_t id,
                                                           const quiver_element_t* element) {
    QUIVER_REQUIRE(db, collection, element);

    try {
        db->db.update_element(collection, id, element->element);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_element_by_label(quiver_database_t* db,
                                                                    const char* collection,
                                                                    const char* label,
                                                                    const quiver_element_t* element) {
    QUIVER_REQUIRE(db, collection, label, element);

    try {
        db->db.update_element(collection, std::string(label), element->element);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_relation(quiver_database_t* db,
                                                            const char* collection_from,
                                                            const char* collection_to,
                                                            const char* relation_type,
                                                            int64_t id,
                                                            const char* target_label) {
    QUIVER_REQUIRE(db, collection_from, collection_to, relation_type);

    try {
        db->db.update_relation(collection_from,
                               collection_to,
                               relation_type,
                               id,
                               target_label ? std::optional<std::string>(target_label) : std::nullopt);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_relation_by_label(quiver_database_t* db,
                                                                     const char* collection_from,
                                                                     const char* collection_to,
                                                                     const char* relation_type,
                                                                     const char* label,
                                                                     const char* target_label) {
    QUIVER_REQUIRE(db, collection_from, collection_to, relation_type, label);

    try {
        db->db.update_relation(collection_from,
                               collection_to,
                               relation_type,
                               std::string(label),
                               target_label ? std::optional<std::string>(target_label) : std::nullopt);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_group(quiver_database_t* db,
                                                                const char* collection,
                                                                const char* group,
                                                                int64_t id,
                                                                const char* const* column_names,
                                                                const int* column_types,
                                                                const void* const* column_data,
                                                                const uint8_t* const* column_has_value,
                                                                size_t column_count,
                                                                size_t row_count) {
    QUIVER_REQUIRE(db, collection, group);
    if (column_count > 0) {
        QUIVER_REQUIRE(column_names, column_types, column_data);
    }

    try {
        auto rows = unmarshal_group_columns_to_rows(
            "update_vector_group", column_names, column_types, column_data, column_has_value, column_count, row_count);
        db->db.update_vector_group(collection, group, id, rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_vector_group_by_label(quiver_database_t* db,
                                                                         const char* collection,
                                                                         const char* group,
                                                                         const char* label,
                                                                         const char* const* column_names,
                                                                         const int* column_types,
                                                                         const void* const* column_data,
                                                                         const uint8_t* const* column_has_value,
                                                                         size_t column_count,
                                                                         size_t row_count) {
    QUIVER_REQUIRE(db, collection, group, label);
    if (column_count > 0) {
        QUIVER_REQUIRE(column_names, column_types, column_data);
    }

    try {
        auto rows = unmarshal_group_columns_to_rows(
            "update_vector_group", column_names, column_types, column_data, column_has_value, column_count, row_count);
        db->db.update_vector_group(collection, group, std::string(label), rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_group(quiver_database_t* db,
                                                             const char* collection,
                                                             const char* group,
                                                             int64_t id,
                                                             const char* const* column_names,
                                                             const int* column_types,
                                                             const void* const* column_data,
                                                             const uint8_t* const* column_has_value,
                                                             size_t column_count,
                                                             size_t row_count) {
    QUIVER_REQUIRE(db, collection, group);
    if (column_count > 0) {
        QUIVER_REQUIRE(column_names, column_types, column_data);
    }

    try {
        auto rows = unmarshal_group_columns_to_rows(
            "update_set_group", column_names, column_types, column_data, column_has_value, column_count, row_count);
        db->db.update_set_group(collection, group, id, rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_set_group_by_label(quiver_database_t* db,
                                                                      const char* collection,
                                                                      const char* group,
                                                                      const char* label,
                                                                      const char* const* column_names,
                                                                      const int* column_types,
                                                                      const void* const* column_data,
                                                                      const uint8_t* const* column_has_value,
                                                                      size_t column_count,
                                                                      size_t row_count) {
    QUIVER_REQUIRE(db, collection, group, label);
    if (column_count > 0) {
        QUIVER_REQUIRE(column_names, column_types, column_data);
    }

    try {
        auto rows = unmarshal_group_columns_to_rows(
            "update_set_group", column_names, column_types, column_data, column_has_value, column_count, row_count);
        db->db.update_set_group(collection, group, std::string(label), rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
