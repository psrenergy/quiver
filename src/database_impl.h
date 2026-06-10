#ifndef QUIVER_DATABASE_IMPL_H
#define QUIVER_DATABASE_IMPL_H

#include "quiver/database.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"

#include <map>
#include <memory>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <string>
#include <vector>

namespace quiver {

using StmtPtr = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

struct ResolvedElement {
    std::map<std::string, Value> scalars;
    std::map<std::string, std::vector<Value>> arrays;
};

struct Database::Impl {
    sqlite3* db = nullptr;
    std::string path;
    std::shared_ptr<spdlog::logger> logger;
    std::unique_ptr<Schema> schema;
    std::unique_ptr<TypeValidator> type_validator;

    void require_schema(const char* operation) const {
        if (!schema) {
            throw std::runtime_error(std::string("Cannot ") + operation + ": no schema loaded");
        }
    }

    void require_collection(const std::string& collection, const char* operation) const {
        require_schema(operation);
        if (!schema->has_table(collection)) {
            throw std::runtime_error(std::string("Cannot ") + operation + ": collection not found: " + collection);
        }
    }

    void require_column(const std::string& table, const std::string& column, const char* operation) const {
        require_schema(operation);
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
                auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
                auto lookup_result = db.execute(lookup_sql, {str_val});
                if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
                    throw std::runtime_error("Failed to resolve label '" + str_val + "' to ID in table '" +
                                             fk.to_table + "'");
                }
                return lookup_result[0].get_integer(0).value();
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

    void insert_rows_into_group_table(const char* caller,
                                      const std::string& table_name,
                                      GroupTableType type,
                                      const std::map<std::string, const std::vector<Value>*>& columns,
                                      int64_t element_id,
                                      bool delete_existing,
                                      Database& db) {
        const char* noun = group_table_noun(type);

        if (delete_existing) {
            db.execute("DELETE FROM " + table_name + " WHERE id = ?", {element_id});
        }

        // Validate types and verify same-length arrays
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            if (!values_ptr->empty()) {
                type_validator->validate_array(caller, table_name, col_name, *values_ptr);
            }
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error(std::string("Cannot ") + caller + ": " + noun + " columns in table '" +
                                         table_name + "' must have the same length");
            }
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

            for (const auto& match : matches) {
                auto& entry = table_columns[match.table_name];
                entry.type = match.type;
                entry.columns[array_name] = &values;
            }
        }

        for (const auto& [table_name, entry] : table_columns) {
            insert_rows_into_group_table(caller, table_name, entry.type, entry.columns, element_id, delete_existing,
                                         db);
        }
    }

    void load_schema_metadata() {
        schema = std::make_unique<Schema>(Schema::from_database(db));
        SchemaValidator validator(*schema);
        validator.validate();
        type_validator = std::make_unique<TypeValidator>(*schema);
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
