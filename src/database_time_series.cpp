#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

std::vector<GroupMetadata> Database::list_time_series_groups(const std::string& collection) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list time series groups: no schema loaded");
    }

    std::vector<GroupMetadata> result;
    auto prefix = collection + "_time_series_";

    for (const auto& table_name : impl_->schema->table_names()) {
        if (!impl_->schema->is_time_series_table(table_name))
            continue;
        if (impl_->schema->get_parent_collection(table_name) != collection)
            continue;

        // Extract group name from table name
        if (table_name.size() > prefix.size() && table_name.substr(0, prefix.size()) == prefix) {
            auto group_name = table_name.substr(prefix.size());
            result.push_back(get_time_series_metadata(collection, group_name));
        }
    }
    return result;
}

GroupMetadata Database::get_time_series_metadata(const std::string& collection, const std::string& group_name) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get time series metadata: no schema loaded");
    }

    // Find the time series table for this group
    auto ts_table = Schema::time_series_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(ts_table);

    if (!table_def) {
        throw std::runtime_error("Time series group '" + group_name + "' not found for collection '" + collection +
                                 "'");
    }

    GroupMetadata metadata;
    metadata.group_name = group_name;
    metadata.dimension_column = internal::find_dimension_column(*table_def);

    // Add value columns (skip id and dimension)
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id" || col_name == metadata.dimension_column) {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_from_column(col));
    }

    return metadata;
}

std::vector<std::map<std::string, Value>>
Database::read_time_series_group(const std::string& collection, const std::string& group, int64_t id) {
    impl_->require_schema("read time series");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }
    auto dim_col = internal::find_dimension_column(*table_def);

    // Build column list (excluding id)
    std::vector<std::string> columns;
    columns.push_back(dim_col);
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name != "id" && col_name != dim_col) {
            columns.push_back(col_name);
        }
    }

    // Build SELECT query
    std::string sql = "SELECT ";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += columns[i];
    }
    sql += " FROM " + ts_table + " WHERE id = ? ORDER BY " + dim_col;

    auto result = execute(sql, {id});

    std::vector<std::map<std::string, Value>> rows;
    rows.reserve(result.row_count());

    for (size_t row_idx = 0; row_idx < result.row_count(); ++row_idx) {
        std::map<std::string, Value> row;
        for (size_t col_idx = 0; col_idx < columns.size(); ++col_idx) {
            const auto& col_name = columns[col_idx];
            // Get the value from the result row
            auto int_val = result[row_idx].get_integer(col_idx);
            auto float_val = result[row_idx].get_float(col_idx);
            auto str_val = result[row_idx].get_string(col_idx);

            if (int_val) {
                row[col_name] = *int_val;
            } else if (float_val) {
                row[col_name] = *float_val;
            } else if (str_val) {
                row[col_name] = *str_val;
            } else {
                row[col_name] = nullptr;
            }
        }
        rows.push_back(std::move(row));
    }

    return rows;
}

void Database::update_time_series_group(const std::string& collection,
                                        const std::string& group,
                                        int64_t id,
                                        const std::vector<std::map<std::string, Value>>& rows) {
    impl_->logger->debug("Updating time series {}.{} for id {} with {} rows", collection, group, id, rows.size());
    impl_->require_schema("update time series");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }
    auto dim_col = internal::find_dimension_column(*table_def);

    Impl::TransactionGuard txn(*impl_);

    // Delete existing time series data for this element
    auto delete_sql = "DELETE FROM " + ts_table + " WHERE id = ?";
    execute(delete_sql, {id});

    if (rows.empty()) {
        txn.commit();
        return;
    }

    // Get column names from first row (excluding dimension column which we handle specially)
    std::vector<std::string> value_columns;
    for (const auto& [col_name, _] : rows[0]) {
        if (col_name != dim_col) {
            value_columns.push_back(col_name);
        }
    }

    // Build INSERT SQL
    auto insert_sql = "INSERT INTO " + ts_table + " (id, " + dim_col;
    for (const auto& col : value_columns) {
        insert_sql += ", " + col;
    }
    insert_sql += ") VALUES (?";
    for (size_t i = 0; i <= value_columns.size(); ++i) {
        insert_sql += ", ?";
    }
    insert_sql += ")";

    // Insert each row
    for (const auto& row : rows) {
        std::vector<Value> params;
        params.push_back(id);

        // Dimension column must be present
        auto dt_it = row.find(dim_col);
        if (dt_it == row.end()) {
            throw std::runtime_error("Time series row missing required '" + dim_col + "' column");
        }
        params.push_back(dt_it->second);

        // Add value columns
        for (const auto& col : value_columns) {
            auto it = row.find(col);
            if (it != row.end()) {
                params.push_back(it->second);
            } else {
                params.push_back(nullptr);
            }
        }

        execute(insert_sql, params);
    }

    txn.commit();
    impl_->logger->info("Updated time series {}.{} for id {} with {} rows", collection, group, id, rows.size());
}

