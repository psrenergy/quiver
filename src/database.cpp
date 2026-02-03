#include "quiver/database.h"

#include "quiver/migrations.h"
#include "quiver/result.h"
#include "quiver/schema.h"
#include "quiver/schema_validator.h"
#include "quiver/type_validator.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace {

std::atomic<uint64_t> g_logger_counter{0};
std::once_flag sqlite3_init_flag;

// Type-specific Row value extractors for template implementations
inline std::optional<int64_t> get_row_value(const quiver::Row& row, size_t index, int64_t*) {
    return row.get_integer(index);
}

inline std::optional<double> get_row_value(const quiver::Row& row, size_t index, double*) {
    return row.get_float(index);
}

inline std::optional<std::string> get_row_value(const quiver::Row& row, size_t index, std::string*) {
    return row.get_string(index);
}

// Template for reading grouped values (vectors or sets) for all elements
template <typename T>
std::vector<std::vector<T>> read_grouped_values_all(const quiver::Result& result) {
    std::vector<std::vector<T>> groups;
    int64_t current_id = -1;

    for (size_t i = 0; i < result.row_count(); ++i) {
        auto id = result[i].get_integer(0);
        auto val = get_row_value(result[i], 1, static_cast<T*>(nullptr));

        if (!id)
            continue;

        if (*id != current_id) {
            groups.emplace_back();
            current_id = *id;
        }

        if (val) {
            groups.back().push_back(*val);
        }
    }
    return groups;
}

// Template for reading grouped values (vectors or sets) for a single element by ID
template <typename T>
std::vector<T> read_grouped_values_by_id(const quiver::Result& result) {
    std::vector<T> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = get_row_value(result[i], 0, static_cast<T*>(nullptr));
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

void ensure_sqlite3_initialized() {
    std::call_once(sqlite3_init_flag, []() { sqlite3_initialize(); });
}

spdlog::level::level_enum to_spdlog_level(quiver::LogLevel level) {
    switch (level) {
    case quiver::LogLevel::debug:
        return spdlog::level::debug;
    case quiver::LogLevel::info:
        return spdlog::level::info;
    case quiver::LogLevel::warn:
        return spdlog::level::warn;
    case quiver::LogLevel::error:
        return spdlog::level::err;
    case quiver::LogLevel::off:
        return spdlog::level::off;
    default:
        return spdlog::level::info;
    }
}

std::shared_ptr<spdlog::logger> create_database_logger(const std::string& db_path, quiver::LogLevel console_level) {
    namespace fs = std::filesystem;

    // Generate unique logger name for multiple Database instances
    auto id = g_logger_counter.fetch_add(1);
    auto logger_name = "quiver_database_" + std::to_string(id);

    // Create console sink (thread-safe)
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(to_spdlog_level(console_level));

    if (db_path == ":memory:") {
        // In-memory database: no file logging
        const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
        logger->set_level(spdlog::level::debug);
        logger->warn("Database is in-memory only; no file logging will be performed.");
        return logger;
    } else {
        // File-based database: use database directory
        auto db_dir = fs::path(db_path).parent_path();
        if (db_dir.empty()) {
            // Database file in current directory (no path separator)
            db_dir = fs::current_path();
        }

        // Create file sink (thread-safe)
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt> file_sink = nullptr;
        try {
            const auto log_file_path = (db_dir / "quiver_database.log").string();
            file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file_path, true);
            file_sink->set_level(spdlog::level::debug);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
            auto logger = std::make_shared<spdlog::logger>(logger_name, sinks.begin(), sinks.end());
            logger->set_level(spdlog::level::debug);

            return logger;
        } catch (const spdlog::spdlog_ex& ex) {
            // If file sink creation fails, continue with console-only logging
            const auto logger = std::make_shared<spdlog::logger>(logger_name, console_sink);
            logger->set_level(spdlog::level::debug);
            logger->warn("Failed to create file sink: {}. Logging to console only.", ex.what());
            return logger;
        }
    }
}

}  // anonymous namespace

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
            throw std::runtime_error("Collection not found in schema: " + collection);
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

Database::Database(const std::string& path, const DatabaseOptions& options) : impl_(std::make_unique<Impl>()) {
    impl_->path = path;
    impl_->logger = create_database_logger(path, options.console_level);

    impl_->logger->debug("Opening database: {}", path);

    ensure_sqlite3_initialized();

    auto flags = options.read_only ? SQLITE_OPEN_READONLY : (SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE);
    const auto rc = sqlite3_open_v2(path.c_str(), &impl_->db, flags, nullptr);

    if (rc != SQLITE_OK) {
        std::string error_msg = impl_->db ? sqlite3_errmsg(impl_->db) : "Unknown error";
        impl_->logger->error("Failed to open database: {}", error_msg);
        if (impl_->db) {
            sqlite3_close(impl_->db);
            impl_->db = nullptr;
        }
        throw std::runtime_error("Failed to open database: " + error_msg);
    }

    // Enable foreign keys
    sqlite3_exec(impl_->db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);
    impl_->logger->debug("Database opened successfully, foreign keys enabled");

    impl_->logger->info("Database opened successfully: {}", path);
}

Database::~Database() = default;

Database::Database(Database&& other) noexcept = default;
Database& Database::operator=(Database&& other) noexcept = default;

bool Database::is_healthy() const {
    return impl_ && impl_->db != nullptr;
}

