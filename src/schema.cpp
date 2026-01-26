#include "quiver/schema.h"

#include <sqlite3.h>
#include <stdexcept>

namespace quiver {

// TableDefinition methods

std::optional<DataType> TableDefinition::get_data_type(const std::string& column) const {
    auto it = columns.find(column);
    if (it != columns.end()) {
        return it->second.type;
    }
    return std::nullopt;
}

bool TableDefinition::has_column(const std::string& column) const {
    return columns.find(column) != columns.end();
}

const ColumnDefinition* TableDefinition::get_column(const std::string& column) const {
    auto it = columns.find(column);
    if (it != columns.end()) {
        return &it->second;
    }
    return nullptr;
}

// Schema factory

Schema Schema::from_database(sqlite3* db) {
    Schema schema;
    schema.load_from_database(db);
    return schema;
}

// Schema public methods

const TableDefinition* Schema::get_table(const std::string& name) const {
    auto it = tables_.find(name);
    if (it != tables_.end()) {
        return &it->second;
    }
    return nullptr;
}

bool Schema::has_table(const std::string& name) const {
    return tables_.find(name) != tables_.end();
}

DataType Schema::get_data_type(const std::string& table, const std::string& column) const {
    const auto* tbl = get_table(table);
    if (!tbl) {
        throw std::runtime_error("Table not found in schema: " + table);
    }
    auto type = tbl->get_data_type(column);
    if (!type) {
        throw std::runtime_error("Column '" + column + "' not found in table '" + table + "'");
    }
    return *type;
}

std::string Schema::vector_table_name(const std::string& collection, const std::string& group) {
    return collection + "_vector_" + group;
}

std::string Schema::set_table_name(const std::string& collection, const std::string& group) {
    return collection + "_set_" + group;
}

bool Schema::is_collection(const std::string& table) const {
    if (table == "Configuration") {
        return true;
    }
    return table.find('_') == std::string::npos;
}

bool Schema::is_vector_table(const std::string& table) const {
    return table.find("_vector_") != std::string::npos;
}

bool Schema::is_set_table(const std::string& table) const {
    return table.find("_set_") != std::string::npos;
}

bool Schema::is_time_series_table(const std::string& table) const {
    return table.find("_time_series_") != std::string::npos;
}

std::string Schema::get_parent_collection(const std::string& table) const {
    auto pos = table.find('_');
    if (pos != std::string::npos) {
        return table.substr(0, pos);
    }
    return "";
}

std::string Schema::find_vector_table(const std::string& collection, const std::string& attribute) const {
    // First try: Collection_vector_attribute
    auto vt = vector_table_name(collection, attribute);
    if (has_table(vt)) {
        return vt;
    }

    // Second try: search all vector tables for the collection
    for (const auto& table_name : table_names()) {
        if (!is_vector_table(table_name))
            continue;
        if (get_parent_collection(table_name) != collection)
            continue;

        const auto* table_def = get_table(table_name);
        if (table_def && table_def->has_column(attribute)) {
            return table_name;
        }
    }

    throw std::runtime_error("Vector attribute '" + attribute + "' not found for collection '" + collection + "'");
}

std::string Schema::find_set_table(const std::string& collection, const std::string& attribute) const {
    // First try: Collection_set_attribute
    auto st = set_table_name(collection, attribute);
    if (has_table(st)) {
        return st;
    }

    // Second try: search all set tables for the collection
    for (const auto& table_name : table_names()) {
        if (!is_set_table(table_name))
            continue;
        if (get_parent_collection(table_name) != collection)
            continue;

        const auto* table_def = get_table(table_name);
        if (table_def && table_def->has_column(attribute)) {
            return table_name;
        }
    }

    throw std::runtime_error("Set attribute '" + attribute + "' not found for collection '" + collection + "'");
}

std::vector<std::string> Schema::table_names() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& [name, _] : tables_) {
        names.push_back(name);
    }
    return names;
}

std::vector<std::string> Schema::collection_names() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : tables_) {
        if (is_collection(name)) {
            names.push_back(name);
        }
    }
    return names;
}

// Schema private methods

void Schema::load_from_database(sqlite3* db) {
    // Get all table names
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to query table names");
    }

    std::vector<std::string> names;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) {
            names.emplace_back(name);
        }
    }
    sqlite3_finalize(stmt);

    // Load each table's metadata
    for (const auto& name : names) {
        TableDefinition table;
        table.name = name;

        auto columns = query_columns(db, name);
        for (auto& col : columns) {
            table.columns[col.name] = std::move(col);
        }

        table.foreign_keys = query_foreign_keys(db, name);
        table.indexes = query_indexes(db, name);

        tables_[name] = std::move(table);
    }
}

std::vector<ColumnDefinition> Schema::query_columns(sqlite3* db, const std::string& table) {
    std::vector<ColumnDefinition> columns;
    std::string sql = "PRAGMA table_info(" + table + ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to query columns for table: " + table);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ColumnDefinition col;
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* dflt_value = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));

        col.name = name ? name : "";
        std::string type_str = type ? type : "";
        col.type = data_type_from_string(type_str);
        col.not_null = sqlite3_column_int(stmt, 3) != 0;
        col.primary_key = sqlite3_column_int(stmt, 5) != 0;
        if (dflt_value) {
            col.default_value = std::string(dflt_value);
        }

        // Infer DATE_TIME type from column name for TEXT columns
        if (col.type == DataType::Text && is_datetime_column(col.name)) {
            col.type = DataType::DateTime;
        }

        columns.push_back(std::move(col));
    }
    sqlite3_finalize(stmt);
    return columns;
}

std::vector<ForeignKey> Schema::query_foreign_keys(sqlite3* db, const std::string& table) {
    std::vector<ForeignKey> fks;
    std::string sql = "PRAGMA foreign_key_list(" + table + ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to query foreign keys for table: " + table);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ForeignKey fk;
        const char* to_table = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        const char* from_col = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        const char* to_col = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
        const char* on_update = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        const char* on_delete = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));

        fk.to_table = to_table ? to_table : "";
        fk.from_column = from_col ? from_col : "";
        fk.to_column = to_col ? to_col : "";
        fk.on_update = on_update ? on_update : "";
        fk.on_delete = on_delete ? on_delete : "";

        fks.push_back(std::move(fk));
    }
    sqlite3_finalize(stmt);
    return fks;
}

std::vector<Index> Schema::query_indexes(sqlite3* db, const std::string& table) {
    std::vector<Index> indexes;
    std::string sql = "PRAGMA index_list(" + table + ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return indexes;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Index idx;
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        idx.name = name ? name : "";
        idx.unique = sqlite3_column_int(stmt, 2) != 0;

        // Get columns for this index
        std::string idx_sql = "PRAGMA index_info(" + idx.name + ")";
        sqlite3_stmt* idx_stmt = nullptr;
        if (sqlite3_prepare_v2(db, idx_sql.c_str(), -1, &idx_stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(idx_stmt) == SQLITE_ROW) {
                const char* col_name = reinterpret_cast<const char*>(sqlite3_column_text(idx_stmt, 2));
                if (col_name) {
                    idx.columns.emplace_back(col_name);
                }
            }
            sqlite3_finalize(idx_stmt);
        }
        indexes.push_back(std::move(idx));
    }
    sqlite3_finalize(stmt);
    return indexes;
}

}  // namespace quiver
