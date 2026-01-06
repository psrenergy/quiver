#include "psr/schema_validator.h"

#include <algorithm>
#include <set>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace psr {

SchemaValidator::SchemaValidator(sqlite3* db) : db_(db) {}

void SchemaValidator::validation_error(const std::string& message) {
    throw std::runtime_error("Schema validation error: " + message);
}

std::vector<std::string> SchemaValidator::get_table_names() {
    std::vector<std::string> tables;
    const char* sql = "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        validation_error("Failed to query table names");
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (name) {
            tables.emplace_back(name);
        }
    }
    sqlite3_finalize(stmt);
    return tables;
}

std::vector<ColumnInfo> SchemaValidator::get_columns(const std::string& table) {
    std::vector<ColumnInfo> columns;
    std::string sql = "PRAGMA table_info(" + table + ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        validation_error("Failed to query columns for table: " + table);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ColumnInfo col;
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        col.name = name ? name : "";
        col.type = type ? type : "";
        col.not_null = sqlite3_column_int(stmt, 3) != 0;
        col.primary_key = sqlite3_column_int(stmt, 5) != 0;
        columns.push_back(col);
    }
    sqlite3_finalize(stmt);
    return columns;
}

std::vector<ForeignKeyInfo> SchemaValidator::get_foreign_keys(const std::string& table) {
    std::vector<ForeignKeyInfo> fks;
    std::string sql = "PRAGMA foreign_key_list(" + table + ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        validation_error("Failed to query foreign keys for table: " + table);
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ForeignKeyInfo fk;
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
        fks.push_back(fk);
    }
    sqlite3_finalize(stmt);
    return fks;
}

std::vector<IndexInfo> SchemaValidator::get_indexes(const std::string& table) {
    std::vector<IndexInfo> indexes;
    std::string sql = "PRAGMA index_list(" + table + ")";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return indexes;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        IndexInfo idx;
        const char* name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        idx.name = name ? name : "";
        idx.unique = sqlite3_column_int(stmt, 2) != 0;

        // Get columns for this index
        std::string idx_sql = "PRAGMA index_info(" + idx.name + ")";
        sqlite3_stmt* idx_stmt = nullptr;
        if (sqlite3_prepare_v2(db_, idx_sql.c_str(), -1, &idx_stmt, nullptr) == SQLITE_OK) {
            while (sqlite3_step(idx_stmt) == SQLITE_ROW) {
                const char* col_name = reinterpret_cast<const char*>(sqlite3_column_text(idx_stmt, 2));
                if (col_name) {
                    idx.columns.emplace_back(col_name);
                }
            }
            sqlite3_finalize(idx_stmt);
        }
        indexes.push_back(idx);
    }
    sqlite3_finalize(stmt);
    return indexes;
}

bool SchemaValidator::is_collection(const std::string& name) const {
    // Collections don't have underscores (those are reserved for special tables)
    // Exception: Configuration is a special collection
    if (name == "Configuration") {
        return true;
    }
    return name.find('_') == std::string::npos;
}

bool SchemaValidator::is_vector_table(const std::string& name) const {
    return name.find("_vector_") != std::string::npos;
}

bool SchemaValidator::is_set_table(const std::string& name) const {
    return name.find("_set_") != std::string::npos;
}

bool SchemaValidator::is_time_series_table(const std::string& name) const {
    return name.find("_time_series_") != std::string::npos;
}

std::string SchemaValidator::get_parent_collection(const std::string& table) const {
    auto pos = table.find('_');
    if (pos != std::string::npos) {
        return table.substr(0, pos);
    }
    return "";
}

void SchemaValidator::validate() {
    // Get all tables
    tables_ = get_table_names();

    // Identify collections
    collections_.clear();
    for (const auto& table : tables_) {
        if (is_collection(table)) {
            collections_.push_back(table);
        }
    }

    // Run all validations
    validate_configuration_exists();
    validate_collection_names();

    for (const auto& table : tables_) {
        if (is_collection(table)) {
            validate_collection(table);
        } else if (is_vector_table(table)) {
            validate_vector_table(table);
        } else if (is_set_table(table)) {
            validate_set_table(table);
        }
        // Time series tables have minimal validation (just file paths)
    }

    validate_no_duplicate_attributes();
    validate_foreign_keys();
}

void SchemaValidator::validate_configuration_exists() {
    auto it = std::find(tables_.begin(), tables_.end(), "Configuration");
    if (it == tables_.end()) {
        validation_error("Schema must have a 'Configuration' table");
    }
}

void SchemaValidator::validate_collection_names() {
    for (const auto& table : tables_) {
        // Skip special tables
        if (is_vector_table(table) || is_set_table(table) || is_time_series_table(table)) {
            continue;
        }

        // Check for invalid collection names (underscores not allowed except for special tables)
        if (table.find('_') != std::string::npos && table != "Configuration") {
            validation_error("Invalid collection name '" + table + "': collection names cannot contain underscores");
        }
    }
}

