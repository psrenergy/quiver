#ifndef QUIVER_DATABASE_IMPL_H
#define QUIVER_DATABASE_IMPL_H

#include "quiver/database.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"

#include <map>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace quiver {

using StmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

// Run a read-only query that yields integer columns and collect the rows. Prepares/steps
// directly on the raw sqlite3* rather than through Database::execute(), which is non-const and
// unusable from const methods (number_of_elements, current_version, describe/summarize_collection).
// Only integer parameters are needed (LIMIT bounds, ids), and every column read is an int64.
inline std::vector<std::vector<int64_t>>
query_int_rows(sqlite3* db, const std::string& sql, const std::vector<int64_t>& parameters = {}) {
    sqlite3_stmt* raw_stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &raw_stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(db)));
    }
    StmtPtr stmt(raw_stmt, sqlite3_finalize);

    for (size_t i = 0; i < parameters.size(); ++i) {
        sqlite3_bind_int64(stmt.get(), static_cast<int>(i + 1), parameters[i]);
    }

    const int col_count = sqlite3_column_count(stmt.get());
    std::vector<std::vector<int64_t>> rows;
    int rc = 0;
    while ((rc = sqlite3_step(stmt.get())) == SQLITE_ROW) {
        std::vector<int64_t> row;
        row.reserve(col_count);
        for (int c = 0; c < col_count; ++c) {
            row.push_back(sqlite3_column_int64(stmt.get(), c));
        }
        rows.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(db)));
    }
    return rows;
}

