#include "database_helpers.h"
#include "internal.h"
#include "quiver/c/database.h"
#include "quiver/data_type.h"

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// Helper to convert quiver_data_type_t enum value to human-readable name for error messages
static const char* c_type_name(int type) {
    switch (type) {
    case QUIVER_DATA_TYPE_INTEGER:
        return "INTEGER";
    case QUIVER_DATA_TYPE_FLOAT:
        return "FLOAT";
    case QUIVER_DATA_TYPE_STRING:
        return "STRING";
    case QUIVER_DATA_TYPE_DATE_TIME:
        return "DATE_TIME";
    default:
        return "UNKNOWN";
    }
}

extern "C" {

// Time series metadata

QUIVER_C_API quiver_error_t quiver_database_get_time_series_metadata(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* group_name,
                                                                     quiver_group_metadata_t* out_metadata) {
    QUIVER_REQUIRE(db, collection, group_name, out_metadata);

    try {
        convert_group_to_c(db->db.get_time_series_metadata(collection, group_name), *out_metadata);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_time_series_groups(quiver_database_t* db,
                                                                    const char* collection,
                                                                    quiver_group_metadata_t** out_metadata,
                                                                    size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_metadata, out_count);

    try {
        auto groups = db->db.list_time_series_groups(collection);
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

// Time series read/update

QUIVER_C_API quiver_error_t quiver_database_read_time_series_group(quiver_database_t* db,
                                                                   const char* collection,
                                                                   const char* group,
                                                                   int64_t id,
                                                                   char*** out_column_names,
                                                                   int** out_column_types,
                                                                   void*** out_column_data,
                                                                   size_t* out_column_count,
                                                                   size_t* out_row_count) {
    QUIVER_REQUIRE(db, collection, group, out_column_names, out_column_types);
    QUIVER_REQUIRE(out_column_data, out_column_count, out_row_count);

    try {
        auto metadata = db->db.get_time_series_metadata(collection, group);
        auto rows = db->db.read_time_series_group(collection, group, id);

        if (rows.empty()) {
            *out_column_names = nullptr;
            *out_column_types = nullptr;
            *out_column_data = nullptr;
            *out_column_count = 0;
            *out_row_count = 0;
            return QUIVER_OK;
        }

        // Build column list from metadata: dimension first, then value columns in schema order
        std::vector<std::pair<std::string, int>> columns;
        columns.push_back({metadata.dimension_column, QUIVER_DATA_TYPE_STRING});
        for (const auto& vc : metadata.value_columns) {
            columns.push_back({vc.name, to_c_data_type(vc.data_type)});
        }

        size_t col_count = columns.size();
        size_t row_count = rows.size();

        // Allocate outer arrays
        *out_column_names = new char*[col_count];
        *out_column_types = new int[col_count];
        *out_column_data = new void*[col_count];

        // Initialize column_data to nullptr for leak-safe cleanup on exception
        for (size_t c = 0; c < col_count; ++c) {
            (*out_column_data)[c] = nullptr;
        }

        try {
            for (size_t c = 0; c < col_count; ++c) {
                (*out_column_names)[c] = quiver::string::new_c_str(columns[c].first);
                (*out_column_types)[c] = columns[c].second;

                switch (columns[c].second) {
                case QUIVER_DATA_TYPE_INTEGER: {
                    auto* arr = new int64_t[row_count];
                    for (size_t r = 0; r < row_count; ++r) {
                        auto& val = rows[r].at(columns[c].first);
                        if (std::holds_alternative<int64_t>(val))
                            arr[r] = std::get<int64_t>(val);
                        else if (std::holds_alternative<double>(val))
                            arr[r] = static_cast<int64_t>(std::get<double>(val));
                        else
                            arr[r] = 0;
                    }
                    (*out_column_data)[c] = arr;
                    break;
                }
                case QUIVER_DATA_TYPE_FLOAT: {
                    auto* arr = new double[row_count];
                    for (size_t r = 0; r < row_count; ++r) {
                        auto& val = rows[r].at(columns[c].first);
                        if (std::holds_alternative<double>(val))
                            arr[r] = std::get<double>(val);
                        else if (std::holds_alternative<int64_t>(val))
                            arr[r] = static_cast<double>(std::get<int64_t>(val));
                        else
                            arr[r] = 0.0;
                    }
                    (*out_column_data)[c] = arr;
                    break;
                }
                case QUIVER_DATA_TYPE_STRING:
                case QUIVER_DATA_TYPE_DATE_TIME: {
                    auto** arr = new char*[row_count];
                    for (size_t r = 0; r < row_count; ++r) {
                        arr[r] = quiver::string::new_c_str(std::get<std::string>(rows[r].at(columns[c].first)));
                    }
                    (*out_column_data)[c] = arr;
                    break;
                }
                default:
                    throw std::runtime_error("Cannot read_time_series_group: unknown data type " +
                                             std::to_string(columns[c].second));
                }
            }
        } catch (...) {
            // Clean up partially allocated results
            quiver_database_free_time_series_data(
                *out_column_names, *out_column_types, *out_column_data, col_count, row_count);
            *out_column_names = nullptr;
            *out_column_types = nullptr;
            *out_column_data = nullptr;
            *out_column_count = 0;
            *out_row_count = 0;
            throw;
        }

        *out_column_count = col_count;
        *out_row_count = row_count;
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_time_series_group(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* group,
                                                                     int64_t id,
                                                                     const char* const* column_names,
                                                                     const int* column_types,
                                                                     const void* const* column_data,
                                                                     size_t column_count,
                                                                     size_t row_count) {
    QUIVER_REQUIRE(db, collection, group);

    // Clear operation: column_count == 0 && row_count == 0
    if (column_count == 0 && row_count == 0) {
        try {
            std::vector<std::map<std::string, quiver::Value>> empty_rows;
            db->db.update_time_series_group(collection, group, id, empty_rows);
            return QUIVER_OK;
        } catch (const std::exception& e) {
            quiver_set_last_error(e.what());
            return QUIVER_ERROR;
        }
    }

    // Validate column_count > 0 when row_count > 0
    if (row_count > 0 && column_count == 0) {
        quiver_set_last_error("Cannot update_time_series_group: column_count must be > 0 when row_count > 0");
        return QUIVER_ERROR;
    }

    // Validate column arrays non-null when column_count > 0
    if (column_count > 0 && (!column_names || !column_types || !column_data)) {
        quiver_set_last_error("Cannot update_time_series_group: column_names, column_types, and column_data must be "
                              "non-null when column_count > 0");
        return QUIVER_ERROR;
    }

    try {
        auto metadata = db->db.get_time_series_metadata(collection, group);

        // Build schema lookup map: column_name -> expected quiver_data_type_t
        std::map<std::string, int> schema_columns;
        schema_columns[metadata.dimension_column] = QUIVER_DATA_TYPE_STRING;
        for (const auto& vc : metadata.value_columns) {
            schema_columns[vc.name] = to_c_data_type(vc.data_type);
        }

        // Validate dimension column present in caller's column_names[]
        bool has_dimension = false;
        for (size_t i = 0; i < column_count; ++i) {
            if (std::string(column_names[i]) == metadata.dimension_column) {
                has_dimension = true;
                break;
            }
        }
        if (!has_dimension) {
            throw std::runtime_error("Cannot update_time_series_group: dimension column '" + metadata.dimension_column +
                                     "' missing from column_names");
        }

        // Validate each column_name exists in schema and type matches
        for (size_t i = 0; i < column_count; ++i) {
            std::string name(column_names[i]);
            auto it = schema_columns.find(name);
            if (it == schema_columns.end()) {
                throw std::runtime_error("Cannot update_time_series_group: column '" + name + "' not found in group '" +
                                         std::string(group) + "' for collection '" + std::string(collection) + "'");
            }
            // Type check: DATE_TIME and STRING are interchangeable
            auto expected = it->second;
            auto actual = column_types[i];
            bool type_ok = (expected == actual) ||
                           (expected == QUIVER_DATA_TYPE_DATE_TIME && actual == QUIVER_DATA_TYPE_STRING) ||
                           (expected == QUIVER_DATA_TYPE_STRING && actual == QUIVER_DATA_TYPE_DATE_TIME);
            if (!type_ok) {
                throw std::runtime_error("Cannot update_time_series_group: column '" + name + "' has type " +
                                         std::string(c_type_name(expected)) + " but received " +
                                         std::string(c_type_name(actual)));
            }
        }

        // Convert columnar to row format
        std::vector<std::map<std::string, quiver::Value>> rows;
        rows.reserve(row_count);

        for (size_t r = 0; r < row_count; ++r) {
            std::map<std::string, quiver::Value> row;
            for (size_t c = 0; c < column_count; ++c) {
                std::string col_name(column_names[c]);
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
                    throw std::runtime_error("Cannot update_time_series_group: unknown data type " +
                                             std::string(c_type_name(column_types[c])));
                }
            }
            rows.push_back(std::move(row));
        }

        db->db.update_time_series_group(collection, group, id, rows);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

// Time series free functions (co-located with read)

QUIVER_C_API quiver_error_t quiver_database_free_time_series_data(char** column_names,
                                                                  int* column_types,
                                                                  void** column_data,
                                                                  size_t column_count,
                                                                  size_t row_count) {
    // Empty result: NULL pointers are valid (nothing to free)
    if (!column_names && !column_data) {
        return QUIVER_OK;
    }

    // Free column names
    if (column_names) {
        for (size_t i = 0; i < column_count; ++i) {
            delete[] column_names[i];
        }
        delete[] column_names;
    }

    // Free column data based on column_types
    if (column_data && column_types) {
        for (size_t i = 0; i < column_count; ++i) {
            if (!column_data[i])
                continue;
            switch (column_types[i]) {
            case QUIVER_DATA_TYPE_INTEGER:
                delete[] static_cast<int64_t*>(column_data[i]);
                break;
            case QUIVER_DATA_TYPE_FLOAT:
                delete[] static_cast<double*>(column_data[i]);
                break;
            case QUIVER_DATA_TYPE_STRING:
            case QUIVER_DATA_TYPE_DATE_TIME: {
                auto** strings = static_cast<char**>(column_data[i]);
                for (size_t j = 0; j < row_count; ++j) {
                    delete[] strings[j];
                }
                delete[] strings;
                break;
            }
            default:
                throw std::runtime_error("Cannot free_time_series_data: unknown data type " +
                                         std::to_string(column_types[i]));
            }
        }
        delete[] column_data;
    }

    // Free types array
    delete[] column_types;

    return QUIVER_OK;
}

// Time series files operations

QUIVER_C_API quiver_error_t quiver_database_has_time_series_files(quiver_database_t* db,
                                                                  const char* collection,
                                                                  int* out_result) {
    QUIVER_REQUIRE(db, collection, out_result);

    try {
        *out_result = db->db.has_time_series_files(collection) ? 1 : 0;
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_list_time_series_files_columns(quiver_database_t* db,
                                                                           const char* collection,
                                                                           char*** out_columns,
                                                                           size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_columns, out_count);

    try {
        auto columns = db->db.list_time_series_files_columns(collection);
        return copy_strings_to_c(columns, out_columns, out_count);
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_read_time_series_files(quiver_database_t* db,
                                                                   const char* collection,
                                                                   char*** out_columns,
                                                                   char*** out_paths,
                                                                   size_t* out_count) {
    QUIVER_REQUIRE(db, collection, out_columns, out_paths, out_count);

    try {
        auto paths_map = db->db.read_time_series_files(collection);
        *out_count = paths_map.size();

        if (paths_map.empty()) {
            *out_columns = nullptr;
            *out_paths = nullptr;
            return QUIVER_OK;
        }

        *out_columns = new char*[paths_map.size()];
        *out_paths = new char*[paths_map.size()];

        size_t i = 0;
        for (const auto& [col_name, path] : paths_map) {
            (*out_columns)[i] = quiver::string::new_c_str(col_name);
            if (path) {
                (*out_paths)[i] = quiver::string::new_c_str(*path);
            } else {
                (*out_paths)[i] = nullptr;
            }
            ++i;
        }

        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_update_time_series_files(quiver_database_t* db,
                                                                     const char* collection,
                                                                     const char* const* columns,
                                                                     const char* const* paths,
                                                                     size_t count) {
    QUIVER_REQUIRE(db, collection);

    if (count > 0 && (!columns || !paths)) {
        quiver_set_last_error("Null columns or paths with non-zero count");
        return QUIVER_ERROR;
    }

    try {
        std::map<std::string, std::optional<std::string>> paths_map;
        for (size_t i = 0; i < count; ++i) {
            if (paths[i]) {
                paths_map[columns[i]] = std::string(paths[i]);
            } else {
                paths_map[columns[i]] = std::nullopt;
            }
        }

        db->db.update_time_series_files(collection, paths_map);
        return QUIVER_OK;
    } catch (const std::exception& e) {
        quiver_set_last_error(e.what());
        return QUIVER_ERROR;
    }
}

QUIVER_C_API quiver_error_t quiver_database_free_time_series_files(char** columns, char** paths, size_t count) {
    QUIVER_REQUIRE(columns, paths);

    for (size_t i = 0; i < count; ++i) {
        delete[] columns[i];
    }
    delete[] columns;

    for (size_t i = 0; i < count; ++i) {
        delete[] paths[i];
    }
    delete[] paths;
    return QUIVER_OK;
}

}  // extern "C"