Result Database::execute(const std::string& sql, const std::vector<Value>& params) {
    sqlite3_stmt* stmt = nullptr;
    auto rc = sqlite3_prepare_v2(impl_->db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    // Bind parameters
    for (size_t i = 0; i < params.size(); ++i) {
        const auto idx = static_cast<int>(i + 1);
        const auto& param = params[i];

        std::visit(
            [&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::nullptr_t>) {
                    sqlite3_bind_null(stmt, idx);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    sqlite3_bind_int64(stmt, idx, arg);
                } else if constexpr (std::is_same_v<T, double>) {
                    sqlite3_bind_double(stmt, idx, arg);
                } else if constexpr (std::is_same_v<T, std::string>) {
                    sqlite3_bind_text(stmt, idx, arg.c_str(), static_cast<int>(arg.size()), SQLITE_TRANSIENT);
                }
            },
            param);
    }

    // Get column info
    std::vector<std::string> columns;
    const auto col_count = sqlite3_column_count(stmt);
    columns.reserve(col_count);
    for (int i = 0; i < col_count; ++i) {
        const char* name = sqlite3_column_name(stmt, i);
        columns.emplace_back(name ? name : "");
    }

    // Execute and fetch results
    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        std::vector<Value> values;
        values.reserve(col_count);

        for (int i = 0; i < col_count; ++i) {
            auto type = sqlite3_column_type(stmt, i);
            switch (type) {
            case SQLITE_INTEGER:
                values.emplace_back(sqlite3_column_int64(stmt, i));
                break;
            case SQLITE_FLOAT:
                values.emplace_back(sqlite3_column_double(stmt, i));
                break;
            case SQLITE_TEXT: {
                const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
                values.emplace_back(std::string(text ? text : ""));
                break;
            }
            case SQLITE_NULL:
                values.emplace_back(nullptr);
                break;
            case SQLITE_BLOB:
                throw std::runtime_error("Blob not implemented");
            default:
                throw std::runtime_error("Type not implemented");
                break;
            }
        }
        rows.emplace_back(std::move(values));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error("Failed to execute statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    return Result(std::move(columns), std::move(rows));
}

int64_t Database::current_version() const {
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "PRAGMA user_version;";
    auto rc = sqlite3_prepare_v2(impl_->db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare statement: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    rc = sqlite3_step(stmt);
    int64_t user_version = 0;
    if (rc == SQLITE_ROW) {
        user_version = sqlite3_column_int(stmt, 0);
    } else {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to retrieve user_version: " + std::string(sqlite3_errmsg(impl_->db)));
    }

    sqlite3_finalize(stmt);
    return user_version;
}

const std::string& Database::path() const {
    return impl_->path;
}

Database Database::from_migrations(const std::string& db_path,
                                   const std::string& migrations_path,
                                   const DatabaseOptions& options) {
    namespace fs = std::filesystem;
    if (!fs::exists(migrations_path)) {
        throw std::runtime_error("Migrations path not found: " + migrations_path);
    }
    if (!fs::is_directory(migrations_path)) {
        throw std::runtime_error("Migrations path is not a directory: " + migrations_path);
    }
    auto db = Database(db_path, options);
    db.migrate_up(migrations_path);
    return db;
}

Database
Database::from_schema(const std::string& db_path, const std::string& schema_path, const DatabaseOptions& options) {
    namespace fs = std::filesystem;
    if (!fs::exists(schema_path)) {
        throw std::runtime_error("Schema file not found: " + schema_path);
    }
    auto db = Database(db_path, options);
    db.apply_schema(schema_path);
    return db;
}

void Database::set_version(int64_t version) {
    const auto sql = "PRAGMA user_version = " + std::to_string(version) + ";";
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to set user_version: " + error);
    }
    impl_->logger->debug("Set database version to {}", version);
}

void Database::begin_transaction() {
    impl_->begin_transaction();
}
void Database::commit() {
    impl_->commit();
}
void Database::rollback() {
    impl_->rollback();
}

void Database::execute_raw(const std::string& sql) {
    char* err_msg = nullptr;
    const auto rc = sqlite3_exec(impl_->db, sql.c_str(), nullptr, nullptr, &err_msg);
    if (rc != SQLITE_OK) {
        std::string error = err_msg ? err_msg : "Unknown error";
        sqlite3_free(err_msg);
        throw std::runtime_error("Failed to execute SQL: " + error);
    }
}

void Database::migrate_up(const std::string& migrations_path) {
    const auto migrations = Migrations(migrations_path);
    if (migrations.empty()) {
        impl_->logger->debug("No migrations found in {}", migrations_path);
        return;
    }

    const auto current = current_version();
    const auto pending = migrations.pending(current);

    if (pending.empty()) {
        impl_->logger->debug("Database is up to date at version {}", current);
        if (!impl_->schema) {
            impl_->load_schema_metadata();
        }
        return;
    }

    impl_->logger->info(
        "Applying {} pending migration(s) from version {} to {}", pending.size(), current, migrations.latest_version());

    for (const auto& migration : pending) {
        impl_->logger->info("Applying migration {}", migration.version());

        const auto up_sql = migration.up_sql();
        if (up_sql.empty()) {
            throw std::runtime_error("Migration " + std::to_string(migration.version()) + " has no up.sql file");
        }

        begin_transaction();
        try {
            execute_raw(up_sql);
            set_version(migration.version());
            commit();
            impl_->logger->info("Migration {} applied successfully", migration.version());
        } catch (const std::exception& e) {
            rollback();
            impl_->logger->error("Migration {} failed: {}", migration.version(), e.what());
            throw std::runtime_error("Migration " + std::to_string(migration.version()) + " failed: " + e.what());
        }
    }

    impl_->load_schema_metadata();
    impl_->logger->info("All migrations applied successfully. Database now at version {}", current_version());
}

void Database::apply_schema(const std::string& schema_path) {
    std::ifstream file(schema_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open schema file: " + schema_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    const auto schema_sql = buffer.str();

    if (schema_sql.empty()) {
        throw std::runtime_error("Schema file is empty: " + schema_path);
    }

    impl_->logger->info("Applying schema from: {}", schema_path);

    begin_transaction();
    try {
        execute_raw(schema_sql);
        impl_->load_schema_metadata();
        commit();
    } catch (const std::exception& e) {
        rollback();
        impl_->logger->error("Failed to apply schema: {}", e.what());
        throw;
    }

    impl_->logger->info("Schema applied successfully");
}

int64_t Database::create_element(const std::string& collection, const Element& element) {
    impl_->logger->debug("Creating element in collection: {}", collection);
    impl_->require_collection(collection, "create element");

    const auto& scalars = element.scalars();
    if (scalars.empty()) {
        throw std::runtime_error("Element must have at least one scalar attribute");
    }

    // Validate scalar types
    for (const auto& [name, value] : scalars) {
        impl_->type_validator->validate_scalar(collection, name, value);
    }

    // Build INSERT SQL for main collection table
    auto sql = "INSERT INTO " + collection + " (";
    std::string placeholders;
    std::vector<Value> params;

    auto first = true;
    for (const auto& [name, value] : scalars) {
        if (!first) {
            sql += ", ";
            placeholders += ", ";
        }
        sql += name;
        placeholders += "?";
        params.push_back(value);
        first = false;
    }
    sql += ") VALUES (" + placeholders + ")";

    execute(sql, params);
    const auto element_id = sqlite3_last_insert_rowid(impl_->db);
    impl_->logger->debug("Inserted element with id: {}", element_id);

    // Process arrays - route to vector or set tables based on schema
    const auto& arrays = element.arrays();

    // Build a map of set table -> (column_name -> array values)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> set_table_columns;
    // Build a map of vector table -> (column_name -> array values)
    std::map<std::string, std::map<std::string, const std::vector<Value>*>> vector_table_columns;

    for (const auto& [array_name, values] : arrays) {
        if (values.empty()) {
            throw std::runtime_error("Empty array not allowed for '" + array_name + "'");
        }

        // Check if this is a single-column vector table (Collection_vector_arrayname)
        auto vector_table = Schema::vector_table_name(collection, array_name);
        if (impl_->schema->has_table(vector_table)) {
            vector_table_columns[vector_table][array_name] = &values;
            continue;
        }

        // Check if this array_name is a column in any vector table for the collection
        bool found_vector_table = false;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_vector_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            const auto* table_def = impl_->schema->get_table(table_name);
            if (table_def && table_def->has_column(array_name)) {
                vector_table_columns[table_name][array_name] = &values;
                found_vector_table = true;
                break;
            }
        }

        if (found_vector_table) {
            continue;
        }

        // Check if this array_name is a column in any set table
        bool found_set_table = false;
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_set_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            const auto* table_def = impl_->schema->get_table(table_name);
            if (table_def && table_def->has_column(array_name)) {
                set_table_columns[table_name][array_name] = &values;
                found_set_table = true;
                break;
            }
        }

        if (!found_set_table) {
            throw std::runtime_error("Array '" + array_name +
                                     "' does not match any vector or set table for collection '" + collection + "'");
        }
    }

    // Insert vector table data - zip arrays together into rows with vector_index
    for (const auto& [vector_table, columns] : vector_table_columns) {
        const auto* table_def = impl_->schema->get_table(vector_table);
        if (!table_def) {
            throw std::runtime_error("Vector table not found: " + vector_table);
        }

        // Verify all arrays have same length
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            impl_->type_validator->validate_array(vector_table, col_name, *values_ptr);
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Vector columns in table '" + vector_table +
                                         "' must have the same length, but got different lengths for columns");
            }
        }

        // Insert each row with vector_index
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            int64_t vector_index = static_cast<int64_t>(row_idx + 1);
            auto vec_sql = "INSERT INTO " + vector_table + " (id, vector_index";
            std::string vec_placeholders = "?, ?";
            std::vector<Value> vec_params = {element_id, vector_index};

            for (const auto& [col_name, values_ptr] : columns) {
                vec_sql += ", " + col_name;
                vec_placeholders += ", ?";
                vec_params.push_back((*values_ptr)[row_idx]);
            }

            vec_sql += ") VALUES (" + vec_placeholders + ")";
            execute(vec_sql, vec_params);
        }
        impl_->logger->debug("Inserted {} vector rows into {}", num_rows, vector_table);
    }

    // Insert set table data - zip arrays together into rows
    for (const auto& [set_table, columns] : set_table_columns) {
        const auto* table_def = impl_->schema->get_table(set_table);
        if (!table_def) {
            throw std::runtime_error("Set table not found: " + set_table);
        }

        // Verify all arrays have same length
        size_t num_rows = 0;
        for (const auto& [col_name, values_ptr] : columns) {
            if (num_rows == 0) {
                num_rows = values_ptr->size();
            } else if (values_ptr->size() != num_rows) {
                throw std::runtime_error("Set columns in table '" + set_table +
                                         "' must have the same length, but got different lengths for columns");
            }
        }

        // Insert each row
        for (size_t row_idx = 0; row_idx < num_rows; ++row_idx) {
            auto set_sql = "INSERT INTO " + set_table + " (id";
            std::string set_placeholders = "?";
            std::vector<Value> set_params = {element_id};

            for (const auto& [col_name, values_ptr] : columns) {
                set_sql += ", " + col_name;
                set_placeholders += ", ?";

                Value val = (*values_ptr)[row_idx];

                // Check if this column is a FK and value is a string (label) that needs resolution
                for (const auto& fk : table_def->foreign_keys) {
                    if (fk.from_column == col_name && std::holds_alternative<std::string>(val)) {
                        const std::string& label = std::get<std::string>(val);
                        // Look up the ID by label using direct SQL query
                        auto lookup_sql = "SELECT id FROM " + fk.to_table + " WHERE label = ?";
                        auto lookup_result = execute(lookup_sql, {label});
                        if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
                            throw std::runtime_error("Failed to resolve label '" + label + "' to ID in table '" +
                                                     fk.to_table + "'");
                        }
                        val = lookup_result[0].get_integer(0).value();
                        break;
                    }
                }

                set_params.push_back(val);
            }
            set_sql += ") VALUES (" + set_placeholders + ")";
            execute(set_sql, set_params);
        }
        impl_->logger->debug("Inserted {} set rows for table {}", num_rows, set_table);
    }

    impl_->logger->info("Created element {} in {}", element_id, collection);
    return element_id;
}

