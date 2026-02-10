#include "quiver/c/database.h"

#include "database_helpers.h"
#include "internal.h"

#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

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
                                                                   char*** out_date_times,
                                                                   double** out_values,
                                                                   size_t* out_row_count) {
    QUIVER_REQUIRE(db, collection, group, out_date_times, out_values, out_row_count);

    try {
        auto metadata = db->db.get_time_series_metadata(collection, group);
        const auto& dim_col = metadata.dimension_column;
        auto val_col = metadata.value_columns.empty() ? "value" : metadata.value_columns[0].name;

        auto rows = db->db.read_time_series_group(collection, group, id);
        *out_row_count = rows.size();

        if (rows.empty()) {
            *out_date_times = nullptr;
            *out_values = nullptr;
            return QUIVER_OK;
        }

        *out_date_times = new char*[rows.size()];
        *out_values = new double[rows.size()];

        for (size_t i = 0; i < rows.size(); ++i) {
            // Get dimension column
            auto dt_it = rows[i].find(dim_col);
            if (dt_it != rows[i].end() && std::holds_alternative<std::string>(dt_it->second)) {
                (*out_date_times)[i] = strdup_safe(std::get<std::string>(dt_it->second));
            } else {
                (*out_date_times)[i] = strdup_safe("");
            }

            // Get value column
            auto val_it = rows[i].find(val_col);
            if (val_it != rows[i].end()) {
                if (std::holds_alternative<double>(val_it->second)) {
                    (*out_values)[i] = std::get<double>(val_it->second);
                } else if (std::holds_alternative<int64_t>(val_it->second)) {
                    (*out_values)[i] = static_cast<double>(std::get<int64_t>(val_it->second));
                } else {
                    (*out_values)[i] = 0.0;
                }
            } else {
                (*out_values)[i] = 0.0;
            }
        }

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
                                                                     const char* const* date_times,
                                                                     const double* values,
                                                                     size_t row_count) {
    QUIVER_REQUIRE(db, collection, group);

    if (row_count > 0 && (!date_times || !values)) {
        quiver_set_last_error("Null date_times or values with non-zero row_count");
        return QUIVER_ERROR;
    }

    try {
        auto metadata = db->db.get_time_series_metadata(collection, group);
        const auto& dim_col = metadata.dimension_column;
        auto val_col = metadata.value_columns.empty() ? "value" : metadata.value_columns[0].name;

        std::vector<std::map<std::string, quiver::Value>> rows;
        rows.reserve(row_count);

        for (size_t i = 0; i < row_count; ++i) {
            std::map<std::string, quiver::Value> row;
            row[dim_col] = std::string(date_times[i]);
            row[val_col] = values[i];
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

QUIVER_C_API quiver_error_t quiver_free_time_series_data(char** date_times, double* values, size_t row_count) {
    QUIVER_REQUIRE(values, date_times);

    if (date_times) {
        for (size_t i = 0; i < row_count; ++i) {
            delete[] date_times[i];
        }
        delete[] date_times;
    }
    delete[] values;
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
            (*out_columns)[i] = strdup_safe(col_name);
            if (path) {
                (*out_paths)[i] = strdup_safe(*path);
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

QUIVER_C_API quiver_error_t quiver_free_time_series_files(char** columns, char** paths, size_t count) {
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
