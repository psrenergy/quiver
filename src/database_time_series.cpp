#include "database_impl.h"
#include "database_internal.h"

namespace quiver {

namespace {

std::map<std::string, DataType> time_series_schema_types(const TableDefinition& table_def) {
    std::map<std::string, DataType> types;
    for (const auto& [col_name, col] : table_def.columns) {
        if (col_name == "id")
            continue;
        types[col_name] = col.type;
    }
    return types;
}

// Shared validation for update_time_series_group / add_time_series_row: every
// dimension column must be present, all caller columns must exist in the
// schema, and values must match column types.
void validate_time_series_row(const std::string& caller,
                              const std::map<std::string, DataType>& schema_types,
                              const std::vector<std::string>& dim_cols,
                              const std::string& collection,
                              const std::string& group,
                              const std::map<std::string, Value>& row) {
    for (const auto& dim_col : dim_cols) {
        if (row.find(dim_col) == row.end()) {
            throw std::runtime_error("Cannot " + caller + ": row missing required '" + dim_col + "' column");
        }
    }
    for (const auto& [col_name, value] : row) {
        auto it = schema_types.find(col_name);
        if (it == schema_types.end()) {
            throw std::runtime_error("Cannot " + caller + ": column '" + col_name + "' not found in group '" + group +
                                     "' for collection '" + collection + "'");
        }
        if (!internal::value_matches_type(value, it->second)) {
            throw std::runtime_error("Cannot " + caller + ": column '" + col_name + "' has type " +
                                     data_type_to_string(it->second) + " but received " +
                                     internal::value_type_name(value));
        }
    }
}

}  // namespace

std::vector<GroupMetadata> Database::list_time_series_groups(const std::string& collection) const {
    impl_->require_schema("list_time_series_groups");

    std::vector<GroupMetadata> result;
    for (const auto& group_name : impl_->schema->group_names(collection, GroupTableType::TimeSeries)) {
        result.push_back(get_time_series_metadata(collection, group_name));
    }
    return result;
}

GroupMetadata Database::get_time_series_metadata(const std::string& collection, const std::string& group_name) const {
    impl_->require_collection(collection, "get_time_series_metadata");

    // Find the time series table for this group
    auto ts_table = Schema::time_series_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(ts_table);

    if (!table_def) {
        throw std::runtime_error("Time series group not found: '" + group_name + "' in collection '" + collection +
                                 "'");
    }

    GroupMetadata metadata;
    metadata.group_name = group_name;
    metadata.dimension_column = internal::find_dimension_column(*table_def);

    // Add value columns in declaration order (skip id and dimension)
    for (const auto& col_name : table_def->column_order) {
        if (col_name == "id" || col_name == metadata.dimension_column) {
            continue;
        }

        metadata.value_columns.push_back(internal::scalar_metadata_from_column(table_def->columns.at(col_name)));
    }

    return metadata;
}

std::vector<std::map<std::string, Value>>
Database::read_time_series_group(const std::string& collection, const std::string& group, int64_t id) {
    impl_->require_collection(collection, "read_time_series_group");

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
            row[columns[col_idx]] = result[row_idx][col_idx];
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
    impl_->require_collection(collection, "update_time_series_group");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }
    auto dim_cols = internal::find_dimension_columns(*table_def);
    const auto& dim_col = dim_cols.front();

    // Validate rows against schema before any writes
    const auto schema_types = time_series_schema_types(*table_def);
    for (const auto& row : rows) {
        validate_time_series_row("update_time_series_group", schema_types, dim_cols, collection, group, row);
    }

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
        std::vector<Value> parameters;
        parameters.emplace_back(id);
        parameters.emplace_back(row.at(dim_col));

        for (const auto& col : value_columns) {
            auto it = row.find(col);
            if (it != row.end()) {
                parameters.emplace_back(it->second);
            } else {
                parameters.emplace_back(nullptr);
            }
        }

        execute(insert_sql, parameters);
    }

    txn.commit();
    impl_->logger->info("Updated time series {}.{} for id {} with {} rows", collection, group, id, rows.size());
}

void Database::add_time_series_row(const std::string& collection,
                                   const std::string& group,
                                   int64_t id,
                                   const std::map<std::string, Value>& row) {
    impl_->logger->debug("Adding time series row {}.{} for id {} ({} columns)", collection, group, id, row.size());
    impl_->require_collection(collection, "add_time_series_row");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }

    // Every PK column except "id" is a dimension that must be supplied by the caller.
    auto dim_cols = internal::find_dimension_columns(*table_def);

    validate_time_series_row("add_time_series_row", time_series_schema_types(*table_def), dim_cols, collection, group,
                             row);

    Impl::TransactionGuard txn(*impl_);

    // INSERT OR REPLACE: if the PK already matches an existing row, REPLACE
    // deletes it and inserts the new one (upsert semantic). Any value column
    // omitted from the caller's row is not listed in the INSERT, so SQLite
    // leaves it as the column DEFAULT (NULL for nullable value columns).
    std::string insert_sql = "INSERT OR REPLACE INTO " + ts_table + " (id";
    std::string placeholders = "?";
    std::vector<Value> parameters;
    parameters.emplace_back(id);

    for (const auto& [col_name, value] : row) {
        insert_sql += ", " + col_name;
        placeholders += ", ?";
        parameters.emplace_back(value);
    }
    insert_sql += ") VALUES (" + placeholders + ")";

    execute(insert_sql, parameters);

    txn.commit();
    impl_->logger->debug("Added time series row {}.{} for id {}", collection, group, id);
}