void Database::update_element(const std::string& collection, int64_t id, const Element& element) {
    impl_->logger->debug("Updating element {} in collection: {}", id, collection);
    impl_->require_collection(collection, "update element");

    const auto& scalars = element.scalars();
    const auto& arrays = element.arrays();

    if (scalars.empty() && arrays.empty()) {
        throw std::runtime_error("Element must have at least one attribute to update");
    }

    Impl::TransactionGuard txn(*impl_);

    // Update scalars if present
    if (!scalars.empty()) {
        // Validate scalar types
        for (const auto& [name, value] : scalars) {
            impl_->type_validator->validate_scalar(collection, name, value);
        }

        // Build UPDATE SQL
        auto sql = "UPDATE " + collection + " SET ";
        std::vector<Value> params;

        auto first = true;
        for (const auto& [name, value] : scalars) {
            if (!first) {
                sql += ", ";
            }
            sql += name + " = ?";
            params.push_back(value);
            first = false;
        }
        sql += " WHERE id = ?";
        params.push_back(id);

        execute(sql, params);
    }

    // Update vectors/sets if present
    for (const auto& [attr_name, values] : arrays) {
        // Try to find as vector table first
        bool found_vector = false;
        std::string vector_table;
        try {
            vector_table = impl_->schema->find_vector_table(collection, attr_name);
            found_vector = true;
        } catch (const std::runtime_error&) {
            // Not a vector table, try set
        }

        if (found_vector) {
            // Delete existing vector data
            auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
            execute(delete_sql, {id});

            // Insert new vector data
            for (size_t i = 0; i < values.size(); ++i) {
                auto insert_sql =
                    "INSERT INTO " + vector_table + " (id, vector_index, " + attr_name + ") VALUES (?, ?, ?)";
                int64_t vector_index = static_cast<int64_t>(i + 1);
                execute(insert_sql, {id, vector_index, values[i]});
            }
            impl_->logger->debug(
                "Updated vector {}.{} for id {} with {} values", collection, attr_name, id, values.size());
            continue;
        }

        // Try to find as set table
        std::string set_table;
        try {
            set_table = impl_->schema->find_set_table(collection, attr_name);
        } catch (const std::runtime_error&) {
            throw std::runtime_error("Attribute '" + attr_name + "' is not a vector or set attribute in collection '" +
                                     collection + "'");
        }

        // Delete existing set data
        auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
        execute(delete_sql, {id});

        // Insert new set data
        for (const auto& value : values) {
            auto insert_sql = "INSERT INTO " + set_table + " (id, " + attr_name + ") VALUES (?, ?)";
            execute(insert_sql, {id, value});
        }
        impl_->logger->debug("Updated set {}.{} for id {} with {} values", collection, attr_name, id, values.size());
    }

    txn.commit();
    impl_->logger->info("Updated element {} in {}", id, collection);
}

