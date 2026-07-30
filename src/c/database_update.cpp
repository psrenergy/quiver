#include "internal.h"
#include "quiver/c/database.h"

#include <map>
#include <string>
#include <vector>

namespace {

// Decode the columnar typed-arrays + per-cell mask form into the row-shaped data the C++ core
// takes. Same contract as quiver_database_update_time_series_group: a NULL mask (for the whole
// parameter or for one column) means dense, and a masked-out cell becomes an explicit
// Value{nullptr} in every row so rows stay uniform.
std::vector<std::map<std::string, quiver::Value>> columns_to_rows(const char* caller,
                                                                  const char* const* column_names,
                                                                  const int* column_types,
                                                                  const void* const* column_data,
                                                                  const uint8_t* const* column_has_value,
                                                                  size_t column_count,
                                                                  size_t row_count) {
    std::vector<std::map<std::string, quiver::Value>> rows;
    rows.reserve(row_count);

    for (size_t r = 0; r < row_count; ++r) {
        std::map<std::string, quiver::Value> row;
        for (size_t c = 0; c < column_count; ++c) {
            std::string col_name(column_names[c]);
            const uint8_t* mask = column_has_value ? column_has_value[c] : nullptr;
            if (mask && mask[r] == 0) {
                row[col_name] = nullptr;
                continue;
            }
            switch (column_types[c]) {
            case QUIVER_DATA_TYPE_INTEGER:
                row[col_name] = static_cast<const int64_t*>(column_data[c])[r];
                break;
            case QUIVER_DATA_TYPE_FLOAT:
                row[col_name] = static_cast<const double*>(column_data[c])[r];
                break;
            case QUIVER_DATA_TYPE_STRING:
            case QUIVER_DATA_TYPE_DATE_TIME:
                row[col_name] = std::string(static_cast<const char* const*>(column_data[c])[r]);
                break;
            default:
                throw std::runtime_error(std::string("Cannot ") + caller + ": unknown column type " +
                                         std::to_string(column_types[c]));
            }
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

}  // namespace

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
        auto rows = columns_to_rows(
            "update_vector_group", column_names, column_types, column_data, column_has_value, column_count, row_count);
        db->db.update_vector_group(collection, group, id, rows);
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
        auto rows = columns_to_rows(
            "update_set_group", column_names, column_types, column_data, column_has_value, column_count, row_count);
        db->db.update_set_group(collection, group, id, rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

}  // extern "C"