std::vector<Value> Database::read_time_series_row(const std::string& collection,
                                                  const std::string& group,
                                                  const std::string& attribute,
                                                  const std::string& date_time) {
    impl_->require_collection(collection, "read_time_series_row");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }
    auto dim_col = internal::find_dimension_column(*table_def);

    const auto* attr_col = table_def->get_column(attribute);
    if (!attr_col || attribute == "id" || attribute == dim_col) {
        throw std::runtime_error("Time series attribute not found: '" + attribute + "' in group '" + group +
                                 "' of collection '" + collection + "'");
    }

    auto element_ids = read_element_ids(collection);
    if (element_ids.empty()) {
        return {};
    }

    // For each element, find the most recent non-null value where dim_col <= date_time.
    // Self-join: subquery picks max dim_col per id, outer query gets the value.
    auto sql = "SELECT t.id, t." + attribute + " FROM " + ts_table + " t INNER JOIN (SELECT id, MAX(" + dim_col +
               ") as max_dt FROM " + ts_table + " WHERE " + dim_col + " <= ? AND " + attribute + " IS NOT NULL " +
               "GROUP BY id) latest ON t.id = latest.id AND t." + dim_col + " = latest.max_dt ORDER BY t.id";

    auto query_result = execute(sql, {date_time});

    std::map<int64_t, Value> id_value_map;
    for (size_t i = 0; i < query_result.row_count(); ++i) {
        auto id = query_result[i].get_integer(0);
        if (!id)
            continue;
        id_value_map[*id] = query_result[i][1];
    }

    std::vector<Value> result;
    result.reserve(element_ids.size());
    for (auto element_id : element_ids) {
        auto it = id_value_map.find(element_id);
        result.push_back(it != id_value_map.end() ? it->second : Value{nullptr});
    }

    return result;
}

bool Database::has_time_series_files(const std::string& collection) const {
    impl_->require_collection(collection, "has_time_series_files");
    auto tsf = Schema::time_series_files_table_name(collection);
    return impl_->schema->has_table(tsf);
}

std::vector<std::string> Database::list_time_series_files_columns(const std::string& collection) const {
    impl_->require_collection(collection, "list_time_series_files_columns");
    auto tsf = Schema::time_series_files_table_name(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found: " + tsf);
    }

    std::vector<std::string> columns;
    columns.reserve(table_def->columns.size());
    for (const auto& [col_name, _] : table_def->columns) {
        columns.push_back(col_name);
    }
    return columns;
}

std::map<std::string, std::optional<std::string>> Database::read_time_series_files(const std::string& collection) {
    impl_->logger->debug("Reading time series files for collection: {}", collection);
    impl_->require_collection(collection, "read_time_series_files");

    auto tsf = impl_->schema->find_time_series_files_table(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found: " + tsf);
    }

    // Build column list
    std::vector<std::string> columns;
    columns.reserve(table_def->columns.size());
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
    impl_->require_collection(collection, "update_time_series_files");

    auto tsf = impl_->schema->find_time_series_files_table(collection);
    const auto* table_def = impl_->schema->get_table(tsf);
    if (!table_def) {
        throw std::runtime_error("Time series files table not found: " + tsf);
    }

    if (paths.empty()) {
        return;
    }

    // Validate caller-provided column names
    for (const auto& [col_name, path] : paths) {
        if (!table_def->has_column(col_name)) {
            throw std::runtime_error("Cannot update_time_series_files: column '" + col_name + "' not found in table '" +
                                     tsf + "'");
        }
    }

    Impl::TransactionGuard txn(*impl_);

    // Delete existing row (singleton table)
    auto delete_sql = "DELETE FROM " + tsf;
    execute(delete_sql);

    // Build INSERT SQL
    std::string insert_sql = "INSERT INTO " + tsf + " (";
    std::string placeholders;
    std::vector<Value> parameters;

    bool first = true;
    for (const auto& [col_name, path] : paths) {
        if (!first) {
            insert_sql += ", ";
            placeholders += ", ";
        }
        insert_sql += col_name;
        placeholders += "?";
        if (path) {
            parameters.emplace_back(*path);
        } else {
            parameters.emplace_back(nullptr);
        }
        first = false;
    }
    insert_sql += ") VALUES (" + placeholders + ")";

    execute(insert_sql, parameters);

    txn.commit();
    impl_->logger->info("Updated time series files for collection: {}", collection);
}

}  // namespace quiver
