#ifndef QUIVER_DATABASE_IMPL_H
#define QUIVER_DATABASE_IMPL_H

#include "quiver/database.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"

#include <memory>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <string>

namespace quiver {

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

    public:
        explicit TransactionGuard(Impl& impl) : impl_(impl) { impl_.begin_transaction(); }

        void commit() {
            impl_.commit();
            committed_ = true;
        }

        ~TransactionGuard() {
            if (!committed_) {
                impl_.rollback();
            }
        }

        TransactionGuard(const TransactionGuard&) = delete;
        TransactionGuard& operator=(const TransactionGuard&) = delete;
    };
};

}  // namespace quiver

#endif  // QUIVER_DATABASE_IMPL_H