void Database::delete_element_by_id(const std::string& collection, int64_t id) {
    impl_->logger->debug("Deleting element {} from collection: {}", id, collection);
    impl_->require_collection(collection, "delete element");

    auto sql = "DELETE FROM " + collection + " WHERE id = ?";
    execute(sql, {id});

    impl_->logger->info("Deleted element {} from {}", id, collection);
}

void Database::set_scalar_relation(const std::string& collection,
                                   const std::string& attribute,
                                   const std::string& from_label,
                                   const std::string& to_label) {
    impl_->logger->debug("Setting relation {}.{} from '{}' to '{}'", collection, attribute, from_label, to_label);

    if (!impl_->schema) {
        throw std::runtime_error("Cannot set relation: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    // Find the foreign key with the given attribute name
    std::string to_table;
    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == attribute) {
            to_table = fk.to_table;
            break;
        }
    }

    if (to_table.empty()) {
        throw std::runtime_error("Attribute '" + attribute + "' is not a foreign key in collection '" + collection +
                                 "'");
    }

    // Look up the target ID by label
    auto lookup_sql = "SELECT id FROM " + to_table + " WHERE label = ?";
    auto lookup_result = execute(lookup_sql, {to_label});
    if (lookup_result.empty() || !lookup_result[0].get_integer(0)) {
        throw std::runtime_error("Target element with label '" + to_label + "' not found in '" + to_table + "'");
    }
    auto to_id = lookup_result[0].get_integer(0).value();

    // Update the source element
    auto update_sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE label = ?";
    execute(update_sql, {to_id, from_label});

    impl_->logger->info(
        "Set relation {}.{} for '{}' to '{}' (id: {})", collection, attribute, from_label, to_label, to_id);
}