void SchemaValidator::validate_collection(const std::string& name) {
    auto columns = get_columns(name);

    // Must have 'id' column as primary key
    bool has_id_pk = false;
    for (const auto& col : columns) {
        if (col.name == "id" && col.primary_key) {
            has_id_pk = true;
            break;
        }
    }
    if (!has_id_pk) {
        validation_error("Collection '" + name + "' must have 'id' as primary key");
    }

    // Must have 'label' column with UNIQUE NOT NULL constraints
    bool has_label = false;
    bool label_not_null = false;
    for (const auto& col : columns) {
        if (col.name == "label") {
            has_label = true;
            label_not_null = col.not_null;
            break;
        }
    }

    if (!has_label) {
        validation_error("Collection '" + name + "' must have a 'label' column");
    }
    if (!label_not_null) {
        validation_error("Collection '" + name + "' label column must have NOT NULL constraint");
    }

    // Check for UNIQUE constraint on label
    auto indexes = get_indexes(name);
    bool label_unique = false;
    for (const auto& idx : indexes) {
        if (idx.unique && idx.columns.size() == 1 && idx.columns[0] == "label") {
            label_unique = true;
            break;
        }
    }
    if (!label_unique) {
        validation_error("Collection '" + name + "' label column must have UNIQUE constraint");
    }
}

void SchemaValidator::validate_vector_table(const std::string& name) {
    auto columns = get_columns(name);
    auto fks = get_foreign_keys(name);

    // Get parent collection
    std::string parent = get_parent_collection(name);
    if (std::find(collections_.begin(), collections_.end(), parent) == collections_.end()) {
        validation_error("Vector table '" + name + "' references non-existent collection '" + parent + "'");
    }

    // Must have 'id' column (FK to parent)
    bool has_id = false;
    for (const auto& col : columns) {
        if (col.name == "id") {
            has_id = true;
            // id should NOT be primary key alone
            if (col.primary_key) {
                // Check if it's a composite PK by counting PK columns
                int pk_count = 0;
                for (const auto& c : columns) {
                    if (c.primary_key) {
                        pk_count++;
                    }
                }
                if (pk_count == 1) {
                    validation_error("Vector table '" + name +
                                     "' must have composite primary key (id, vector_index), not just 'id'");
                }
            }
            break;
        }
    }
    if (!has_id) {
        validation_error("Vector table '" + name + "' must have 'id' column");
    }

    // Must have 'vector_index' column
    bool has_vector_index = false;
    for (const auto& col : columns) {
        if (col.name == "vector_index") {
            has_vector_index = true;
            break;
        }
    }
    if (!has_vector_index) {
        validation_error("Vector table '" + name + "' must have 'vector_index' column");
    }

    // Must have FK to parent with ON DELETE CASCADE ON UPDATE CASCADE
    bool has_parent_fk = false;
    for (const auto& fk : fks) {
        if (fk.from_column == "id" && fk.to_table == parent) {
            has_parent_fk = true;
            if (fk.on_delete != "CASCADE" || fk.on_update != "CASCADE") {
                validation_error("Vector table '" + name +
                                 "' FK to parent must use ON DELETE CASCADE ON UPDATE CASCADE");
            }
            break;
        }
    }
    if (!has_parent_fk) {
        validation_error("Vector table '" + name + "' must have foreign key to parent collection '" + parent + "'");
    }
}

void SchemaValidator::validate_set_table(const std::string& name) {
    auto columns = get_columns(name);
    auto indexes = get_indexes(name);
    auto fks = get_foreign_keys(name);

    // Get FK column names
    std::set<std::string> fk_columns;
    for (const auto& fk : fks) {
        fk_columns.insert(fk.from_column);
    }

    // Must have at least one UNIQUE constraint
    bool has_unique = false;
    for (const auto& idx : indexes) {
        if (idx.unique) {
            has_unique = true;

            // All non-FK columns should be part of some unique constraint
            // (simplified check: at least check that unique constraint exists)
            break;
        }
    }
    if (!has_unique) {
        validation_error("Set table '" + name + "' must have a UNIQUE constraint");
    }

    // Check that all non-id, non-FK columns are covered by unique constraints
    std::set<std::string> unique_columns;
    for (const auto& idx : indexes) {
        if (idx.unique) {
            for (const auto& col : idx.columns) {
                unique_columns.insert(col);
            }
        }
    }

    for (const auto& col : columns) {
        // Skip id and FK columns
        if (col.name == "id" || fk_columns.count(col.name) > 0) {
            continue;
        }
        // Non-FK columns should be in unique constraint
        if (unique_columns.find(col.name) == unique_columns.end()) {
            validation_error("Set table '" + name + "' column '" + col.name + "' must be part of a UNIQUE constraint");
        }
    }
}