bool Database::has_time_series_files(const std::string& collection) const {
    impl_->require_schema("check time series files");
    auto tsf = Schema::time_series_files_table_name(collection);
    return impl_->schema->has_table(tsf);
}

std::vector<std::string> Database::list_time_series_files_columns(const std::string& collection) const {
    impl_->require_schema("list time series files columns");
    auto tsf = Schema::time_series_files_table_name(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found for collection '" + collection + "'");
    }

    std::vector<std::string> columns;
    for (const auto& [col_name, _] : table_def->columns) {
        columns.push_back(col_name);
    }
    return columns;
}

std::map<std::string, std::optional<std::string>> Database::read_time_series_files(const std::string& collection) {
    impl_->logger->debug("Reading time series files for collection: {}", collection);
    impl_->require_schema("read time series files");

    auto tsf = impl_->schema->find_time_series_files_table(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found: " + tsf);
    }

    // Build column list
    std::vector<std::string> columns;
    for (const auto& [col_name, _] : table_def->columns) {
        columns.push_back(col_name);
    }

    if (columns.empty()) {
        return {};
    }

    // Build SELECT query
    std::string sql = "SELECT ";
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0)
            sql += ", ";
        sql += columns[i];
    }
    sql += " FROM " + tsf + " LIMIT 1";

    auto result = execute(sql);

    std::map<std::string, std::optional<std::string>> paths;
    if (result.empty()) {
        // No row yet - return all nulls
        for (const auto& col : columns) {
            paths[col] = std::nullopt;
        }
    } else {
        for (size_t i = 0; i < columns.size(); ++i) {
            auto str_val = result[0].get_string(i);
            paths[columns[i]] = str_val;
        }
    }

    return paths;
}

void Database::update_time_series_files(const std::string& collection,
                                        const std::map<std::string, std::optional<std::string>>& paths) {
    impl_->logger->debug("Updating time series files for collection: {}", collection);
    impl_->require_schema("update time series files");

    auto tsf = impl_->schema->find_time_series_files_table(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found: " + tsf);
    }

    if (paths.empty()) {
        return;
    }

    Impl::TransactionGuard txn(*impl_);

    // Delete existing row (singleton table)
    auto delete_sql = "DELETE FROM " + tsf;
    execute(delete_sql);

    // Build INSERT SQL
    std::string insert_sql = "INSERT INTO " + tsf + " (";
    std::string placeholders;
    std::vector<Value> params;

    bool first = true;
    for (const auto& [col_name, path] : paths) {
        if (!first) {
            insert_sql += ", ";
            placeholders += ", ";
        }
        insert_sql += col_name;
        placeholders += "?";
        if (path) {
            params.push_back(*path);
        } else {
            params.push_back(nullptr);
        }
        first = false;
    }
    insert_sql += ") VALUES (" + placeholders + ")";

    execute(insert_sql, params);

    txn.commit();
    impl_->logger->info("Updated time series files for collection: {}", collection);
}

}  // namespace quiver