std::vector<std::string> Database::read_scalar_relation(const std::string& collection, const std::string& attribute) {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot read relation: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    // Find the foreign key with the given attribute name
    std::string to_table;
    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == attribute) {
            to_table = fk.to_table;
            break;
        }
    }

    if (to_table.empty()) {
        throw std::runtime_error("Attribute '" + attribute + "' is not a foreign key in collection '" + collection +
                                 "'");
    }

    // LEFT JOIN to get target labels (NULL for unset relations)
    auto sql = "SELECT t.label FROM " + collection + " c LEFT JOIN " + to_table + " t ON c." + attribute + " = t.id";
    auto result = execute(sql);

    std::vector<std::string> labels;
    labels.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_string(0);
        labels.push_back(val.value_or(""));
    }
    return labels;
}

std::vector<int64_t> Database::read_scalar_integers(const std::string& collection, const std::string& attribute) {
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<int64_t> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_integer(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<double> Database::read_scalar_floats(const std::string& collection, const std::string& attribute) {
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<double> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_float(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::vector<std::string> Database::read_scalar_strings(const std::string& collection, const std::string& attribute) {
    auto sql = "SELECT " + attribute + " FROM " + collection;
    auto result = execute(sql);

    std::vector<std::string> values;
    values.reserve(result.row_count());
    for (size_t i = 0; i < result.row_count(); ++i) {
        auto val = result[i].get_string(0);
        if (val) {
            values.push_back(*val);
        }
    }
    return values;
}

std::optional<int64_t>
Database::read_scalar_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_integer(0);
}

std::optional<double>
Database::read_scalar_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_float(0);
}

std::optional<std::string>
Database::read_scalar_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto sql = "SELECT " + attribute + " FROM " + collection + " WHERE id = ?";
    auto result = execute(sql, {id});

    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_string(0);
}

std::vector<std::vector<int64_t>> Database::read_vector_integers(const std::string& collection,
                                                                 const std::string& attribute) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return read_grouped_values_all<int64_t>(execute(sql));
}

std::vector<std::vector<double>> Database::read_vector_floats(const std::string& collection,
                                                              const std::string& attribute) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return read_grouped_values_all<double>(execute(sql));
}

std::vector<std::vector<std::string>> Database::read_vector_strings(const std::string& collection,
                                                                    const std::string& attribute) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + vector_table + " ORDER BY id, vector_index";
    return read_grouped_values_all<std::string>(execute(sql));
}

std::vector<int64_t>
Database::read_vector_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return read_grouped_values_by_id<int64_t>(execute(sql, {id}));
}

std::vector<double>
Database::read_vector_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return read_grouped_values_by_id<double>(execute(sql, {id}));
}

std::vector<std::string>
Database::read_vector_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto vector_table = impl_->schema->find_vector_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + vector_table + " WHERE id = ? ORDER BY vector_index";
    return read_grouped_values_by_id<std::string>(execute(sql, {id}));
}

std::vector<std::vector<int64_t>> Database::read_set_integers(const std::string& collection,
                                                              const std::string& attribute) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return read_grouped_values_all<int64_t>(execute(sql));
}

std::vector<std::vector<double>> Database::read_set_floats(const std::string& collection,
                                                           const std::string& attribute) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return read_grouped_values_all<double>(execute(sql));
}

std::vector<std::vector<std::string>> Database::read_set_strings(const std::string& collection,
                                                                 const std::string& attribute) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT id, " + attribute + " FROM " + set_table + " ORDER BY id";
    return read_grouped_values_all<std::string>(execute(sql));
}

std::vector<int64_t>
Database::read_set_integers_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return read_grouped_values_by_id<int64_t>(execute(sql, {id}));
}

std::vector<double>
Database::read_set_floats_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return read_grouped_values_by_id<double>(execute(sql, {id}));
}

std::vector<std::string>
Database::read_set_strings_by_id(const std::string& collection, const std::string& attribute, int64_t id) {
    auto set_table = impl_->schema->find_set_table(collection, attribute);
    auto sql = "SELECT " + attribute + " FROM " + set_table + " WHERE id = ?";
    return read_grouped_values_by_id<std::string>(execute(sql, {id}));
}

std::vector<int64_t> Database::read_element_ids(const std::string& collection) {
    auto sql = "SELECT id FROM " + collection + " ORDER BY rowid";
    return read_grouped_values_by_id<int64_t>(execute(sql));
}