void SchemaValidator::validate_no_duplicate_attributes() {
    // For each collection, gather all attribute names from collection + vector tables
    for (const auto& collection : collections_) {
        if (collection == "Configuration") {
            continue;
        }

        std::set<std::string> attributes;
        auto columns = get_columns(collection);

        // Add scalar attributes (excluding id and label)
        for (const auto& col : columns) {
            if (col.name != "id" && col.name != "label") {
                attributes.insert(col.name);
            }
        }

        // Check vector tables for duplicates
        for (const auto& table : tables_) {
            if (!is_vector_table(table)) {
                continue;
            }
            if (get_parent_collection(table) != collection) {
                continue;
            }

            auto vec_columns = get_columns(table);
            for (const auto& col : vec_columns) {
                // Skip id and vector_index
                if (col.name == "id" || col.name == "vector_index") {
                    continue;
                }
                // Check for FK columns (they end with _id typically)
                auto fks = get_foreign_keys(table);
                bool is_fk = false;
                for (const auto& fk : fks) {
                    if (fk.from_column == col.name) {
                        is_fk = true;
                        break;
                    }
                }
                if (is_fk) {
                    continue;
                }

                // Check if attribute already exists in collection
                if (attributes.count(col.name) > 0) {
                    validation_error("Duplicate attribute '" + col.name + "' in collection '" + collection +
                                     "' and vector table '" + table + "'");
                }
            }
        }
    }
}

void SchemaValidator::validate_foreign_keys() {
    for (const auto& table : tables_) {
        auto columns = get_columns(table);
        auto fks = get_foreign_keys(table);

        for (const auto& fk : fks) {
            // Find the column for this FK
            const ColumnInfo* col = nullptr;
            for (const auto& c : columns) {
                if (c.name == fk.from_column) {
                    col = &c;
                    break;
                }
            }
            if (!col) {
                continue;
            }

            // Rule: FK columns with ON DELETE SET NULL cannot have NOT NULL constraint
            if (fk.on_delete == "SET NULL" && col->not_null) {
                validation_error("Foreign key column '" + fk.from_column + "' in table '" + table +
                                 "' has ON DELETE SET NULL but NOT NULL constraint");
            }

            // Rule: Valid FK action combinations
            // For vector table parent FK: ON DELETE CASCADE ON UPDATE CASCADE
            // For relation FKs: ON DELETE SET NULL ON UPDATE CASCADE
            if (is_vector_table(table) && fk.from_column == "id") {
                // Parent FK must be CASCADE/CASCADE
                if (fk.on_delete != "CASCADE" || fk.on_update != "CASCADE") {
                    validation_error("Vector table '" + table +
                                     "' parent FK must use ON DELETE CASCADE ON UPDATE CASCADE");
                }
            } else if (!is_time_series_table(table)) {
                // Relation FK - check for valid combinations
                // Must be ON UPDATE CASCADE
                if (fk.on_update != "CASCADE") {
                    validation_error("Foreign key '" + fk.from_column + "' in table '" + table +
                                     "' must use ON UPDATE CASCADE");
                }
                // ON DELETE must be SET NULL or CASCADE
                if (fk.on_delete != "SET NULL" && fk.on_delete != "CASCADE") {
                    validation_error("Foreign key '" + fk.from_column + "' in table '" + table +
                                     "' must use ON DELETE SET NULL or ON DELETE CASCADE");
                }
            }

            // Rule: FK column names should follow pattern <collection>_id or <collection>_<relation>
            // Skip vector table id column and set table columns
            if (!is_vector_table(table) || fk.from_column != "id") {
                if (!is_set_table(table)) {
                    // Check if FK column name ends with _id or matches target_relation pattern
                    std::string target = fk.to_table;
                    // Convert to lowercase for comparison
                    std::string target_lower = target;
                    std::transform(target_lower.begin(), target_lower.end(), target_lower.begin(), ::tolower);
                    std::string col_lower = fk.from_column;
                    std::transform(col_lower.begin(), col_lower.end(), col_lower.begin(), ::tolower);

                    // Expected pattern: <target>_id or <target>_<something>
                    bool valid_name = (col_lower == target_lower + "_id") ||
                                      (col_lower.find(target_lower + "_") == 0) ||
                                      (col_lower.find("_id") != std::string::npos);

                    if (!valid_name) {
                        validation_error("Foreign key column '" + fk.from_column + "' in table '" + table +
                                         "' should follow naming pattern '<collection>_id'");
                    }
                }
            }
        }
    }
}

}  // namespace psr