struct ResolvedElement {
    std::map<std::string, Value> scalars;
    std::map<std::string, std::vector<Value>> arrays;
};

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    // Loaded lazily by require_schema: the Database(path, options) constructor opens an existing
    // database without reading its schema, and every metadata/CRUD path goes through
    // require_schema. mutable so the const readers (get_*_metadata, describe, ...) can trigger it.
    mutable std::unique_ptr<Schema> schema;
    mutable std::unique_ptr<TypeValidator> type_validator;
    // A dry run holds one real transaction open and absorbs the public begin/commit/rollback so
    // nested callers compose. TransactionGuard needs no flag - it already no-ops when a
    // transaction is active.
    bool dry_run = false;

    // Takes no operation name: reading an existing database's schema on first use is what makes
    // open() usable, and a database that is not a quiver database throws the validator's own
    // (already self-describing) reason from here rather than "Cannot <op>: no schema loaded".
    void require_schema() const {
        if (!schema) {
            load_schema_metadata();
        }
    }

    void require_collection(const std::string& collection, const char* operation) const {
        require_schema();
        if (!schema->has_table(collection)) {
            throw std::runtime_error(std::string("Cannot ") + operation + ": collection not found: " + collection);
        }
    }

    // A missing id is Pattern 2 everywhere (root design decision), so every id-scoped write
    // resolves it through here rather than letting SQLite report a foreign-key failure.
    void require_element(const std::string& collection, int64_t id, Database& db) const {
        if (db.execute("SELECT 1 FROM " + collection + " WHERE id = ?", {id}).empty()) {
            throw std::runtime_error("Element not found: " + std::to_string(id) + " in collection '" + collection +
                                     "'");
        }
    }

    // The one label -> id lookup; callers own the throw (Pattern 2 vs Pattern 3).
    static std::optional<int64_t> lookup_id_by_label(const std::string& table, const std::string& label, Database& db) {
        auto result = db.execute("SELECT id FROM " + table + " WHERE label = ?", {label});
        if (result.empty()) {
            return std::nullopt;
        }
        return result[0].get_integer(0);
    }

    int64_t
    resolve_label(const std::string& collection, const std::string& label, const char* operation, Database& db) const {
        require_collection(collection, operation);
        // Any table is accepted by require_collection (it only checks has_table), so a group table
        // would otherwise reach the SELECT and leak a raw "no such column: label" prepare error.
        require_column(collection, "label", operation);

        auto id = lookup_id_by_label(collection, label, db);
        if (!id) {
            throw std::runtime_error("Element not found: label '" + label + "' in collection '" + collection + "'");
        }
        return *id;
    }

    void require_column(const std::string& table, const std::string& column, const char* operation) const {
        require_schema();
        const auto* table_def = schema->get_table(table);
        if (!table_def) {
            throw std::runtime_error(std::string("Cannot ") + operation + ": table not found: " + table);
        }
        if (!table_def->has_column(column)) {
            throw std::runtime_error(std::string("Cannot ") + operation + ": column '" + column +
                                     "' not found in table '" + table + "'");
        }
    }

    Value
    resolve_fk_label(const TableDefinition& table_def, const std::string& column, const Value& value, Database& db) {
        if (!std::holds_alternative<std::string>(value)) {
            return value;
        }

        const auto& str_val = std::get<std::string>(value);

        // Check if column is a foreign key
        for (const auto& fk : table_def.foreign_keys) {
            if (fk.from_column == column) {
                auto id = lookup_id_by_label(fk.to_table, str_val, db);
                if (!id) {
                    throw std::runtime_error("Failed to resolve label '" + str_val + "' to ID in table '" +
                                             fk.to_table + "'");
                }
                return *id;
            }
        }

        // String value on a non-FK INTEGER column is an error
        auto col_type = table_def.get_data_type(column);
        if (col_type && *col_type == DataType::Integer) {
            throw std::runtime_error("Cannot resolve attribute: '" + column + "' is INTEGER but received string '" +
                                     str_val + "' (not a foreign key)");
        }

        // String value for TEXT/DATETIME column: pass through
        return value;
    }

    ResolvedElement resolve_element_fk_labels(const std::string& collection, const Element& element, Database& db) {
        ResolvedElement resolved;

        // Resolve scalars against collection table FK metadata
        const auto* collection_def = schema->get_table(collection);
        for (const auto& [name, value] : element.scalars()) {
            resolved.scalars[name] = resolve_fk_label(*collection_def, name, value, db);
        }

        // Resolve arrays against their respective group table FK metadata
        for (const auto& [array_name, values] : element.arrays()) {
            auto matches = schema->find_all_tables_for_column(collection, array_name);

            // Find the first table match for FK resolution
            // (FK columns have unique names per schema design, so first match is correct;
            //  non-FK columns pass through resolve_fk_label unchanged regardless of table)
            const TableDefinition* resolve_table = nullptr;
            for (const auto& match : matches) {
                const auto* td = schema->get_table(match.table_name);
                if (td) {
                    resolve_table = td;
                    break;
                }
            }

            std::vector<Value> resolved_values;
            resolved_values.reserve(values.size());
            for (const auto& val : values) {
                if (resolve_table) {
                    resolved_values.push_back(resolve_fk_label(*resolve_table, array_name, val, db));
                } else {
                    resolved_values.push_back(val);
                }
            }
            resolved.arrays[array_name] = std::move(resolved_values);
        }

        return resolved;
    }

    static const char* group_table_noun(GroupTableType type) {
        switch (type) {
        case GroupTableType::Vector:
            return "vector";
        case GroupTableType::Set:
            return "set";
        case GroupTableType::TimeSeries:
            return "time series";
        default:
            throw std::runtime_error("Cannot group_table_noun: unknown group table type " +
                                     std::to_string(static_cast<int>(type)));
        }
    }

    // Shared body of update_vector_group / update_set_group (defined in database_update.cpp):
    // resolves (collection, group) to exactly one table, validates and FK-resolves the rows,
    // then replaces the element's rows in that table.
    void update_group_rows(const char* caller,
                           const std::string& collection,
                           const std::string& group,
                           GroupTableType type,
                           int64_t id,
                           const std::vector<std::map<std::string, Value>>& rows,
                           Database& db);

    void insert_rows_into_group_table(const char* caller,
                                      const std::string& table_name,
                                      GroupTableType type,
                                      const std::map<std::string, const std::vector<Value>*>& columns,
                                      int64_t element_id,
                                      bool delete_existing,
                                      Database& db) {
        const char* noun = group_table_noun(type);

        // Validate types and verify same-length arrays *before* the DELETE: TransactionGuard
        // no-ops inside a caller-owned transaction (or a dry run), so throwing after the DELETE
        // would leave the group cleared with nothing to roll it back.
        // num_rows is seeded from the first column rather than the first non-empty one: `columns`
        // is name-sorted, so an empty alphabetically-first column used to leave it at 0 for a
        // later column to set, skipping the check and indexing the empty vector below.
        size_t num_rows = columns.empty() ? 0 : columns.begin()->second->size();
        for (const auto& [col_name, values_ptr] : columns) {
            if (!values_ptr->empty()) {
                type_validator->validate_array(caller, table_name, col_name, *values_ptr);
            }
            if (values_ptr->size() != num_rows) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": " + noun + " columns in table '" +
                                         table_name + "' must have the same length");
            }
        }

        if (delete_existing) {
            db.execute("DELETE FROM " + table_name + " WHERE id = ?", {element_id});
        }

        // Insert rows (vector tables get a 1-based vector_index column)
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto sql = "INSERT INTO " + table_name + " (id";
            std::string placeholders = "?";
            std::vector<Value> parameters = {element_id};

            if (type == GroupTableType::Vector) {
                sql += ", vector_index";
                placeholders += ", ?";
                parameters.emplace_back(static_cast<int64_t>(row_idx + 1));
            }

            for (const auto& [col_name, values_ptr] : columns) {
                sql += ", " + col_name;
                placeholders += ", ?";
                parameters.push_back((*values_ptr)[row_idx]);
            }

            sql += ") VALUES (" + placeholders + ")";
            db.execute(sql, parameters);
        }
        logger->debug("Inserted {} {} rows into {}", num_rows, noun, table_name);
    }

    void insert_group_data(const char* caller,
                           const std::string& collection,
                           int64_t element_id,
                           const std::map<std::string, std::vector<Value>>& arrays,
                           bool delete_existing,
                           Database& db) {
        // Route arrays to their target tables
        struct TableColumns {
            GroupTableType type;
            std::map<std::string, const std::vector<Value>*> columns;
        };
        std::map<std::string, TableColumns> table_columns;

        for (const auto& [array_name, values] : arrays) {
            // Empty array handling: create skips silently, update still routes (for DELETE)
            if (values.empty() && !delete_existing) {
                continue;
            }

            auto matches = schema->find_all_tables_for_column(collection, array_name);
            if (matches.empty()) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": array '" + array_name +
                                         "' does not match any vector, set, or time series table in collection '" +
                                         collection + "'");
            }

            // A column name shared by several group tables (legal for FK columns, which
            // validate_no_duplicate_attributes exempts) cannot say which group was meant, so the
            // array lands in all of them - rewriting groups the caller never named. Kept for
            // compatibility, but say so: update_vector_group / update_set_group take
            // (collection, group) and name exactly one table.
            if (matches.size() > 1) {
                std::string table_list;
                for (const auto& match : matches) {
                    table_list += (table_list.empty() ? "" : ", ") + match.table_name;
                }
                logger->warn("{}: array '{}' matches {} group tables ({}) and will be written to all of "
                             "them; use update_vector_group/update_set_group to target one group",
                             caller,
                             array_name,
                             matches.size(),
                             table_list);
            }

            for (const auto& match : matches) {
                auto& entry = table_columns[match.table_name];
                entry.type = match.type;
                entry.columns[array_name] = &values;
            }
        }

        for (const auto& [table_name, entry] : table_columns) {
            insert_rows_into_group_table(
                caller, table_name, entry.type, entry.columns, element_id, delete_existing, db);
        }
    }

    // Nothing is published until validation passes: a half-loaded state (schema set,
    // type_validator null) would survive a failed lazy load and crash the next call.
    void load_schema_metadata() const {
        auto loaded = std::make_unique<Schema>(Schema::from_database(db));
        SchemaValidator(*loaded).validate();
        // TypeValidator holds a reference to the Schema; moving the unique_ptr keeps the pointee.
        type_validator = std::make_unique<TypeValidator>(*loaded);
        schema = std::move(loaded);
    }

    ~Impl() {
        if (db) {
            logger->debug("Closing database: {}", path);
            sqlite3_close_v2(db);
            db = nullptr;
            logger->info("Database closed");
        }
    }

    void begin_transaction() {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error("Failed to begin transaction: " + error);
        }
        logger->debug("Transaction started");
    }

    void commit() {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            throw std::runtime_error("Failed to commit transaction: " + error);
        }
        logger->debug("Transaction committed");
    }

    void rollback() {
        char* err_msg = nullptr;
        const auto rc = sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string error = err_msg ? err_msg : "Unknown error";
            sqlite3_free(err_msg);
            logger->error("Failed to rollback transaction: {}", error);
            // Don't throw - rollback is often called in error recovery
        } else {
            logger->debug("Transaction rolled back");
        }
    }

    class TransactionGuard {
        Impl& impl_;
        bool committed_ = false;
        bool owns_transaction_ = false;

    public:
        explicit TransactionGuard(Impl& impl) : impl_(impl) {
            if (sqlite3_get_autocommit(impl_.db)) {
                impl_.begin_transaction();
                owns_transaction_ = true;
            }
        }

        void commit() {
            if (owns_transaction_) {
                impl_.commit();
            }
            committed_ = true;
        }

        ~TransactionGuard() {
            if (!committed_ && owns_transaction_) {
                impl_.rollback();
            }
        }

        TransactionGuard(const TransactionGuard&) = delete;
        TransactionGuard& operator=(const TransactionGuard&) = delete;
    };
};

}  // namespace quiver

#endif  // QUIVER_DATABASE_IMPL_H