void Database::update_scalar_integer(const std::string& collection,
                                     const std::string& attribute,
                                     int64_t id,
                                     int64_t value) {
    impl_->logger->debug("Updating {}.{} for id {} to {}", collection, attribute, id, value);
    impl_->require_collection(collection, "update scalar");
    impl_->type_validator->validate_scalar(collection, attribute, value);

    auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";
    execute(sql, {value, id});

    impl_->logger->info("Updated {}.{} for id {} to {}", collection, attribute, id, value);
}

void Database::update_scalar_float(const std::string& collection,
                                   const std::string& attribute,
                                   int64_t id,
                                   double value) {
    impl_->logger->debug("Updating {}.{} for id {} to {}", collection, attribute, id, value);
    impl_->require_collection(collection, "update scalar");
    impl_->type_validator->validate_scalar(collection, attribute, value);

    auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";
    execute(sql, {value, id});

    impl_->logger->info("Updated {}.{} for id {} to {}", collection, attribute, id, value);
}

void Database::update_scalar_string(const std::string& collection,
                                    const std::string& attribute,
                                    int64_t id,
                                    const std::string& value) {
    impl_->logger->debug("Updating {}.{} for id {} to '{}'", collection, attribute, id, value);
    impl_->require_collection(collection, "update scalar");
    impl_->type_validator->validate_scalar(collection, attribute, value);

    auto sql = "UPDATE " + collection + " SET " + attribute + " = ? WHERE id = ?";
    execute(sql, {value, id});

    impl_->logger->info("Updated {}.{} for id {} to '{}'", collection, attribute, id, value);
}

void Database::update_vector_integers(const std::string& collection,
                                      const std::string& attribute,
                                      int64_t id,
                                      const std::vector<int64_t>& values) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update vector");

    auto vector_table = impl_->schema->find_vector_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (size_t i = 0; i < values.size(); ++i) {
        auto insert_sql = "INSERT INTO " + vector_table + " (id, vector_index, " + attribute + ") VALUES (?, ?, ?)";
        int64_t vector_index = static_cast<int64_t>(i + 1);
        execute(insert_sql, {id, vector_index, values[i]});
    }

    txn.commit();
    impl_->logger->info("Updated vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_vector_floats(const std::string& collection,
                                    const std::string& attribute,
                                    int64_t id,
                                    const std::vector<double>& values) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update vector");

    auto vector_table = impl_->schema->find_vector_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (size_t i = 0; i < values.size(); ++i) {
        auto insert_sql = "INSERT INTO " + vector_table + " (id, vector_index, " + attribute + ") VALUES (?, ?, ?)";
        int64_t vector_index = static_cast<int64_t>(i + 1);
        execute(insert_sql, {id, vector_index, values[i]});
    }

    txn.commit();
    impl_->logger->info("Updated vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_vector_strings(const std::string& collection,
                                     const std::string& attribute,
                                     int64_t id,
                                     const std::vector<std::string>& values) {
    impl_->logger->debug("Updating vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update vector");

    auto vector_table = impl_->schema->find_vector_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + vector_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (size_t i = 0; i < values.size(); ++i) {
        auto insert_sql = "INSERT INTO " + vector_table + " (id, vector_index, " + attribute + ") VALUES (?, ?, ?)";
        int64_t vector_index = static_cast<int64_t>(i + 1);
        execute(insert_sql, {id, vector_index, values[i]});
    }

    txn.commit();
    impl_->logger->info("Updated vector {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_set_integers(const std::string& collection,
                                   const std::string& attribute,
                                   int64_t id,
                                   const std::vector<int64_t>& values) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update set");

    auto set_table = impl_->schema->find_set_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (const auto& value : values) {
        auto insert_sql = "INSERT INTO " + set_table + " (id, " + attribute + ") VALUES (?, ?)";
        execute(insert_sql, {id, value});
    }

    txn.commit();
    impl_->logger->info("Updated set {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_set_floats(const std::string& collection,
                                 const std::string& attribute,
                                 int64_t id,
                                 const std::vector<double>& values) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update set");

    auto set_table = impl_->schema->find_set_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (const auto& value : values) {
        auto insert_sql = "INSERT INTO " + set_table + " (id, " + attribute + ") VALUES (?, ?)";
        execute(insert_sql, {id, value});
    }

    txn.commit();
    impl_->logger->info("Updated set {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

void Database::update_set_strings(const std::string& collection,
                                  const std::string& attribute,
                                  int64_t id,
                                  const std::vector<std::string>& values) {
    impl_->logger->debug("Updating set {}.{} for id {} with {} values", collection, attribute, id, values.size());
    impl_->require_schema("update set");

    auto set_table = impl_->schema->find_set_table(collection, attribute);

    Impl::TransactionGuard txn(*impl_);

    auto delete_sql = "DELETE FROM " + set_table + " WHERE id = ?";
    execute(delete_sql, {id});

    for (const auto& value : values) {
        auto insert_sql = "INSERT INTO " + set_table + " (id, " + attribute + ") VALUES (?, ?)";
        execute(insert_sql, {id, value});
    }

    txn.commit();
    impl_->logger->info("Updated set {}.{} for id {} with {} values", collection, attribute, id, values.size());
}

ScalarMetadata Database::get_scalar_metadata(const std::string& collection, const std::string& attribute) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get scalar metadata: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    const auto* col = table_def->get_column(attribute);
    if (!col) {
        throw std::runtime_error("Scalar attribute '" + attribute + "' not found in collection '" + collection + "'");
    }

    ScalarMetadata meta;
    meta.name = col->name;
    meta.data_type = col->type;
    meta.not_null = col->not_null;
    meta.primary_key = col->primary_key;
    meta.default_value = col->default_value;

    for (const auto& fk : table_def->foreign_keys) {
        if (fk.from_column == attribute) {
            meta.is_foreign_key = true;
            meta.references_collection = fk.to_table;
            meta.references_column = fk.to_column;
            break;
        }
    }

    return meta;
}

VectorMetadata Database::get_vector_metadata(const std::string& collection, const std::string& group_name) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get vector metadata: no schema loaded");
    }

    // Find the vector table for this group
    auto vector_table = Schema::vector_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(vector_table);

    if (!table_def) {
        throw std::runtime_error("Vector group '" + group_name + "' not found for collection '" + collection + "'");
    }

    VectorMetadata meta;
    meta.group_name = group_name;

    // Add all data columns (skip id and vector_index)
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id" || col_name == "vector_index") {
            continue;
        }

        ScalarMetadata attr;
        attr.name = col.name;
        attr.data_type = col.type;
        attr.not_null = col.not_null;
        attr.primary_key = col.primary_key;
        attr.default_value = col.default_value;
        meta.value_columns.push_back(std::move(attr));
    }

    return meta;
}

SetMetadata Database::get_set_metadata(const std::string& collection, const std::string& group_name) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get set metadata: no schema loaded");
    }

    // Find the set table for this group
    auto set_table = Schema::set_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(set_table);

    if (!table_def) {
        throw std::runtime_error("Set group '" + group_name + "' not found for collection '" + collection + "'");
    }

    SetMetadata meta;
    meta.group_name = group_name;

    // Add all data columns (skip id)
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id") {
            continue;
        }

        ScalarMetadata attr;
        attr.name = col.name;
        attr.data_type = col.type;
        attr.not_null = col.not_null;
        attr.primary_key = col.primary_key;
        attr.default_value = col.default_value;
        meta.value_columns.push_back(std::move(attr));
    }

    return meta;
}

std::vector<ScalarMetadata> Database::list_scalar_attributes(const std::string& collection) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list scalar attributes: no schema loaded");
    }

    const auto* table_def = impl_->schema->get_table(collection);
    if (!table_def) {
        throw std::runtime_error("Collection not found in schema: " + collection);
    }

    std::vector<ScalarMetadata> result;
    for (const auto& [col_name, col] : table_def->columns) {
        ScalarMetadata meta;
        meta.name = col.name;
        meta.data_type = col.type;
        meta.not_null = col.not_null;
        meta.primary_key = col.primary_key;
        meta.default_value = col.default_value;

        for (const auto& fk : table_def->foreign_keys) {
            if (fk.from_column == col_name) {
                meta.is_foreign_key = true;
                meta.references_collection = fk.to_table;
                meta.references_column = fk.to_column;
                break;
            }
        }

        result.push_back(std::move(meta));
    }
    return result;
}

std::vector<VectorMetadata> Database::list_vector_groups(const std::string& collection) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list vector groups: no schema loaded");
    }

    std::vector<VectorMetadata> result;
    auto prefix = collection + "_vector_";

    for (const auto& table_name : impl_->schema->table_names()) {
        if (!impl_->schema->is_vector_table(table_name))
            continue;
        if (impl_->schema->get_parent_collection(table_name) != collection)
            continue;

        // Extract group name from table name
        if (table_name.size() > prefix.size() && table_name.substr(0, prefix.size()) == prefix) {
            auto group_name = table_name.substr(prefix.size());
            result.push_back(get_vector_metadata(collection, group_name));
        }
    }
    return result;
}

std::vector<SetMetadata> Database::list_set_groups(const std::string& collection) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list set groups: no schema loaded");
    }

    std::vector<SetMetadata> result;
    auto prefix = collection + "_set_";

    for (const auto& table_name : impl_->schema->table_names()) {
        if (!impl_->schema->is_set_table(table_name))
            continue;
        if (impl_->schema->get_parent_collection(table_name) != collection)
            continue;

        // Extract group name from table name
        if (table_name.size() > prefix.size() && table_name.substr(0, prefix.size()) == prefix) {
            auto group_name = table_name.substr(prefix.size());
            result.push_back(get_set_metadata(collection, group_name));
        }
    }
    return result;
}

std::vector<TimeSeriesMetadata> Database::list_time_series_groups(const std::string& collection) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot list time series groups: no schema loaded");
    }

    std::vector<TimeSeriesMetadata> result;
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

TimeSeriesMetadata Database::get_time_series_metadata(const std::string& collection,
                                                       const std::string& group_name) const {
    if (!impl_->schema) {
        throw std::runtime_error("Cannot get time series metadata: no schema loaded");
    }

    // Find the time series table for this group
    auto ts_table = Schema::time_series_table_name(collection, group_name);
    const auto* table_def = impl_->schema->get_table(ts_table);

    if (!table_def) {
        throw std::runtime_error("Time series group '" + group_name + "' not found for collection '" + collection + "'");
    }

    TimeSeriesMetadata meta;
    meta.group_name = group_name;

    // Add all data columns (skip id and date_time)
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name == "id" || col_name == "date_time") {
            continue;
        }

        ScalarMetadata attr;
        attr.name = col.name;
        attr.data_type = col.type;
        attr.not_null = col.not_null;
        attr.primary_key = col.primary_key;
        attr.default_value = col.default_value;
        meta.value_columns.push_back(std::move(attr));
    }

    return meta;
}

std::vector<std::map<std::string, Value>>
Database::read_time_series_group_by_id(const std::string& collection, const std::string& group, int64_t id) {
    impl_->require_schema("read time series");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);
    const auto* table_def = impl_->schema->get_table(ts_table);
    if (!table_def) {
        throw std::runtime_error("Time series table not found: " + ts_table);
    }

    // Build column list (excluding id)
    std::vector<std::string> columns;
    columns.push_back("date_time");
    for (const auto& [col_name, col] : table_def->columns) {
        if (col_name != "id" && col_name != "date_time") {
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
    sql += " FROM " + ts_table + " WHERE id = ? ORDER BY date_time";

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
    impl_->logger->debug(
        "Updating time series {}.{} for id {} with {} rows", collection, group, id, rows.size());
    impl_->require_schema("update time series");

    auto ts_table = impl_->schema->find_time_series_table(collection, group);

    Impl::TransactionGuard txn(*impl_);

    // Delete existing time series data for this element
    auto delete_sql = "DELETE FROM " + ts_table + " WHERE id = ?";
    execute(delete_sql, {id});

    if (rows.empty()) {
        txn.commit();
        return;
    }

    // Get column names from first row (excluding date_time which we handle specially)
    std::vector<std::string> value_columns;
    for (const auto& [col_name, _] : rows[0]) {
        if (col_name != "date_time") {
            value_columns.push_back(col_name);
        }
    }

    // Build INSERT SQL
    auto insert_sql = "INSERT INTO " + ts_table + " (id, date_time";
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

        // date_time must be present
        auto dt_it = row.find("date_time");
        if (dt_it == row.end()) {
            throw std::runtime_error("Time series row missing required 'date_time' column");
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
    impl_->logger->info(
        "Updated time series {}.{} for id {} with {} rows", collection, group, id, rows.size());
}

void Database::export_to_csv(const std::string& table, const std::string& path) {
    return;
}

void Database::import_from_csv(const std::string& table, const std::string& path) {
    return;
}

std::optional<std::string> Database::query_string(const std::string& sql, const std::vector<Value>& params) {
    auto result = execute(sql, params);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_string(0);
}

std::optional<int64_t> Database::query_integer(const std::string& sql, const std::vector<Value>& params) {
    auto result = execute(sql, params);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_integer(0);
}

std::optional<double> Database::query_float(const std::string& sql, const std::vector<Value>& params) {
    auto result = execute(sql, params);
    if (result.empty()) {
        return std::nullopt;
    }
    return result[0].get_float(0);
}

void Database::describe() const {
    std::cout << "Database: " << impl_->path << "\n";
    std::cout << "Version: " << current_version() << "\n";

    if (!impl_->schema) {
        std::cout << "\nNo schema loaded.\n";
        return;
    }

    for (const auto& collection : impl_->schema->collection_names()) {
        std::cout << "\nCollection: " << collection << "\n";

        // Scalars
        const auto* table_def = impl_->schema->get_table(collection);
        if (table_def && !table_def->columns.empty()) {
            std::cout << "  Scalars:\n";
            for (const auto& [name, col] : table_def->columns) {
                std::cout << "    - " << name << " (" << data_type_to_string(col.type) << ")";
                if (col.primary_key) {
                    std::cout << " PRIMARY KEY";
                }
                if (col.not_null && !col.primary_key) {
                    std::cout << " NOT NULL";
                }
                std::cout << "\n";
            }
        }

        // Vectors
        auto prefix_vec = collection + "_vector_";
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_vector_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            auto group_name = table_name.substr(prefix_vec.size());
            const auto* vec_table = impl_->schema->get_table(table_name);
            if (!vec_table)
                continue;

            std::cout << "  Vectors:\n";
            std::cout << "    - " << group_name << ": ";
            bool first = true;
            for (const auto& [col_name, col] : vec_table->columns) {
                if (col_name == "id" || col_name == "vector_index")
                    continue;
                if (!first)
                    std::cout << ", ";
                std::cout << col_name << "(" << data_type_to_string(col.type) << ")";
                first = false;
            }
            std::cout << "\n";
        }

        // Sets
        auto prefix_set = collection + "_set_";
        for (const auto& table_name : impl_->schema->table_names()) {
            if (!impl_->schema->is_set_table(table_name))
                continue;
            if (impl_->schema->get_parent_collection(table_name) != collection)
                continue;

            auto group_name = table_name.substr(prefix_set.size());
            const auto* set_table = impl_->schema->get_table(table_name);
            if (!set_table)
                continue;

            std::cout << "  Sets:\n";
            std::cout << "    - " << group_name << ": ";
            bool first = true;
            for (const auto& [col_name, col] : set_table->columns) {
                if (col_name == "id")
                    continue;
                if (!first)
                    std::cout << ", ";
                std::cout << col_name << "(" << data_type_to_string(col.type) << ")";
                first = false;
            }
            std::cout << "\n";
        }
    }
}

}  // namespace quiver
